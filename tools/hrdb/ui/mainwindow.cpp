#include "mainwindow.h"

#include <iostream>
#include <QtWidgets>
#include <QShortcut>
#include <QFontDatabase>

#include "../transport/dispatcher.h"
#include "../models/targetmodel.h"
#include "../models/exceptionmask.h"

#include "disasmwidget.h"
#include "memoryviewwidget.h"
#include "graphicsinspector.h"
#include "breakpointswidget.h"
#include "consolewindow.h"
#include "addbreakpointdialog.h"
#include "exceptiondialog.h"
#include "rundialog.h"

static QString CreateNumberTooltip(uint32_t value)
{
    uint16_t word = value & 0xffff;
    uint16_t byte = value & 0xff;

    QString final;
    QTextStream ref(&final);

    if (value & 0x80000000)
        ref << QString::asprintf("LONG %u (%d)\n", value, static_cast<int32_t>(value));
    else
        ref << QString::asprintf("LONG %u\n", value);
    if (value & 0x8000)
        ref << QString::asprintf("WORD %u (%d)\n", word, static_cast<int16_t>(word));
    else
        ref << QString::asprintf("WORD %u\n", word);
    if (value & 0x80)
        ref << QString::asprintf("BYTE %u (%d)\n", byte, static_cast<int8_t>(byte));
    else
        ref << QString::asprintf("BYTE %u\n", byte);

    ref << "BINARY ";
    for (int bit = 31; bit >= 0; --bit)
        ref << ((value & (1U << bit)) ? "1" : "0");
    ref << "\n";

    ref << "ASCII \"";
    for (int bit = 3; bit >= 0; --bit)
    {
        unsigned char val = static_cast<unsigned char>(value >> (bit * 8));
        ref << ((val >= 32 && val < 128) ? QString(val) : ".");
    }
    ref << "\"\n";

    return final;
}


RegisterWidget::RegisterWidget(QWidget *parent, TargetModel *pTargetModel, Dispatcher *pDispatcher) :
    QWidget(parent),
    m_pDispatcher(pDispatcher),
    m_pTargetModel(pTargetModel)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Listen for target changes
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal,        this, &RegisterWidget::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::registersChangedSignal,        this, &RegisterWidget::registersChangedSlot);
    connect(m_pTargetModel, &TargetModel::connectChangedSignal,          this, &RegisterWidget::connectChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,           this, &RegisterWidget::memoryChangedSlot);
    connect(m_pTargetModel, &TargetModel::symbolTableChangedSignal,      this, &RegisterWidget::symbolTableChangedSlot);
    connect(m_pTargetModel, &TargetModel::startStopChangedSignalDelayed, this, &RegisterWidget::startStopDelayedSlot);

    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setMouseTracking(true);
    UpdateFont();
}

RegisterWidget::~RegisterWidget()
{

}

void RegisterWidget::paintEvent(QPaintEvent * ev)
{
    QWidget::paintEvent(ev);

    QPainter painter(this);
    painter.setFont(monoFont);
    QFontMetrics info(painter.fontMetrics());
    const QPalette& pal = this->palette();

    const QBrush& br = pal.background().color();
    painter.fillRect(this->rect(), br);
    painter.setPen(QPen(pal.dark(), hasFocus() ? 6 : 2));
    painter.drawRect(this->rect());

    for (int i = 0; i < m_tokens.size(); ++i)
    {
        Token& tok = m_tokens[i];
        painter.setPen(tok.highlight ? Qt::red : pal.text().color());
        painter.drawText(tok.x * char_width, y_base + tok.y * y_height, tok.text);

        int x = tok.x * char_width;
        int y = 0 + tok.y * y_height;
        int w = info.horizontalAdvance(tok.text);
        int h = y_height;
        tok.rect.setRect(x, y, w, h);
    }
}

