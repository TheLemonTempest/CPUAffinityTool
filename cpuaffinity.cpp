#include "cpuaffinity.h"
#include "./ui_CPUAffinity.h"
#include "processlistdialog.h"

#include <QFileDialog>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QLabel>
#include <QSpinBox>
#include <QProcess>
#include <QStandardItemModel>
#include <QRegularExpression>
#include <QDateTime>
#include <QTime>
#include <cmath>
#include <QThread>

CPUAffinity::CPUAffinity(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CPUAffinity)
{
    ui->setupUi(this);
    connectUi();

    // Defaults
    cfg_.processName.clear();
    cfg_.pid = 0;
    cfg_.assignedCores = 1;

    refreshUiProcessLabel();
    pushConfigIntoEditors();
}

CPUAffinity::~CPUAffinity()
{
    delete ui;
}

int CPUAffinity::totalLogicalProcessors()
{
    int n = QThread::idealThreadCount();
    if (n > 0)
        return n;

#ifdef Q_OS_WINDOWS
    // Fallback: ask PowerShell
    QProcess p;
    p.start("powershell", {
                              "-NoLogo", "-NoProfile", "-Command",
                              "(Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors"
                          });
    if (p.waitForFinished(1500)) {
        bool ok = false;
        int val = QString::fromLocal8Bit(p.readAllStandardOutput())
                      .trimmed()
                      .toInt(&ok);
        if (ok && val > 0)
            return val;
    }
#endif
    return 4; // conservative fallback
}

void CPUAffinity::connectUi()
{
    // Menus
    connect(ui->actionSelectProcess,     &QAction::triggered, this, &CPUAffinity::onActionSelectProcess);
    connect(ui->actionSave,              &QAction::triggered, this, &CPUAffinity::onActionSave);
    connect(ui->actionSaveAs,            &QAction::triggered, this, &CPUAffinity::onActionSaveAs);
    connect(ui->actionLoad,              &QAction::triggered, this, &CPUAffinity::onActionLoad);
    connect(ui->actionCheckForNewVersion,&QAction::triggered, this, &CPUAffinity::onActionCheckForNewVersion);
    connect(ui->actionAbout,             &QAction::triggered, this, &CPUAffinity::onActionAbout);
    connect(ui->actionQuit,              &QAction::triggered, this, &CPUAffinity::close);

    // Top Left buttons
    connect(ui->btnSelectProcess, &QPushButton::clicked, this, &CPUAffinity::onButtonSelectProcess);
    if (auto* b = this->findChild<QPushButton*>("btnLoadConfig"))
        connect(b, &QPushButton::clicked, this, &CPUAffinity::onButtonLoadConfig);
    if (auto* b = this->findChild<QPushButton*>("btnSaveConfig"))
        connect(b, &QPushButton::clicked, this, &CPUAffinity::onButtonSaveConfig);

    // Bottom Right buttons
    if (auto* b = this->findChild<QPushButton*>("btnApply"))
        connect(b, &QPushButton::clicked, this, &CPUAffinity::onButtonApply);
    if (auto* b = this->findChild<QPushButton*>("btnLoadConfig_2"))
        connect(b, &QPushButton::clicked, this, &CPUAffinity::onButtonLoadConfig);
    if (auto* b = this->findChild<QPushButton*>("btnSaveConfig_2"))
        connect(b, &QPushButton::clicked, this, &CPUAffinity::onButtonSaveConfig);
    if (auto* b = this->findChild<QPushButton*>("btnSaveConfigAs"))
        connect(b, &QPushButton::clicked, this, &CPUAffinity::onButtonSaveConfigAs);
}

void CPUAffinity::refreshUiProcessLabel()
{
    QString text;
    if (cfg_.processName.isEmpty()) {
        text = "No process selected";
    } else {
        text = QString("Current Process: %1 (PID %2)")
        .arg(cfg_.processName)
            .arg(cfg_.pid ? QString::number(cfg_.pid) : QStringLiteral("—"));
    }
    if (ui->labelCurrentProcess)
        ui->labelCurrentProcess->setText(text);
}

QSpinBox* CPUAffinity::findSpinUnassign() const
{
    // We expect a QSpinBox in Frame Two with objectName "spinUnassign"
    if (auto* s = this->findChild<QSpinBox*>("spinUnassign"))
        return s;
    return nullptr;
}

