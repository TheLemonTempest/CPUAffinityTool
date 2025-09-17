#ifndef CPUAffinity_H
#define CPUAffinity_H

#include <QMainWindow>
#include <QPointer>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
namespace Ui { class CPUAffinity; }
class QSpinBox;
QT_END_NAMESPACE

struct AffinityConfig {
    QString processName;
    qint64  pid{0};
    int     assignedCores{0};
};

class CPUAffinity : public QMainWindow
{
    Q_OBJECT
public:
    explicit CPUAffinity(QWidget *parent = nullptr);
    ~CPUAffinity();

private slots:
    // Menus
    void onActionSelectProcess();
    void onActionSave();
    void onActionSaveAs();
    void onActionLoad();
    void onActionCheckForNewVersion();
    void onActionAbout();

    // Row 1 buttons
    void onButtonSelectProcess();
    void onButtonLoadConfig();
    void onButtonSaveConfig();
    void onButtonSaveConfigAs();
    void onButtonApply();   // Apply all current editor settings

private:
    Ui::CPUAffinity *ui;

    // State
    AffinityConfig cfg_;
    QString currentConfigPath_; // empty = not saved yet

    // Helpers
    void connectUi();
    void refreshUiProcessLabel();
    void pullEditorsIntoConfig();   // read values from Frame Two
    void pushConfigIntoEditors();   // write values into Frame Two
    void updateProcessInfoView();
    QSpinBox* findSpinUnassign() const;
    static int totalLogicalProcessors();

    // Config I/O
    static QJsonObject toJson(const AffinityConfig& c);
    static AffinityConfig fromJson(const QJsonObject& o, bool* ok=nullptr);
    bool saveConfigTo(const QString& path);
    bool loadConfigFrom(const QString& path);

    // File dialogs
    QString dialogSavePath();
    QString dialogLoadPath();
};

#endif // CPUAffinity_H
