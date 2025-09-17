#include "processlistdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QProcess>
#include <QTextStream>
#include <QRegularExpression>

ProcessListDialog::ProcessListDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Select Process");
    resize(600, 400);

    auto* v = new QVBoxLayout(this);
    table_ = new QTableWidget(this);
    table_->setColumnCount(2);
    table_->setHorizontalHeaderLabels({"Name", "PID"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->addWidget(table_);

    auto* h = new QHBoxLayout();
    h->addStretch();
    auto* btnOk = new QPushButton("OK", this);
    auto* btnCancel = new QPushButton("Cancel", this);
    h->addWidget(btnOk);
    h->addWidget(btnCancel);
    v->addLayout(h);

    connect(btnOk, &QPushButton::clicked, this, &ProcessListDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &ProcessListDialog::reject);
    connect(table_, &QTableWidget::cellDoubleClicked, this, &ProcessListDialog::onActivated);

    populate();
}

void ProcessListDialog::populate()
{
#ifdef Q_OS_WINDOWS
    // PowerShell: only processes with a window (non-background)
    QString cmd =
        "Get-Process | Where-Object { $_.MainWindowTitle -ne \"\" } "
        "| Select-Object -Property ProcessName,Id,MainWindowTitle "
        "| Sort-Object -Property ProcessName,Id "
        "| ConvertTo-Csv -NoTypeInformation";

    QProcess p;
    p.start("powershell", {"-NoLogo","-NoProfile","-Command", cmd});
    p.waitForFinished(3000);
    const QString csv = QString::fromLocal8Bit(p.readAllStandardOutput());

    // Split lines (use static regex to avoid temp objects each call)
    static const QRegularExpression lineBreakRe("[\r\n]");
    QStringList lines = csv.split(lineBreakRe, Qt::SkipEmptyParts);
    if (lines.size() < 2) return; // no data
    lines.removeFirst(); // remove header

    // Setup table for 3 columns: Name, PID, Window Title
    table_->setColumnCount(3);
    table_->setHorizontalHeaderLabels({"Name", "PID", "Window Title"});
    table_->setRowCount(lines.size());

    int r = 0;
    for (const QString& line : lines) {
        QString s = line.trimmed();
        if (!s.startsWith('"')) continue;

        // Naive CSV split (since we know fields are quoted)
        QStringList cols;
        QString tmp;
        bool inQuotes = false;
        for (QChar c : s) {
            if (c == '"') { inQuotes = !inQuotes; continue; }
            if (c == ',' && !inQuotes) { cols << tmp; tmp.clear(); continue; }
            tmp.append(c);
        }
        cols << tmp;

        if (cols.size() < 3) continue;
        QString name = cols[0].trimmed();
        QString pidStr = cols[1].trimmed();
        QString windowTitle = cols[2].trimmed();

        bool ok = false;
        qint64 pid = pidStr.toLongLong(&ok);
        if (!ok) continue;

        // Insert into table
        table_->setItem(r, 0, new QTableWidgetItem(name));

        auto* pidItem = new QTableWidgetItem(QString::number(pid));
        pidItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(r, 1, pidItem);

        table_->setItem(r, 2, new QTableWidgetItem(windowTitle));
        ++r;
    }

    table_->setRowCount(r); // in case of skips
    table_->resizeColumnsToContents();
#else
    table_->setRowCount(0);
#endif
}

void ProcessListDialog::onActivated(int row, int /*col*/)
{
    if (row < 0) return;
    selected_.name = table_->item(row, 0)->text();
    selected_.pid  = table_->item(row, 1)->text().toLongLong();
    accept();
}

void ProcessListDialog::onAccept()
{
    int row = table_->currentRow();
    if (row < 0 && table_->rowCount() > 0) row = 0;
    onActivated(row, 0);
}
