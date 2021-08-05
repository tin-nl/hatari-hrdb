#ifndef RUNDIALOG_H
#define RUNDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class TargetModel;
class Dispatcher;
class Session;

class RunDialog : public QDialog
{
   // Q_OBJECT
public:
    RunDialog(QWidget* parent, Session* pSession);
    virtual ~RunDialog();

    // Settings
    void loadSettings();
    void saveSettings();

protected:
    virtual void showEvent(QShowEvent *event);
    virtual void closeEvent(QCloseEvent *event);

private slots:
    void okClicked();
    void exeClicked();
    void workingDirectoryClicked();

private:
    // UI elements
    QLineEdit*      m_pExecutableTextEdit;
    QLineEdit*      m_pArgsTextEdit;
    QLineEdit*      m_pWorkingDirectoryTextEdit;
    QComboBox*      m_pBreakModeCombo;

    // What sort of automatic breakpoint to use
    enum BreakMode
    {
        kNone,
        kBoot,
        kProgStart
    };

    // Shared session data pointer (storage for launched process, temp file etc)
    Session*        m_pSession;
};

#endif // RUNDIALOG_H