bool RegisterWidget::event(QEvent *event)
{

    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        int index = -1;
        for (int i = 0; i < m_tokens.size(); ++i)
        {
            const Token& tok = m_tokens[i];
            if (tok.rect.contains(helpEvent->pos()))
            {
                index = i;
                break;
            }
        }

        if (index != -1)
        {
            QString text = GetTooltipText(m_tokens[index]);
            if (text.size() != 0)
                QToolTip::showText(helpEvent->globalPos(), text);
        }
        else
        {
            QToolTip::hideText();
            event->ignore();
        }

        return true;
    }
    return QWidget::event(event);
}

void RegisterWidget::connectChangedSlot()
{
    PopulateRegisters();
}

void RegisterWidget::startStopChangedSlot()
{
    bool isRunning = m_pTargetModel->IsRunning();

    // Update text here
    if (isRunning)
    {
        // Update our previous values
        // TODO: this is not the ideal way to do this, since there
        // are timing issues. In future, parcel up the stopping updates so
        // that widget refreshes happen as one.
        m_prevRegs = m_pTargetModel->GetRegs();

        // Don't populate display here, since it causes colours to flash
    }
    else
    {
        // STOPPED
        // We don't do any requests here; the MainWindow does them.
        // We just listen for the callbacks.
    }
}

void RegisterWidget::startStopDelayedSlot(int running)
{
    if (running)
    {
        m_tokens.clear();
        AddToken(1, 1, tr("Running, Ctrl+R to break..."), TokenType::kNone, 0, false);
        update();
    }
}

void RegisterWidget::registersChangedSlot(uint64_t /*commandId*/)
{
    // Update text here
    PopulateRegisters();
}

void RegisterWidget::memoryChangedSlot(int slot, uint64_t /*commandId*/)
{
    if (slot != MemorySlot::kMainPC)
        return;

    // Disassemble the first instruction
    m_disasm.lines.clear();
    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kMainPC);
    if (!pMem)
        return;

    buffer_reader disasmBuf(pMem->GetData(), pMem->GetSize());
    Disassembler::decode_buf(disasmBuf, m_disasm, pMem->GetAddress(), 2);
    PopulateRegisters();
}

void RegisterWidget::symbolTableChangedSlot(uint64_t /*commandId*/)
{
    PopulateRegisters();
}

void RegisterWidget::PopulateRegisters()
{
    m_tokens.clear();
    if (!m_pTargetModel->IsConnected())
    {
        AddToken(1, 1, tr("Not connected."), TokenType::kNone, 0, false);
        update();
        return;
    }

    // Build up the text area
    regs = m_pTargetModel->GetRegs();

    AddReg32(1, 0, Registers::PC, m_prevRegs, regs);
    QString disasmText;
    QTextStream ref(&disasmText);
    if (m_disasm.lines.size() > 0)
    {
        const instruction& inst = m_disasm.lines[0].inst;
        Disassembler::print(inst, m_disasm.lines[0].address, ref);
        QString sym = FindSymbol(GET_REG(regs, PC) & 0xffffff);

        bool branchTaken;
        if (DisAnalyse::isBranch(inst, regs, branchTaken))
        {
            if (branchTaken)
                ref << " [TAKEN]";
            else
                ref << " [NOT TAKEN]";
        }

        if (sym.size() != 0)
            ref << "     ;" << sym;

        AddToken(21, 0, disasmText, TokenType::kNone, 0, false);
    }

    AddReg16(1, 2, Registers::SR, m_prevRegs, regs);
    AddSR(10, 2, m_prevRegs, regs, 15, "T");
    AddSR(11, 2, m_prevRegs, regs, 14, "T");
    AddSR(12, 2, m_prevRegs, regs, 13, "S");
    AddSR(15, 2, m_prevRegs, regs, 10, "2");
    AddSR(16, 2, m_prevRegs, regs, 9, "1");
    AddSR(17, 2, m_prevRegs, regs, 8, "0");

    AddSR(20, 2, m_prevRegs, regs, 4, "X");
    AddSR(21, 2, m_prevRegs, regs, 3, "N");
    AddSR(22, 2, m_prevRegs, regs, 2, "Z");
    AddSR(23, 2, m_prevRegs, regs, 1, "V");
    AddSR(24, 2, m_prevRegs, regs, 0, "C");

    uint32_t ex = GET_REG(regs, EX);
    if (ex != 0)
        AddToken(1, 3, QString::asprintf("EXCEPTION: %s", ExceptionMask::GetName(ex)), TokenType::kNone, 0, true);

    for (uint32_t reg = 0; reg < 8; ++reg)
    {
        int32_t y = 4 + static_cast<int>(reg);
        AddReg32(1, y, Registers::D0 + reg, m_prevRegs, regs); AddReg32(15, y, Registers::A0 + reg, m_prevRegs, regs); AddSymbol(29, y, regs.m_value[Registers::A0 + reg]);
    }
    AddReg32(14, 12, Registers::USP, m_prevRegs, regs); AddSymbol(29, 12, regs.m_value[Registers::USP]);
    AddReg32(14, 13, Registers::ISP, m_prevRegs, regs); AddSymbol(29, 13, regs.m_value[Registers::ISP]);

    // Sundry info
    AddToken(1, 15, QString::asprintf("VBL: %10u Frame Cycles: %6u", GET_REG(regs, VBL), GET_REG(regs, FrameCycles)), TokenType::kNone, 0, false);
    AddToken(1, 16, QString::asprintf("HBL: %10u Line Cycles:  %6u", GET_REG(regs, HBL), GET_REG(regs, LineCycles)), TokenType::kNone, 0, false);
    update();
}

