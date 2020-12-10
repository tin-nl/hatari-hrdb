#include "mainwindow.h"

#include <iostream>
#include <QtWidgets>
#include <QtNetwork>
#include <QShortcut>

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

    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::TypeWriter);
    m_pRegistersTextEdit->setCurrentFont(monoFont);
    m_pRegistersTextEdit->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);
    m_pRegistersTextEdit->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

    m_pDisasmWindow = new DisasmWidget(this, m_pTargetModel, m_pDispatcher);
    m_pMemoryViewWidget = new MemoryViewWidget(this, m_pTargetModel, m_pDispatcher);

    // https://doc.qt.io/qt-5/qtwidgets-layouts-basiclayouts-example.html
    QVBoxLayout *vlayout = new QVBoxLayout;
    QHBoxLayout *hlayout = new QHBoxLayout;
    auto pTopGroupBox = new QWidget(this);
    auto pMainGroupBox = new QGroupBox(this);

    //pTopGroupBox->setFixedSize(400, 50);
    //pTopGroupBox->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred));
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

    this->addDockWidget(Qt::BottomDockWidgetArea, m_pDisasmWindow);
    this->addDockWidget(Qt::RightDockWidgetArea, m_pMemoryViewWidget);

	// Listen for target changes
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &MainWindow::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::registersChangedSignal, this, &MainWindow::registersChangedSlot);
    connect(m_pTargetModel, &TargetModel::connectChangedSignal,   this, &MainWindow::connectChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,    this, &MainWindow::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::symbolTableChangedSignal,this, &MainWindow::symbolTableChangedSlot);

	// Wire up buttons to actions
    connect(m_pStartStopButton, &QAbstractButton::clicked, this, &MainWindow::startStopClicked);
    connect(m_pSingleStepButton, &QAbstractButton::clicked, this, &MainWindow::singleStepClicked);

    // Set up menus
    createActions();
    createMenus();
	// Keyboard shortcuts
	new QShortcut(QKeySequence(tr("F5", "Start/Stop")),
					this,
					SLOT(startStopClicked()));

    new QShortcut(QKeySequence(tr("F10", "Next")),
					this,
                    SLOT(nextClicked()));

    new QShortcut(QKeySequence(tr("F11", "Step")),
                    this,
                    SLOT(singleStepClicked()));

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
		// TODO this is where all windows should put in requests for data
        m_pDispatcher->SendCommandPacket("regs");
        m_pDispatcher->SendCommandPacket("bplist");
        m_pDispatcher->RequestMemory(MemorySlot::kMainPC, "pc", "100");

        // Only re-request symbols if we didn't find any the first time
        if (m_pTargetModel->GetSymbolTable().m_userSymbolCount == 0)  // NO CHECK
            m_pDispatcher->SendCommandPacket("symlist");

        m_pStartStopButton->setText("Run");
		m_pSingleStepButton->setEnabled(true);	
	}
    PopulateRunningSquare();
    PopulateRegisters();
}

void MainWindow::registersChangedSlot()
{
	// Update text here
    PopulateRegisters();
}

void MainWindow::memoryChangedSlot(int slot)
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

void MainWindow::symbolTableChangedSlot()
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
    m_pDispatcher->SendCommandPacket("step");
}


