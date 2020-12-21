#include "mainwindow.h"

#include <iostream>
#include <QtWidgets>
#include <QtNetwork>
#include <QShortcut>
#include <QFontDatabase>

#include "dispatcher.h"
#include "targetmodel.h"

#include "disasmwidget.h"
#include "memoryviewwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tcpSocket(new QTcpSocket(this))
{
    // Create the core data models, since other object want to connect to them.
    m_pTargetModel = new TargetModel();
    m_pDispatcher = new Dispatcher(tcpSocket, m_pTargetModel);

    // Top row of buttons
    m_pRunningSquare = new QWidget(this);
    m_pRunningSquare->setFixedSize(10, 25);
    m_pRunningSquare->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    m_pStartStopButton = new QPushButton("Break", this);
    m_pSingleStepButton = new QPushButton("Step", this);

    // Register/status window
    m_pRegistersTextEdit = new QTextEdit("", this);
	m_pRegistersTextEdit->setReadOnly(true);
	m_pRegistersTextEdit->setAcceptRichText(false);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_pRegistersTextEdit->setCurrentFont(monoFont);
    m_pRegistersTextEdit->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);
    m_pRegistersTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    m_pDisasmWidget0 = new DisasmWidget(this, m_pTargetModel, m_pDispatcher, 0);
    m_pDisasmWidget1 = new DisasmWidget(this, m_pTargetModel, m_pDispatcher, 1);
    m_pMemoryViewWidget0 = new MemoryViewWidget(this, m_pTargetModel, m_pDispatcher, 0);
    m_pMemoryViewWidget1 = new MemoryViewWidget(this, m_pTargetModel, m_pDispatcher, 1);

    // https://doc.qt.io/qt-5/qtwidgets-layouts-basiclayouts-example.html
    QVBoxLayout *vlayout = new QVBoxLayout;
    QHBoxLayout *hlayout = new QHBoxLayout;
    auto pTopGroupBox = new QWidget(this);
    auto pMainGroupBox = new QGroupBox(this);

    hlayout->addWidget(m_pRunningSquare);
    hlayout->addWidget(m_pStartStopButton);
    hlayout->addWidget(m_pSingleStepButton);
    pTopGroupBox->setLayout(hlayout);

    vlayout->addWidget(pTopGroupBox);
    vlayout->addWidget(m_pRegistersTextEdit);
    vlayout->setAlignment(Qt::Alignment(Qt::AlignTop));
    pMainGroupBox->setFlat(true);
    pMainGroupBox->setLayout(vlayout);

    setCentralWidget(pMainGroupBox);

    this->addDockWidget(Qt::BottomDockWidgetArea, m_pDisasmWidget0);
    this->addDockWidget(Qt::RightDockWidgetArea, m_pMemoryViewWidget0);
    this->addDockWidget(Qt::BottomDockWidgetArea, m_pMemoryViewWidget1);
    this->addDockWidget(Qt::BottomDockWidgetArea, m_pDisasmWidget1);
    m_pMemoryViewWidget1->hide();
    m_pDisasmWidget1->hide();

    // Set up menus
    createActions();
    createMenus();

	// Listen for target changes
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &MainWindow::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::registersChangedSignal, this, &MainWindow::registersChangedSlot);
    connect(m_pTargetModel, &TargetModel::connectChangedSignal,   this, &MainWindow::connectChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,    this, &MainWindow::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::symbolTableChangedSignal,this, &MainWindow::symbolTableChangedSlot);
    connect(m_pTargetModel, &TargetModel::startStopChangedSignalDelayed,this, &MainWindow::startStopDelayedSlot);

	// Wire up buttons to actions
    connect(m_pStartStopButton, &QAbstractButton::clicked, this, &MainWindow::startStopClicked);
    connect(m_pSingleStepButton, &QAbstractButton::clicked, this, &MainWindow::singleStepClicked);

    // Wire up menu appearance
    connect(windowMenu, &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

	// Keyboard shortcuts
    new QShortcut(QKeySequence(tr("F5", "Start/Stop")), 		this, 			SLOT(startStopClicked()));

    new QShortcut(QKeySequence(tr("F10", "Next")),				this,           SLOT(nextClicked()));
    new QShortcut(QKeySequence(tr("F11", "Step")),              this,           SLOT(singleStepClicked()));

    // Try initial connect
    Connect();

    // Update everything
    connectChangedSlot();
    startStopChangedSlot();
}