void RegisterWidget::UpdateFont()
{
    monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    QPainter painter(this);
    painter.setFont(monoFont);
    QFontMetrics info(painter.fontMetrics());
    y_base = info.ascent();
    y_height = info.lineSpacing();
    char_width = info.horizontalAdvance("0");
}

QString RegisterWidget::FindSymbol(uint32_t addr)
{
    Symbol sym;
    if (!m_pTargetModel->GetSymbolTable().FindLowerOrEqual(addr & 0xffffff, sym))
        return QString();

    uint32_t offset = addr - sym.address;
    if (offset)
        return QString::asprintf("%s+%d", sym.name.c_str(), offset);
    return QString::fromStdString(sym.name);
}

void RegisterWidget::AddToken(int x, int y, QString text, TokenType type, uint32_t subIndex, bool highlight)
{
    Token tok;
    tok.x = x;
    tok.y = y;
    tok.text = text;
    tok.type = type;
    tok.subIndex = subIndex;
    tok.highlight = highlight;
    m_tokens.push_back(tok);
}

void RegisterWidget::AddReg16(int x, int y, uint32_t regIndex, const Registers& prevRegs, const Registers& regs)
{
    bool highlight = (regs.m_value[regIndex] != prevRegs.m_value[regIndex]);

    QString label = QString::asprintf("%s:",  Registers::s_names[regIndex]);
    QString value = QString::asprintf("%04x", regs.m_value[regIndex]);
    AddToken(x, y, label, TokenType::kNone, 0, false);
    AddToken(x + label.size() + 1, y, value, TokenType::kRegister, regIndex, highlight);
}

void RegisterWidget::AddReg32(int x, int y, uint32_t regIndex, const Registers& prevRegs, const Registers& regs)
{
    bool highlight = (regs.m_value[regIndex] != prevRegs.m_value[regIndex]);

    QString label = QString::asprintf("%s:",  Registers::s_names[regIndex]);
    QString value = QString::asprintf("%08x", regs.m_value[regIndex]);
    AddToken(x, y, label, TokenType::kNone, 0, false);
    AddToken(x + label.size() + 1, y, value, TokenType::kRegister, regIndex, highlight);
}

void RegisterWidget::AddSR(int x, int y, const Registers& prevRegs, const Registers& regs, uint32_t bit, const char* pName)
{
    uint32_t mask = 1U << bit;
    uint32_t valNew = regs.m_value[Registers::SR] & mask;
    uint32_t valOld = prevRegs.m_value[Registers::SR] & mask;
    bool highlight = valNew != valOld;
    const char* text = valNew != 0 ? pName : ".";

    AddToken(x, y, text, TokenType::kNone, 0, highlight);
}

