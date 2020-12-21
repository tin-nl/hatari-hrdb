#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/*
	Main GUI Window.

	Currently everything is controlled and routed through here.
*/

#include <QMainWindow>
#include "targetmodel.h"
#include "disassembler.h"

class QPushButton;
class QLabel;
class QTcpSocket;
class QTextEdit;
class QActionGroup;

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
    void registersChangedSlot(uint64_t commandId);
    void memoryChangedSlot(int slot, uint64_t commandId);
    void symbolTableChangedSlot(uint64_t commandId);
    void startStopDelayedSlot(int running);

    void startStopClicked();
    void singleStepClicked();
    void nextClicked();
private slots:
    void newFile();
    void menuConnect();
    void menuDisconnect();
    void menuDisasmWindow();
    void menuMemoryWindow();
    void about();
    void aboutQt();
private:
    // Network
    void Connect();
    void Disconnect();

	// Populaters
	void PopulateRegisters();
    QString FindSymbol(uint32_t addr);
    void PopulateRunningSquare();

    // Our UI widgets
    QPushButton*	m_pStartStopButton;
    QPushButton*	m_pSingleStepButton;
	QTextEdit*		m_pRegistersTextEdit;
    QWidget*        m_pRunningSquare;

    // Docking windows
    DisasmWidget*       m_pDisasmWidget;
    MemoryViewWidget*   m_pMemoryViewWidget;

    // Low-level data
	QTcpSocket* 	tcpSocket;
    Dispatcher*		m_pDispatcher;
	TargetModel*	m_pTargetModel;

    // Shown data
    Registers                      m_prevRegs;
    Disassembler::disassembly      m_disasm;

    // Menus
    void createActions();
    void createMenus();
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *windowMenu;
    QMenu *helpMenu;

    QAction *connectAct;
    QAction *disconnectAct;

    QAction *disasmWindowAct;
    QAction *memoryWindowAct;
    QAction *aboutAct;
    QAction *aboutQtAct;
};
#endif // MAINWINDOW_H