MainWindow::~MainWindow()
{
	delete m_pDispatcher;
    delete m_pTargetModel;
}

void MainWindow::connectChangedSlot()
{
    bool isConnect = m_pTargetModel->IsConnected() ? true : false;
    m_pStartStopButton->setEnabled(isConnect);
    m_pSingleStepButton->setEnabled(isConnect);
    m_pRegistersTextEdit->setEnabled(isConnect);

    PopulateRunningSquare();
    PopulateRegisters();
}

void MainWindow::startStopChangedSlot()
{
	// Update text here
	if (m_pTargetModel->IsRunning())
	{       
        // Update our previous values
        // TODO: this is not the ideal way to do this, since there
        // are timing issues. In future, parcel up the stopping updates so
        // that widget refreshes happen as one.
        m_prevRegs = m_pTargetModel->GetRegs();

        m_pStartStopButton->setText("Break");
        m_pSingleStepButton->setEnabled(false);
    }
	else
	{
        // STOPPED
		// TODO this is where all windows should put in requests for data
        m_pDispatcher->SendCommandPacket("regs");
        m_pDispatcher->SendCommandPacket("bplist");
        m_pDispatcher->RequestMemory(MemorySlot::kMainPC, m_pTargetModel->GetPC(), 10);

        // Only re-request symbols if we didn't find any the first time
        if (m_pTargetModel->GetSymbolTable().m_userSymbolCount == 0)  // NO CHECK
            m_pDispatcher->SendCommandPacket("symlist");

        m_pStartStopButton->setText("Run");
		m_pSingleStepButton->setEnabled(true);	
        m_pRegistersTextEdit->setEnabled(true);
    }
    PopulateRunningSquare();
    PopulateRegisters();
}

void MainWindow::startStopDelayedSlot(int running)
{
    if (running)
    {
        m_pRegistersTextEdit->setEnabled(false);
        m_pRegistersTextEdit->setText("Running, F5 to break...");
    }
}

void MainWindow::registersChangedSlot(uint64_t commandId)
{
	// Update text here
    PopulateRegisters();
}

void MainWindow::memoryChangedSlot(int slot, uint64_t commandId)
{
    if (slot != MemorySlot::kMainPC)
        return;

    // Disassemble the first instruction
    m_disasm.lines.clear();
    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kMainPC);
    if (!pMem)
        return;

    // Fetch underlying data, this is picked up by the model class
    buffer_reader disasmBuf(pMem->GetData(), pMem->GetSize());
    Disassembler::decode_buf(disasmBuf, m_disasm, pMem->GetAddress(), 2);
    PopulateRegisters();
}

void MainWindow::symbolTableChangedSlot(uint64_t commandId)
{
    PopulateRegisters();
}

void MainWindow::startStopClicked()
{
	if (m_pTargetModel->IsRunning())
        m_pDispatcher->SendCommandPacket("break");
	else
        m_pDispatcher->SendCommandPacket("run");
}

void MainWindow::singleStepClicked()
{
    if (m_pTargetModel->IsRunning())
        return;
    m_pDispatcher->SendCommandPacket("step");
}

void MainWindow::nextClicked()
{
    if (m_pTargetModel->IsRunning())
        return;

    // Work out where the next PC is
    if (m_disasm.lines.size() == 0)
        return;

    const Disassembler::line& nextInst = m_disasm.lines[0];
    // Either "next" or set breakpoint to following instruction
    bool shouldStepOver = DisAnalyse::isSubroutine(nextInst.inst) ||
                          DisAnalyse::isTrap(nextInst.inst);
    if (shouldStepOver)
    {
        uint32_t next_pc = nextInst.inst.byte_count + nextInst.address;
        m_pDispatcher->RunToPC(next_pc);
    }
    else
    {
        m_pDispatcher->SendCommandPacket("step");
    }
}

// Network
void MainWindow::Connect()
{
    // Create the TCP socket and start listening
    QHostAddress qha(QHostAddress::LocalHost);
    tcpSocket->connectToHost(qha, 56001);
}

void MainWindow::Disconnect()
{
    tcpSocket->disconnectFromHost();
}