void RegisterWidget::AddSymbol(int x, int y, uint32_t address)
{
    QString symText = FindSymbol(address & 0xffffff);
    if (!symText.size())
        return;

    AddToken(x, y, symText, TokenType::kSymbol, address, false);
}

QString RegisterWidget::GetTooltipText(const RegisterWidget::Token& token)
{
    switch (token.type)
    {
    case TokenType::kRegister:
        {
            uint32_t value = regs.Get(token.subIndex);
            return CreateNumberTooltip(value);
        }
    case TokenType::kSymbol:
        return QString::asprintf("Original address: $%08x", token.subIndex);
    default:
        break;
    }
    return "";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName("MainWindow");

    m_pTargetModel = new TargetModel();
    m_pDispatcher = new Dispatcher(m_session.m_pTcpSocket, m_pTargetModel);

    // Top row of buttons
    m_pRunningSquare = new QWidget(this);
    m_pRunningSquare->setFixedSize(10, 25);
    m_pRunningSquare->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    m_pStartStopButton = new QPushButton("Break", this);
    m_pStepIntoButton = new QPushButton("Step", this);
    m_pStepOverButton = new QPushButton("Next", this);
    m_pRunToButton = new QPushButton("Run Until:", this);
    m_pRunToCombo = new QComboBox(this);
    m_pRunToCombo->insertItem(0, "RTS");
    m_pRunToCombo->insertItem(1, "RTE");
    m_pRunToCombo->insertItem(2, "Next VBL");
    m_pRunToCombo->insertItem(3, "Next HBL");

    // Register/status window
    m_pRegisterWidget = new RegisterWidget(this, m_pTargetModel, m_pDispatcher);

    m_pDisasmWidget0 = new DisasmWindow(this, m_pTargetModel, m_pDispatcher, 0);
    m_pDisasmWidget0->setWindowTitle("Disassembly (Alt+D)");
    m_pDisasmWidget1 = new DisasmWindow(this, m_pTargetModel, m_pDispatcher, 1);
    m_pDisasmWidget1->setWindowTitle("Disassembly 2");
    m_pMemoryViewWidget0 = new MemoryWindow(this, m_pTargetModel, m_pDispatcher, 0);
    m_pMemoryViewWidget0->setWindowTitle("Memory (Alt+M)");
    m_pMemoryViewWidget1 = new MemoryWindow(this, m_pTargetModel, m_pDispatcher, 1);
    m_pMemoryViewWidget1->setWindowTitle("Memory 2");
    m_pGraphicsInspector = new GraphicsInspectorWidget(this, m_pTargetModel, m_pDispatcher);
    m_pGraphicsInspector->setWindowTitle("Graphics Inspector (Alt+G)");
    m_pBreakpointsWidget = new BreakpointsWindow(this, m_pTargetModel, m_pDispatcher);
    m_pBreakpointsWidget->setWindowTitle("Breakpoints (Alt+B)");
    m_pConsoleWindow = new ConsoleWindow(this, m_pTargetModel, m_pDispatcher);

    m_pExceptionDialog = new ExceptionDialog(this, m_pTargetModel, m_pDispatcher);
    m_pRunDialog = new RunDialog(this, &m_session);

    // https://doc.qt.io/qt-5/qtwidgets-layouts-basiclayouts-example.html
    QVBoxLayout *vlayout = new QVBoxLayout;
    QHBoxLayout *hlayout = new QHBoxLayout;
    auto pTopGroupBox = new QWidget(this);
    auto pMainGroupBox = new QGroupBox(this);

    hlayout->addWidget(m_pRunningSquare);
    hlayout->addWidget(m_pStartStopButton);
    hlayout->addWidget(m_pStepIntoButton);
    hlayout->addWidget(m_pStepOverButton);
    hlayout->addWidget(m_pRunToButton);
    hlayout->addWidget(m_pRunToCombo);

    //hlayout->setAlignment(m_pRunToCombo, Qt::Align);
    pTopGroupBox->setLayout(hlayout);

    vlayout->addWidget(pTopGroupBox);
    vlayout->addWidget(m_pRegisterWidget);
    vlayout->setAlignment(Qt::Alignment(Qt::AlignTop));
    pMainGroupBox->setFlat(true);
    pMainGroupBox->setLayout(vlayout);

    setCentralWidget(pMainGroupBox);

    this->addDockWidget(Qt::BottomDockWidgetArea, m_pDisasmWidget0);
    this->addDockWidget(Qt::RightDockWidgetArea, m_pMemoryViewWidget0);
    this->addDockWidget(Qt::BottomDockWidgetArea, m_pMemoryViewWidget1);
    this->addDockWidget(Qt::BottomDockWidgetArea, m_pDisasmWidget1);
    this->addDockWidget(Qt::LeftDockWidgetArea, m_pGraphicsInspector);
    this->addDockWidget(Qt::BottomDockWidgetArea, m_pBreakpointsWidget);
    this->addDockWidget(Qt::BottomDockWidgetArea, m_pConsoleWindow);

    loadSettings();

    // Set up menus (reflecting current state)
    createActions();
    createMenus();

    // Listen for target changes
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &MainWindow::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::connectChangedSignal,   this, &MainWindow::connectChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,    this, &MainWindow::memoryChangedSlot);

    // Wire up cross-window requests
    connect(m_pTargetModel, &TargetModel::addressRequested, m_pMemoryViewWidget0, &MemoryWindow::requestAddress);
    connect(m_pTargetModel, &TargetModel::addressRequested, m_pMemoryViewWidget1, &MemoryWindow::requestAddress);
    connect(m_pTargetModel, &TargetModel::addressRequested, m_pDisasmWidget0,     &DisasmWindow::requestAddress);
    connect(m_pTargetModel, &TargetModel::addressRequested, m_pDisasmWidget1,     &DisasmWindow::requestAddress);

    // Wire up buttons to actions
    connect(m_pStartStopButton, &QAbstractButton::clicked, this, &MainWindow::startStopClicked);
    connect(m_pStepIntoButton, &QAbstractButton::clicked, this, &MainWindow::singleStepClicked);
    connect(m_pStepOverButton, &QAbstractButton::clicked, this, &MainWindow::nextClicked);
    connect(m_pRunToButton, &QAbstractButton::clicked, this, &MainWindow::runToClicked);

    // Wire up menu appearance
    connect(windowMenu, &QMenu::aboutToShow, this, &MainWindow::updateWindowMenu);

	// Keyboard shortcuts
    new QShortcut(QKeySequence(tr("Ctrl+R", "Start/Stop")),         this, SLOT(startStopClicked()));
    new QShortcut(QKeySequence(tr("n",      "Next")),               this, SLOT(nextClicked()));
    new QShortcut(QKeySequence(tr("s",      "Step")),               this, SLOT(singleStepClicked()));
    new QShortcut(QKeySequence(tr("Esc",    "Break")),              this, SLOT(breakPressed()));
    new QShortcut(QKeySequence(tr("u",      "Run Until")),          this, SLOT(runToClicked()));

    // Try initial connect
    Connect();

    // Update everything
    connectChangedSlot();
    startStopChangedSlot();