void CPUAffinity::pullEditorsIntoConfig()
{
    if (auto* s = findChild<QSpinBox*>("spinBoxAssignedCores"))
        cfg_.assignedCores = s->value();
}

void CPUAffinity::pushConfigIntoEditors()
{
    if (auto* s = findChild<QSpinBox*>("spinBoxAssignedCores")) {
        s->setMaximum(totalLogicalProcessors());
        s->setValue(cfg_.assignedCores > 0 ? cfg_.assignedCores : 1);
    }
}

void CPUAffinity::updateProcessInfoView()
{
    auto* listView = this->findChild<QListView*>("processInfoListView");
    if (!listView) return;

    auto* model = new QStandardItemModel(listView);

#ifdef Q_OS_WINDOWS
    if (cfg_.pid <= 0) {
        model->appendRow(new QStandardItem("No process selected."));
        listView->setModel(model);
        return;
    }

    // PS script: get process by PID, fill in path via WMI if needed, then emit a single JSON object.
    const QString ps = QString(
                           "$p=Get-Process -Id %1 -ErrorAction SilentlyContinue;"
                           "if(-not $p){ exit 2 };"
                           "$wp=Get-CimInstance Win32_Process -Filter \"ProcessId=%1\" -ErrorAction SilentlyContinue;"
                           "$path=if($p.Path){$p.Path}else{$wp.ExecutablePath};"
                           "$total=(Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors;"
                           "$mask=$p.ProcessorAffinity;"
                           "$assigned=0;"
                           "for($i=0;$i -lt $total;$i++){ if($mask -band (1 -shl $i)){ $assigned++ } };"
                           "$obj=[PSCustomObject]@{"
                           " ProcessName=$p.ProcessName;"
                           " Id=$p.Id;"
                           " MainWindowTitle=$p.MainWindowTitle;"
                           " Path=$path;"
                           " StartTime=$p.StartTime;"
                           " CPU=$p.CPU;"
                           " WorkingSet64=$p.WorkingSet64;"
                           " PrivateMemorySize64=$p.PrivateMemorySize64;"
                           " PagedMemorySize64=$p.PagedMemorySize64;"
                           " Threads=$p.Threads.Count;"
                           " Handles=$p.Handles;"
                           " Responding=$p.Responding;"
                           " AssignedCores=$assigned;"
                           " TotalCores=$total"
                           "};"
                           "$obj | ConvertTo-Json -Depth 3"
                           ).arg(cfg_.pid);

    QProcess p;
    p.start("powershell", {"-NoLogo","-NoProfile","-Command", ps});
    if (!p.waitForFinished(3000)) {
        model->appendRow(new QStandardItem("Timed out reading process info."));
        listView->setModel(model);
        return;
    }

    const QByteArray out = p.readAllStandardOutput();
    QJsonParseError jerr{};
    const QJsonDocument doc = QJsonDocument::fromJson(out, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
        model->appendRow(new QStandardItem("Failed to parse process info."));
        listView->setModel(model);
        return;
    }

    const QJsonObject o = doc.object();

    if (o.contains("AssignedCores")) {
        cfg_.assignedCores = o.value("AssignedCores").toInt(0);

        if (auto* s = findChild<QSpinBox*>("spinBoxAssignedCores")) {
            int total = o.value("TotalCores").toInt(totalLogicalProcessors());
            s->setMaximum(total);
            s->setValue(cfg_.assignedCores > 0 ? cfg_.assignedCores : 1);
        }
    }

    auto addKV = [&](const QString& label, const QString& value) {
        model->appendRow(new QStandardItem(label + ": " + (value.isEmpty() ? "—" : value)));
    };
    auto addKVVar = [&](const QString& label, const QJsonValue& v) {
        if (v.isBool()) addKV(label, v.toBool() ? "Yes" : "No");
        else addKV(label, v.isNull() ? QString() : v.toVariant().toString());
    };
    auto fmtBytesMB = [](qint64 bytes) -> QString {
        if (bytes <= 0) return "0 MB";
        const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        return QString::number(mb, 'f', 1) + " MB";
    };

    addKV("Name", o.value("ProcessName").toString());
    addKVVar("PID", o.value("Id"));
    addKV("Window Title", o.value("MainWindowTitle").toString());
    addKV("Path", o.value("Path").toString());

    // Start time → local, pretty
    QString startIso = o.value("StartTime").toString();
    if (!startIso.isEmpty()) {
        QDateTime dt;

        static const QRegularExpression msEpochRe("^/Date\\((\\d+)\\)/$");
        auto m = msEpochRe.match(startIso);
        if (m.hasMatch()) {
            qint64 msSinceEpoch = m.captured(1).toLongLong();
            dt = QDateTime::fromMSecsSinceEpoch(msSinceEpoch, QTimeZone::UTC);
        } else {
            dt = QDateTime::fromString(startIso, Qt::ISODateWithMs);
            if (!dt.isValid())
                dt = QDateTime::fromString(startIso, Qt::ISODate);
        }

        if (dt.isValid()) {
            addKV("Start Time", QLocale().toString(dt.toLocalTime(), QLocale::ShortFormat));
        } else {
            addKV("Start Time", startIso);
        }
    } else {
        addKV("Start Time", QString());
    }

    // CPU seconds → hh:mm:ss
    const double cpuSecs = o.value("CPU").toDouble(0.0);
    const int secs = static_cast<int>(std::round(cpuSecs));
    addKV("CPU Time", QTime(0,0).addSecs(qMax(0,secs)).toString("hh:mm:ss"));

    // Memory
    const qint64 ws  = static_cast<qint64>(o.value("WorkingSet64").toDouble(0.0));
    const qint64 pvt = static_cast<qint64>(o.value("PrivateMemorySize64").toDouble(0.0));
    const qint64 pgd = static_cast<qint64>(o.value("PagedMemorySize64").toDouble(0.0));
    addKV("Working Set", fmtBytesMB(ws));
    addKV("Private Memory", fmtBytesMB(pvt));
    addKV("Paged Memory", fmtBytesMB(pgd));

    addKVVar("Threads", o.value("Threads"));
    addKVVar("Handles", o.value("Handles"));
    addKVVar("Responding", o.value("Responding"));
#else
    model->appendRow(new QStandardItem("Process info only works on Windows."));
#endif

    listView->setModel(model);
}