QString DispReg16(int regIndex, const Registers& prevRegs, const Registers& regs)
{
    const char* col = (regs.m_value[regIndex] != prevRegs.m_value[regIndex]) ? "red" : "black";
    return QString::asprintf("%s: <span style=\"color:%s\">%04x</span>", Registers::s_names[regIndex], col, regs.m_value[regIndex]);
}
QString DispReg32(int regIndex, const Registers& prevRegs, const Registers& regs)
{
    const char* col = (regs.m_value[regIndex] != prevRegs.m_value[regIndex]) ? "red" : "black";
    return QString::asprintf("%s: <span style=\"color:%s\">%08x</span>", Registers::s_names[regIndex], col, regs.m_value[regIndex]);
}

QString DispSR(const Registers& prevRegs, const Registers& regs, uint32_t bit, const char* pName)
{
	uint32_t mask = 1U << bit;
	uint32_t valNew = regs.m_value[Registers::SR] & mask;
	uint32_t valOld = prevRegs.m_value[Registers::SR] & mask;

    const char* col = valNew != valOld ? "red" : "black";
	const char* text = valNew != 0 ? pName : ".";
    return QString::asprintf("<span style=\"color:%s\">%s</span>", col, text);
}

void MainWindow::PopulateRegisters()
{
    if (!m_pTargetModel->IsConnected())
    {
        m_pRegistersTextEdit->clear();
        return;
    }

    if (m_pTargetModel->IsRunning())
		return;

	// Build up the text area
	QString regsText;
    QTextStream ref(&regsText);

	Registers regs = m_pTargetModel->GetRegs();

    ref << "<font face=\"Courier\">";
    ref << DispReg32(Registers::PC, m_prevRegs, regs) << "   ";
    if (m_disasm.lines.size() > 0)
        Disassembler::print(m_disasm.lines[0].inst, m_disasm.lines[0].address, ref);
    ref << "     ;" << FindSymbol(regs.m_value[Registers::PC] & 0xffffff);
    ref << "<br>";

    ref << DispReg16(Registers::SR, m_prevRegs, regs) << "   ";
	ref << DispSR(m_prevRegs, regs, 15, "T");
	ref << DispSR(m_prevRegs, regs, 14, "T");
	ref << " ";
	ref << DispSR(m_prevRegs, regs, 13, "S");
	ref << " ";
	ref << DispSR(m_prevRegs, regs, 10, "2");
	ref << DispSR(m_prevRegs, regs, 9, "1");
	ref << DispSR(m_prevRegs, regs, 8, "0");
	ref << " ";
	ref << DispSR(m_prevRegs, regs, 4, "X");
	ref << DispSR(m_prevRegs, regs, 3, "N");
	ref << DispSR(m_prevRegs, regs, 2, "Z");
	ref << DispSR(m_prevRegs, regs, 1, "V");
	ref << DispSR(m_prevRegs, regs, 0, "C");
    ref << "<br><br>";
    ref << DispReg32(Registers::D0, m_prevRegs, regs) << " " << DispReg32(Registers::A0, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A0] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D1, m_prevRegs, regs) << " " << DispReg32(Registers::A1, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A1] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D2, m_prevRegs, regs) << " " << DispReg32(Registers::A2, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A2] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D3, m_prevRegs, regs) << " " << DispReg32(Registers::A3, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A3] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D4, m_prevRegs, regs) << " " << DispReg32(Registers::A4, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A4] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D5, m_prevRegs, regs) << " " << DispReg32(Registers::A5, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A5] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D6, m_prevRegs, regs) << " " << DispReg32(Registers::A6, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A6] & 0xffffff) << "<br>";
    ref << DispReg32(Registers::D7, m_prevRegs, regs) << " " << DispReg32(Registers::A7, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A7] & 0xffffff) << "<br>";
    ref << "</font>";
    m_pRegistersTextEdit->setHtml(regsText);
}

QString MainWindow::FindSymbol(uint32_t addr)
{
    Symbol sym;
    if (!m_pTargetModel->GetSymbolTable().FindLowerOrEqual(addr & 0xffffff, sym))
        return QString();

    uint32_t offset = addr - sym.address;
    if (offset)
        return QString::asprintf("%s+%d", sym.name.c_str(), offset);
    return QString::fromStdString(sym.name);
}

