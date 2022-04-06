#ifndef PROFILEWINDOW_H
#define PROFILEWINDOW_H

#include <QDockWidget>

class TargetModel;
class Dispatcher;
class Session;
class QLabel;
class QPushButton;

class ProfileWindow : public QDockWidget
{
    Q_OBJECT
public:
    ProfileWindow(QWidget *parent, Session* pSession);
    virtual ~ProfileWindow();

    // Grab focus and point to the main widget
    void keyFocus();

    void loadSettings();
    void saveSettings();

private slots:
    void connectChangedSlot();
    void startStopChangedSlot();
    void profileChangedSlot();
    void settingsChangedSlot();

    void startStopClicked();
    void resetClicked();

private:

    Session*            m_pSession;
    TargetModel*        m_pTargetModel;
    Dispatcher*         m_pDispatcher;

    QPushButton*        m_pStartStopButton;
    QPushButton*        m_pResetButton;
};

#endif // PROFILEWINDOW_H