//    m_pDisasmWidget0->keyFocus();
}

MainWindow::~MainWindow()
{
	delete m_pDispatcher;
    delete m_pTargetModel;
}

void MainWindow::connectChangedSlot()
{
    PopulateRunningSquare();
    updateButtonEnable();
#if 0
    // Experimental: force Hatari output to a file
    if (m_pTargetModel->IsConnected())
    {
        QString logCmd("setstd ");
        m_session.m_pLoggingFile->open();
        logCmd += m_session.m_pLoggingFile->fileName();
        m_pDispatcher->SendCommandPacket(logCmd.toStdString().c_str());
    }
#endif
}

void MainWindow::startStopChangedSlot()
{
    bool isRunning = m_pTargetModel->IsRunning();

    // Update text here
    if (!isRunning)
    {
        // STOPPED
		// TODO this is where all windows should put in requests for data
        // The Main Window does this and other windows feed from it.

        // Do all the "essentials" straight away.
        m_pDispatcher->SendCommandPacket("regs");
        m_pDispatcher->RequestMemory(MemorySlot::kMainPC, m_pTargetModel->GetPC(), 10);

        m_pDispatcher->SendCommandPacket("bplist");
        m_pDispatcher->SendCommandPacket("exmask");

        // Video memory is generally handy
        m_pDispatcher->RequestMemory(MemorySlot::kVideo, 0xff8200, 0x70);

        // Only re-request symbols if we didn't find any the first time
        if (m_pTargetModel->GetSymbolTable().m_userSymbolCount == 0)  // NO CHECK
            m_pDispatcher->SendCommandPacket("symlist");
    }
    PopulateRunningSquare();
    updateButtonEnable();
}