void MainWindow::PopulateRunningSquare()
{
    QPalette pal = m_pRunningSquare->palette();

    // set black background
    QColor col = Qt::red;
    if (!m_pTargetModel->IsConnected())
    {
        col = Qt::gray;
    }
    else if (m_pTargetModel->IsRunning())
    {
        col = Qt::green;
    }
    pal.setColor(QPalette::Background, col);
    m_pRunningSquare->setAutoFillBackground(true);
    m_pRunningSquare->setPalette(pal);
}

void MainWindow::updateWindowMenu()
{
    disasmWindowAct0->setChecked(m_pDisasmWidget0->isVisible());
    disasmWindowAct1->setChecked(m_pDisasmWidget1->isVisible());
    memoryWindowAct0->setChecked(m_pMemoryViewWidget0->isVisible());
    memoryWindowAct1->setChecked(m_pMemoryViewWidget1->isVisible());
}

void MainWindow::menuConnect()
{
    Connect();
}

void MainWindow::menuDisconnect()
{
    Disconnect();
}

void MainWindow::menuDisasmWindow0()
{
    toggleVis(m_pDisasmWidget0);
}

void MainWindow::menuDisasmWindow1()
{
    toggleVis(m_pDisasmWidget1);
}

void MainWindow::menuMemoryWindow0()
{
    toggleVis(m_pMemoryViewWidget0);
}

void MainWindow::menuMemoryWindow1()
{
    toggleVis(m_pMemoryViewWidget1);
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("hrdb"),
            tr("hrdb - Hatari remote debugger GUI"));
}

void MainWindow::aboutQt()
{
}

//! [4]
void MainWindow::createActions()
{
    // "File"
    connectAct = new QAction(tr("&Connect"), this);
    connectAct->setStatusTip(tr("Connect to Hatari"));
    connect(connectAct, &QAction::triggered, this, &MainWindow::Connect);

    disconnectAct = new QAction(tr("&Disonnect"), this);
    disconnectAct->setStatusTip(tr("Disconnect from Hatari"));
    connect(disconnectAct, &QAction::triggered, this, &MainWindow::Disconnect);

    // "Window"
    disasmWindowAct0 = new QAction(tr("&Disassembly 1"), this);
    disasmWindowAct0->setStatusTip(tr("Show the memory window"));
    disasmWindowAct0->setCheckable(true);
    connect(disasmWindowAct0, &QAction::triggered, this, &MainWindow::menuDisasmWindow0);

    disasmWindowAct1 = new QAction(tr("&Disassembly 2"), this);
    disasmWindowAct1->setStatusTip(tr("Show the memory window"));
    disasmWindowAct1->setCheckable(true);
    connect(disasmWindowAct1, &QAction::triggered, this, &MainWindow::menuDisasmWindow1);

    memoryWindowAct0 = new QAction(tr("&Memory 1"), this);
    memoryWindowAct0->setStatusTip(tr("Show the memory window"));
    memoryWindowAct0->setCheckable(true);
    connect(memoryWindowAct0, &QAction::triggered, this, &MainWindow::menuMemoryWindow0);

    memoryWindowAct1 = new QAction(tr("&Memory 2"), this);
    memoryWindowAct1->setStatusTip(tr("Show the memory window"));
    memoryWindowAct1->setCheckable(true);
    connect(memoryWindowAct1, &QAction::triggered, this, &MainWindow::menuMemoryWindow1);

    // "About"
    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);
}
//! [7]

//! [8]
void MainWindow::createMenus()
{
    // "File"
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(connectAct);
    fileMenu->addAction(disconnectAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
    //editMenu->addAction(undoAct);
    //editMenu->addAction(redoAct);
    editMenu->addSeparator();
    //editMenu->addAction(cutAct);
    //editMenu->addAction(copyAct);
    //editMenu->addAction(pasteAct);
    editMenu->addSeparator();

    windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(disasmWindowAct0);
    windowMenu->addAction(disasmWindowAct1);
    windowMenu->addAction(memoryWindowAct0);
    windowMenu->addAction(memoryWindowAct1);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}

void MainWindow::toggleVis(QWidget* pWidget)
{
    if (pWidget->isVisible())
        pWidget->hide();
    else
        pWidget->show();
}
