#ifndef PROCESSLISTDIALOG_H
#define PROCESSLISTDIALOG_H

#include <QDialog>
#include <QVector>

struct ProcEntry {
    QString name;
    qint64 pid{};
};

class QTableWidget;

class ProcessListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ProcessListDialog(QWidget* parent=nullptr);
    ~ProcessListDialog() override = default;

    ProcEntry selected() const { return selected_; }

private:
    void populate();
    void onAccept();
    void onActivated(int row, int /*col*/);

    QTableWidget* table_{};
    ProcEntry selected_{};
};

#endif // PROCESSLISTDIALOG_H