void MainWindow::nextClicked()
{
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
        QString str = QString::asprintf("bp pc = $%x : once", next_pc);
        m_pDispatcher->SendCommandPacket(str.toStdString().c_str());
        m_pDispatcher->SendCommandPacket("run");
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
    ref << "     ;" << FindSymbol(regs.m_value[Registers::PC]);
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
    ref << DispReg32(Registers::D0, m_prevRegs, regs) << " " << DispReg32(Registers::A0, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A0]) << "<br>";
    ref << DispReg32(Registers::D1, m_prevRegs, regs) << " " << DispReg32(Registers::A1, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A1]) << "<br>";
    ref << DispReg32(Registers::D2, m_prevRegs, regs) << " " << DispReg32(Registers::A2, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A2]) << "<br>";
    ref << DispReg32(Registers::D3, m_prevRegs, regs) << " " << DispReg32(Registers::A3, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A3]) << "<br>";
    ref << DispReg32(Registers::D4, m_prevRegs, regs) << " " << DispReg32(Registers::A4, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A4]) << "<br>";
    ref << DispReg32(Registers::D5, m_prevRegs, regs) << " " << DispReg32(Registers::A5, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A5]) << "<br>";
    ref << DispReg32(Registers::D6, m_prevRegs, regs) << " " << DispReg32(Registers::A6, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A6]) << "<br>";
    ref << DispReg32(Registers::D7, m_prevRegs, regs) << " " << DispReg32(Registers::A7, m_prevRegs, regs) << " " << FindSymbol(regs.m_value[Registers::A7]) << "<br>";

    ref << "</font>";
    /*
	for (int i = 0; i < Registers::REG_COUNT; ++i)
	{
		QString pc_text;
		pc_text = QString::asprintf("%s: %08x\n", Registers::s_names[i], regs.m_value[i]);
		regsText += pc_text;
	}
    */
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



void MainWindow::newFile()
{
}

void MainWindow::menuConnect()
{
    Connect();
}

void MainWindow::menuDisconnect()
{
    Disconnect();
}

void MainWindow::print()
{
}

void MainWindow::undo()
{
}

void MainWindow::redo()
{
}

void MainWindow::cut()
{
}

void MainWindow::copy()
{
}

void MainWindow::paste()
{
}

void MainWindow::bold()
{
}

void MainWindow::italic()
{
}

void MainWindow::leftAlign()
{
}

void MainWindow::rightAlign()
{
}

void MainWindow::justify()
{
}

void MainWindow::center()
{
}

void MainWindow::setLineSpacing()
{
}

void MainWindow::setParagraphSpacing()
{
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Menu"),
            tr("The <b>Menu</b> example shows how to create "
               "menu-bar menus and context menus."));
}

void MainWindow::aboutQt()
{
}