void MainWindow::memoryChangedSlot(int slot, uint64_t /*commandId*/)
{
    if (slot != MemorySlot::kMainPC)
        return;

    // Disassemble the first instruction
    m_disasm.lines.clear();
    const Memory* pMem = m_pTargetModel->GetMemory(MemorySlot::kMainPC);
    if (!pMem)
        return;

    // Fetch data and decode the next instruction.
    buffer_reader disasmBuf(pMem->GetData(), pMem->GetSize());
    Disassembler::decode_buf(disasmBuf, m_disasm, pMem->GetAddress(), 1);
}

void MainWindow::startStopClicked()
{
    if (!m_pTargetModel->IsConnected())
        return;

    if (m_pTargetModel->IsRunning())
        m_pDispatcher->SendCommandPacket("break");
	else
        m_pDispatcher->SendCommandPacket("run");
}

void MainWindow::singleStepClicked()
{
    if (!m_pTargetModel->IsConnected())
        return;

    if (m_pTargetModel->IsRunning())
        return;

    m_pDispatcher->SendCommandPacket("step");
}

void MainWindow::nextClicked()
{
    if (!m_pTargetModel->IsConnected())
        return;

    if (m_pTargetModel->IsRunning())
        return;

    // Work out where the next PC is
    if (m_disasm.lines.size() == 0)
        return;

    const Disassembler::line& nextInst = m_disasm.lines[0];
    // Either "next" or set breakpoint to following instruction
    bool shouldStepOver = DisAnalyse::isSubroutine(nextInst.inst) ||
                          DisAnalyse::isTrap(nextInst.inst) ||
                          DisAnalyse::isBackDbf(nextInst.inst);
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

void MainWindow::runToClicked()
{
    if (!m_pTargetModel->IsConnected())
        return;
    if (m_pTargetModel->IsRunning())
        return;

    if (m_pRunToCombo->currentIndex() == 0)
        m_pDispatcher->SetBreakpoint("(pc).w = $4e75", true);      // RTS
    else if (m_pRunToCombo->currentIndex() == 1)
        m_pDispatcher->SetBreakpoint("(pc).w = $4e73", true);      // RTE
    else if (m_pRunToCombo->currentIndex() == 2)
        m_pDispatcher->SetBreakpoint("VBL ! VBL", true);        // VBL
        //m_pDispatcher->SetBreakpoint("pc = ($70).l", true);        // VBL interrupt code
    else if (m_pRunToCombo->currentIndex() == 3)
        m_pDispatcher->SetBreakpoint("HBL ! HBL", true);        // VBL
    else
        return;
    m_pDispatcher->SendCommandPacket("run");
}

void MainWindow::addBreakpointPressed()
{
    AddBreakpointDialog dialog(this, m_pTargetModel, m_pDispatcher);
    dialog.exec();
}

void MainWindow::breakPressed()
{
    if (!m_pTargetModel->IsConnected())
        return;

    if (m_pTargetModel->IsRunning())
        m_pDispatcher->SendCommandPacket("break");
}

// Actions
void MainWindow::Run()
{
    m_pRunDialog->setModal(true);
    m_pRunDialog->show();
    // We can't connect here since the dialog hasn't really run yet.
}

void MainWindow::Connect()
{
    m_session.Connect();
}

void MainWindow::Disconnect()
{
    m_session.Disconnect();
}

void MainWindow::ExceptionsDialog()
{
    m_pExceptionDialog->setModal(true);
    m_pExceptionDialog->show();
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
    graphicsInspectorAct->setChecked(m_pGraphicsInspector->isVisible());
    breakpointsWindowAct->setChecked(m_pBreakpointsWidget->isVisible());
    consoleWindowAct->setChecked(m_pConsoleWindow->isVisible());
}

void MainWindow::updateButtonEnable()
{
    bool isConnected = m_pTargetModel->IsConnected();
    bool isRunning = m_pTargetModel->IsRunning();

    // Buttons...
    m_pStartStopButton->setEnabled(isConnected);
    m_pStartStopButton->setText(isRunning ? "Break" : "Run");

    m_pStepIntoButton->setEnabled(isConnected && !isRunning);
    m_pStepOverButton->setEnabled(isConnected && !isRunning);
    m_pRunToButton->setEnabled(isConnected && !isRunning);

    // Menu items...
    connectAct->setEnabled(!isConnected);
    disconnectAct->setEnabled(isConnected);

    exceptionsAct->setEnabled(isConnected);
}


void MainWindow::loadSettings()
{
    //https://doc.qt.io/qt-5/qsettings.html#details
    QSettings settings;

    settings.beginGroup("MainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    if(!restoreState(settings.value("windowState").toByteArray()))
    {
        // Default docking status
        m_pDisasmWidget0->setVisible(true);
        m_pDisasmWidget1->setVisible(false);
        m_pMemoryViewWidget0->setVisible(true);
        m_pMemoryViewWidget1->setVisible(false);
        m_pGraphicsInspector->setVisible(true);
        m_pBreakpointsWidget->setVisible(true);
        m_pConsoleWindow->setVisible(false);
    }
    else
    {

        QDockWidget* wlist[] =
        {
            m_pDisasmWidget0, m_pDisasmWidget1,
            m_pMemoryViewWidget0, m_pMemoryViewWidget1,
            m_pBreakpointsWidget, m_pGraphicsInspector,
            m_pConsoleWindow,
            nullptr
        };
        QDockWidget** pCurr = wlist;
        while (*pCurr)
        {
            // Fix for docking system: for some reason, we need to manually
            // activate floating docking windows for them to appear
            if ((*pCurr)->isFloating())
            {
                (*pCurr)->activateWindow();
            }
            ++pCurr;
        }
    }

    m_pRunToCombo->setCurrentIndex(settings.value("runto", QVariant(0)).toInt());
    settings.endGroup();
}

void MainWindow::saveSettings()
{
    // enclose in scope so it's saved before widgets are saved
    {
        QSettings settings;
        settings.beginGroup("MainWindow");
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState());
        settings.setValue("runto", m_pRunToCombo->currentIndex());
        settings.endGroup();
    }

    m_pDisasmWidget0->saveSettings();
    m_pDisasmWidget1->saveSettings();
    m_pMemoryViewWidget0->saveSettings();
    m_pMemoryViewWidget1->saveSettings();
    m_pGraphicsInspector->saveSettings();
    m_pConsoleWindow->saveSettings();
}

void MainWindow::menuConnect()
{
    Connect();
}

void MainWindow::menuDisconnect()
{
    Disconnect();
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
    runAct = new QAction(tr("&Run..."), this);
    runAct->setStatusTip(tr("Run Hatari"));
    connect(runAct, &QAction::triggered, this, &MainWindow::Run);

    connectAct = new QAction(tr("&Connect"), this);
    connectAct->setStatusTip(tr("Connect to Hatari"));
    connect(connectAct, &QAction::triggered, this, &MainWindow::Connect);

    disconnectAct = new QAction(tr("&Disconnect"), this);
    disconnectAct->setStatusTip(tr("Disconnect from Hatari"));
    connect(disconnectAct, &QAction::triggered, this, &MainWindow::Disconnect);

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    // Edit
    exceptionsAct = new QAction(tr("&Exceptions..."), this);
    exceptionsAct->setStatusTip(tr("Disconnect from Hatari"));
    connect(exceptionsAct, &QAction::triggered, this, &MainWindow::ExceptionsDialog);

    // "Window"
    disasmWindowAct0 = new QAction(tr("Disassembly 1"), this);
    disasmWindowAct0->setShortcut(QKeySequence("Alt+D"));
    disasmWindowAct0->setStatusTip(tr("Show the memory window"));
    disasmWindowAct0->setCheckable(true);

    disasmWindowAct1 = new QAction(tr("&Disassembly 2"), this);
    disasmWindowAct1->setStatusTip(tr("Show the memory window"));
    disasmWindowAct1->setCheckable(true);

    memoryWindowAct0 = new QAction(tr("&Memory 1"), this);
    memoryWindowAct0->setShortcut(QKeySequence("Alt+M"));
    memoryWindowAct0->setStatusTip(tr("Show the memory window"));
    memoryWindowAct0->setCheckable(true);

    memoryWindowAct1 = new QAction(tr("Memory 2"), this);
    memoryWindowAct1->setStatusTip(tr("Show the memory window"));
    memoryWindowAct1->setCheckable(true);

    graphicsInspectorAct = new QAction(tr("&Graphics Inspector"), this);
    graphicsInspectorAct->setShortcut(QKeySequence("Alt+G"));
    graphicsInspectorAct->setStatusTip(tr("Show the Graphics Inspector"));
    graphicsInspectorAct->setCheckable(true);

    breakpointsWindowAct = new QAction(tr("&Breakpoints"), this);
    breakpointsWindowAct->setShortcut(QKeySequence("Alt+B"));
    breakpointsWindowAct->setStatusTip(tr("Show the Breakpoints window"));
    breakpointsWindowAct->setCheckable(true);

    consoleWindowAct = new QAction(tr("&Console"), this);
    consoleWindowAct->setStatusTip(tr("Show the Console window"));
    consoleWindowAct->setCheckable(true);

    connect(disasmWindowAct0, &QAction::triggered, this,     [=] () { this->enableVis(m_pDisasmWidget0); m_pDisasmWidget0->keyFocus(); } );
    connect(disasmWindowAct1, &QAction::triggered, this,     [=] () { this->enableVis(m_pDisasmWidget1); m_pDisasmWidget1->keyFocus(); } );
    connect(memoryWindowAct0, &QAction::triggered, this,     [=] () { this->enableVis(m_pMemoryViewWidget0); m_pMemoryViewWidget0->keyFocus(); } );
    connect(memoryWindowAct1, &QAction::triggered, this,     [=] () { this->enableVis(m_pMemoryViewWidget1); m_pMemoryViewWidget1->keyFocus(); } );
    connect(graphicsInspectorAct, &QAction::triggered, this, [=] () { this->enableVis(m_pGraphicsInspector); m_pGraphicsInspector->keyFocus(); } );
    connect(breakpointsWindowAct, &QAction::triggered, this, [=] () { this->enableVis(m_pBreakpointsWidget); m_pBreakpointsWidget->keyFocus(); } );
    connect(consoleWindowAct,     &QAction::triggered, this, [=] () { this->enableVis(m_pConsoleWindow); m_pConsoleWindow->keyFocus(); } );

    // This should be an action
    new QShortcut(QKeySequence(tr("Shift+Alt+B",  "Add Breakpoint...")),  this, SLOT(addBreakpointPressed()));

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
    fileMenu->addAction(runAct);
    fileMenu->addAction(connectAct);
    fileMenu->addAction(disconnectAct);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addSeparator();
    editMenu->addAction(exceptionsAct);
    editMenu->addSeparator();

    windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(disasmWindowAct0);
    windowMenu->addAction(disasmWindowAct1);
    windowMenu->addAction(memoryWindowAct0);
    windowMenu->addAction(memoryWindowAct1);
    windowMenu->addAction(graphicsInspectorAct);
    windowMenu->addAction(breakpointsWindowAct);
    windowMenu->addAction(consoleWindowAct);

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}

void MainWindow::enableVis(QWidget* pWidget)
{
    // This used to be a toggle
    pWidget->setVisible(true);
    pWidget->setHidden(false);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    event->accept();
}
