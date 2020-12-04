#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/*
	Main GUI Window.

	Currently everything is controlled and routed through here.
*/

#include <QMainWindow>
#include "targetmodel.h"

class QPushButton;
class QLabel;
class QTcpSocket;
class QTextEdit;

class Dispatcher;
class TargetModel;

class DisasmWidget;
class MemoryViewWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void connectChangedSlot();
	void startStopChangedSlot();
	void registersChangedSlot();

    void startStopClicked();
    void singleStepClicked();
private:

	// Populaters
	void PopulateRegisters();

    QPushButton*	m_pStartStopButton;
    QPushButton*	m_pSingleStepButton;
	QTextEdit*		m_pRegistersTextEdit;

    DisasmWidget*   m_pDisasmWindow;
    MemoryViewWidget* m_pMemoryViewWidget;

	QTcpSocket* 	tcpSocket;
    Dispatcher*		m_pDispatcher;
	TargetModel*	m_pTargetModel;

    Registers       m_prevRegs;
};
#endif // MAINWINDOW_H