//! [4]
void MainWindow::createActions()
{
//! [5]
    newAct = new QAction(tr("&New"), this);
    newAct->setShortcuts(QKeySequence::New);
    newAct->setStatusTip(tr("Create a new file"));
    connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
//! [4]

    openAct = new QAction(tr("&Connect..."), this);
    //openAct->setShortcuts(QKeySequence::Open);
    openAct->setStatusTip(tr("Connect to server"));
    connect(openAct, &QAction::triggered, this, &MainWindow::menuConnect);
//! [5]

    saveAct = new QAction(tr("&Disconnect"), this);
    //saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Disconnect from server"));
    connect(saveAct, &QAction::triggered, this, &MainWindow::menuDisconnect);

    printAct = new QAction(tr("&Print..."), this);
    printAct->setShortcuts(QKeySequence::Print);
    printAct->setStatusTip(tr("Print the document"));
    connect(printAct, &QAction::triggered, this, &MainWindow::print);

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    undoAct = new QAction(tr("&Undo"), this);
    undoAct->setShortcuts(QKeySequence::Undo);
    undoAct->setStatusTip(tr("Undo the last operation"));
    connect(undoAct, &QAction::triggered, this, &MainWindow::undo);

    redoAct = new QAction(tr("&Redo"), this);
    redoAct->setShortcuts(QKeySequence::Redo);
    redoAct->setStatusTip(tr("Redo the last operation"));
    connect(redoAct, &QAction::triggered, this, &MainWindow::redo);

    cutAct = new QAction(tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));
    connect(cutAct, &QAction::triggered, this, &MainWindow::cut);

    copyAct = new QAction(tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(copyAct, &QAction::triggered, this, &MainWindow::copy);

    pasteAct = new QAction(tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(pasteAct, &QAction::triggered, this, &MainWindow::paste);

    boldAct = new QAction(tr("&Bold"), this);
    boldAct->setCheckable(true);
    boldAct->setShortcut(QKeySequence::Bold);
    boldAct->setStatusTip(tr("Make the text bold"));
    connect(boldAct, &QAction::triggered, this, &MainWindow::bold);

    QFont boldFont = boldAct->font();
    boldFont.setBold(true);
    boldAct->setFont(boldFont);

    italicAct = new QAction(tr("&Italic"), this);
    italicAct->setCheckable(true);
    italicAct->setShortcut(QKeySequence::Italic);
    italicAct->setStatusTip(tr("Make the text italic"));
    connect(italicAct, &QAction::triggered, this, &MainWindow::italic);

    QFont italicFont = italicAct->font();
    italicFont.setItalic(true);
    italicAct->setFont(italicFont);

    setLineSpacingAct = new QAction(tr("Set &Line Spacing..."), this);
    setLineSpacingAct->setStatusTip(tr("Change the gap between the lines of a "
                                       "paragraph"));
    connect(setLineSpacingAct, &QAction::triggered, this, &MainWindow::setLineSpacing);

    setParagraphSpacingAct = new QAction(tr("Set &Paragraph Spacing..."), this);
    setParagraphSpacingAct->setStatusTip(tr("Change the gap between paragraphs"));
    connect(setParagraphSpacingAct, &QAction::triggered,
            this, &MainWindow::setParagraphSpacing);

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about);

    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(aboutQtAct, &QAction::triggered, this, &MainWindow::aboutQt);

    leftAlignAct = new QAction(tr("&Left Align"), this);
    leftAlignAct->setCheckable(true);
    leftAlignAct->setShortcut(tr("Ctrl+L"));
    leftAlignAct->setStatusTip(tr("Left align the selected text"));
    connect(leftAlignAct, &QAction::triggered, this, &MainWindow::leftAlign);

    rightAlignAct = new QAction(tr("&Right Align"), this);
    rightAlignAct->setCheckable(true);
    rightAlignAct->setShortcut(tr("Ctrl+R"));
    rightAlignAct->setStatusTip(tr("Right align the selected text"));
    connect(rightAlignAct, &QAction::triggered, this, &MainWindow::rightAlign);

    justifyAct = new QAction(tr("&Justify"), this);
    justifyAct->setCheckable(true);
    justifyAct->setShortcut(tr("Ctrl+J"));
    justifyAct->setStatusTip(tr("Justify the selected text"));
    connect(justifyAct, &QAction::triggered, this, &MainWindow::justify);

    centerAct = new QAction(tr("&Center"), this);
    centerAct->setCheckable(true);
    centerAct->setShortcut(tr("Ctrl+E"));
    centerAct->setStatusTip(tr("Center the selected text"));
    connect(centerAct, &QAction::triggered, this, &MainWindow::center);

//! [6] //! [7]
    alignmentGroup = new QActionGroup(this);
    alignmentGroup->addAction(leftAlignAct);
    alignmentGroup->addAction(rightAlignAct);
    alignmentGroup->addAction(justifyAct);
    alignmentGroup->addAction(centerAct);
    leftAlignAct->setChecked(true);
//! [6]
}
//! [7]

//! [8]
void MainWindow::createMenus()
{
//! [9] //! [10]
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(newAct);
//! [9]
    fileMenu->addAction(openAct);
//! [10]
    fileMenu->addAction(saveAct);
    fileMenu->addAction(printAct);
//! [11]
    fileMenu->addSeparator();
//! [11]
    fileMenu->addAction(exitAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);
    editMenu->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
//! [8]

//! [12]
    formatMenu = editMenu->addMenu(tr("&Format"));
    formatMenu->addAction(boldAct);
    formatMenu->addAction(italicAct);
    formatMenu->addSeparator()->setText(tr("Alignment"));
    formatMenu->addAction(leftAlignAct);
    formatMenu->addAction(rightAlignAct);
    formatMenu->addAction(justifyAct);
    formatMenu->addAction(centerAct);
    formatMenu->addSeparator();
    formatMenu->addAction(setLineSpacingAct);
    formatMenu->addAction(setParagraphSpacingAct);
}
//! [12]
