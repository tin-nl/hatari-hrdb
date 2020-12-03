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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
	void startStopChangedSlot();
	void registersChangedSlot();
	void memoryChangedSlot();

    void startStopClicked();
    void singleStepClicked();
private:

	// Populaters
	void PopulateRegisters();
	void PopulateMemory();

    QPushButton*	m_pStartStopButton;
    QPushButton*	m_pSingleStepButton;
	QTextEdit*		m_pRegistersTextEdit;
	QTextEdit*		m_pMemoryTextEdit;

	QTcpSocket* 	tcpSocket;
    Dispatcher*		m_pDispatcher;
	TargetModel*	m_pTargetModel;

    Registers       m_prevRegs;
};
#endif // MAINWINDOW_H