void CPUAffinity::onActionSelectProcess()
{
    onButtonSelectProcess();
}

void CPUAffinity::onButtonSelectProcess()
{
#ifdef Q_OS_WINDOWS
    ProcessListDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto sel = dlg.selected();
        if (!sel.name.isEmpty()) {
            cfg_.processName = sel.name;
            cfg_.pid = sel.pid;
            refreshUiProcessLabel();
            updateProcessInfoView();   // refresh the ListView
        }
    }
#else
    QMessageBox::information(this, "Unavailable", "Process selection is Windows-only.");
#endif
}


void CPUAffinity::onActionSave()
{
    pullEditorsIntoConfig();

    if (currentConfigPath_.isEmpty())
        currentConfigPath_ = dialogSavePath();

    if (currentConfigPath_.isEmpty())
        return;

    if (!saveConfigTo(currentConfigPath_)) {
        QMessageBox::warning(this, "Save failed", "Could not save the configuration.");
        currentConfigPath_.clear();
    } else {
        statusBar()->showMessage("Saved: " + currentConfigPath_, 2000);
    }
}

void CPUAffinity::onButtonSaveConfig()
{
    onActionSave();
}

void CPUAffinity::onActionSaveAs()
{
    pullEditorsIntoConfig();

    const QString path = dialogSavePath();
    if (path.isEmpty()) return;

    if (saveConfigTo(path)) {
        currentConfigPath_ = path;
        statusBar()->showMessage("Saved: " + currentConfigPath_, 2000);
    } else {
        QMessageBox::warning(this, "Save As failed", "Could not save the configuration.");
    }
}

void CPUAffinity::onButtonSaveConfigAs()
{
    onActionSaveAs();
}

void CPUAffinity::onActionLoad()
{
    const QString path = dialogLoadPath();
    if (path.isEmpty()) return;

    if (loadConfigFrom(path)) {
        currentConfigPath_ = path;
        pushConfigIntoEditors();
        refreshUiProcessLabel();
        statusBar()->showMessage("Loaded: " + currentConfigPath_, 2000);
    } else {
        QMessageBox::warning(this, "Load failed", "Could not load the configuration.");
    }
}

void CPUAffinity::onButtonLoadConfig()
{
    onActionLoad();
}

void CPUAffinity::onButtonApply()
{
#ifdef Q_OS_WINDOWS
    if (cfg_.processName.isEmpty() || cfg_.pid <= 0) {
        QMessageBox::warning(this, "No process selected",
                             "Please select a process first.");
        return;
    }

    pullEditorsIntoConfig(); // sync UI → cfg_

    int coresToAssign = cfg_.assignedCores;
    if (coresToAssign < 1) coresToAssign = 1;
    if (coresToAssign > totalLogicalProcessors())
        coresToAssign = totalLogicalProcessors();

    QString psCommand = QString(
                            "$total=(Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors; "
                            "$assign=%1; "
                            "if($assign -gt $total){ $assign=$total }; "
                            "$mask=0; "
                            "$sel=Get-Random -Count $assign -InputObject (0..($total-1)); "
                            "foreach($i in $sel){ $mask=$mask -bor (1 -shl $i) }; "
                            "(Get-Process -Id %2 -ErrorAction SilentlyContinue) | "
                            "ForEach-Object { $_.ProcessorAffinity=$mask; "
                            " Write-Output (\"Affinity for {0} (PID {1}) set to 0x{2:X}\" -f $_.ProcessName, $_.Id, $mask) }"
                            ).arg(coresToAssign).arg(cfg_.pid);

    QProcess* ps = new QProcess(this);
    connect(ps, &QProcess::readyReadStandardOutput, this, [this, ps]() {
        auto text = QString::fromLocal8Bit(ps->readAllStandardOutput()).trimmed();
        if (!text.isEmpty())
            this->statusBar()->showMessage(text, 5000);
    });
    connect(ps, &QProcess::readyReadStandardError, this, [this, ps]() {
        auto text = QString::fromLocal8Bit(ps->readAllStandardError()).trimmed();
        if (!text.isEmpty())
            QMessageBox::warning(this, "PowerShell Error", text);
    });
    connect(ps, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), ps, &QObject::deleteLater);

    ps->start("powershell",
              {"-NoLogo","-NoProfile","-WindowStyle","Hidden","-ExecutionPolicy","Bypass",
               "-Command", psCommand});
#else
    QMessageBox::information(this, "Unsupported", "Affinity setting only works on Windows.");
#endif
}

void CPUAffinity::onActionCheckForNewVersion()
{
    // Placeholder: just inform the user for now
    QMessageBox::information(this, "Check for new version",
                             "Update check is not implemented yet.");
}

void CPUAffinity::onActionAbout()
{
    QMessageBox::about(this, "About CPU Affinity",
                       "CPU Affinity\n\n"
                       "Authors: Brendon Banville & Ashley P.\n"
                       "Version: 0.1.0\n"
                       "Last Updated: 2025-09-16\n\n"
                       "This utility lets you inspect a process and edit process settings,\n"
                       "so you can make sure they aren't hogging all the cores.");
}

// ---------- Config I/O ----------

QJsonObject CPUAffinity::toJson(const AffinityConfig& c)
{
    QJsonObject o;
    o["processName"]   = c.processName;
    o["pid"]           = QString::number(c.pid);
    o["assignedCores"] = c.assignedCores;
    return o;
}

AffinityConfig CPUAffinity::fromJson(const QJsonObject& o, bool* ok)
{
    AffinityConfig c;
    c.processName   = o.value("processName").toString();
    c.pid           = o.value("pid").toString().toLongLong();
    c.assignedCores = o.value("assignedCores").toInt(0);
    if (c.assignedCores < 1) c.assignedCores = 1;
    if (ok) *ok = true;
    return c;
}

bool CPUAffinity::saveConfigTo(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const QJsonDocument doc(toJson(cfg_));
    f.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool CPUAffinity::loadConfigFrom(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;
    bool ok=false;
    cfg_ = fromJson(doc.object(), &ok);
    return ok;
}

QString CPUAffinity::dialogSavePath()
{
    return QFileDialog::getSaveFileName(this, "Save Config",
                                        QString(), "Affinity Config (*.affinity.json);;JSON (*.json);;All Files (*.*)");
}

QString CPUAffinity::dialogLoadPath()
{
    return QFileDialog::getOpenFileName(this, "Load Config",
                                        QString(), "Affinity Config (*.affinity.json *.json);;All Files (*.*)");
}
