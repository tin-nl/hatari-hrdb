#include "mainwindow.h"

#include <iostream>
#include <QtWidgets>
#include <QtNetwork>
#include <QShortcut>

#include "dispatcher.h"
#include "targetmodel.h"

#include "disasmwidget.h"
#include "memoryviewwidget.h"

//#include "disassembler.h"
//#include "hopper/buffer.h"
//#include "hopper/instruction.h"
//#include "hopper/decode.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tcpSocket(new QTcpSocket(this))
{
    // Create the core data models, since other object want to connect to them.
    m_pTargetModel = new TargetModel();

    // Create the TCP socket and start listening
    QHostAddress qha(QHostAddress::LocalHost);
    tcpSocket->connectToHost(qha, 56001);

    m_pDispatcher = new Dispatcher(tcpSocket, m_pTargetModel);

    // https://doc.qt.io/qt-5/qtwidgets-layouts-basiclayouts-example.html
	auto pGroupBox = new QGroupBox(this);
    QVBoxLayout *layout = new QVBoxLayout;
    m_pStartStopButton = new QPushButton("STOP", pGroupBox);
    m_pSingleStepButton = new QPushButton("Step", pGroupBox);
    m_pRegistersTextEdit = new QTextEdit("", pGroupBox);
	m_pRegistersTextEdit->setReadOnly(true);
	m_pRegistersTextEdit->setAcceptRichText(false);

    m_pDisasmWindow = new DisasmWidget(this, m_pTargetModel, m_pDispatcher);
    m_pMemoryViewWidget = new MemoryViewWidget(this, m_pTargetModel, m_pDispatcher);

    QFont monoFont("Monospace");
    monoFont.setStyleHint(QFont::TypeWriter);
    monoFont.setPointSize(9);
    m_pRegistersTextEdit->setCurrentFont(monoFont);

    layout->addWidget(m_pStartStopButton);
    layout->addWidget(m_pSingleStepButton);
    layout->addWidget(m_pRegistersTextEdit);
    pGroupBox->setLayout(layout);
	setCentralWidget(pGroupBox);

    this->addDockWidget(Qt::BottomDockWidgetArea, m_pDisasmWindow);
    this->addDockWidget(Qt::RightDockWidgetArea, m_pMemoryViewWidget);

	// Listen for target changes
    connect(m_pTargetModel, &TargetModel::startStopChangedSignal, this, &MainWindow::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::registersChangedSignal, this, &MainWindow::registersChangedSlot);
    connect(m_pTargetModel, &TargetModel::connectChangedSignal,   this, &MainWindow::connectChangedSlot);
    connect(m_pTargetModel, &TargetModel::memoryChangedSignal,    this, &MainWindow::memoryChangedSlot);

	// Wire up buttons to actions
    connect(m_pStartStopButton, &QAbstractButton::clicked, this, &MainWindow::startStopClicked);
    connect(m_pSingleStepButton, &QAbstractButton::clicked, this, &MainWindow::singleStepClicked);

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

    PopulateRegisters();
}

void MainWindow::startStopChangedSlot()
{
	// Update text here
	if (m_pTargetModel->IsRunning())
	{
		m_pStartStopButton->setText("STOP");
		m_pSingleStepButton->setEnabled(false);	
	}
	else
	{
		// TODO this is where all windows should put in requests for data
        m_pDispatcher->SendCommandPacket("regs");
        m_pDispatcher->SendCommandPacket("bplist");
        m_pDispatcher->RequestMemory(MemorySlot::kMainPC, "pc", "100");

        m_pStartStopButton->setText("START");
		m_pSingleStepButton->setEnabled(true);	
	}
    PopulateRegisters();
}

void MainWindow::registersChangedSlot()
{
	// Update text here
    PopulateRegisters();

    // Update our previous values
    m_prevRegs = m_pTargetModel->GetRegs();
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
    ref << DispReg32(Registers::D0, m_prevRegs, regs) << " " << DispReg32(Registers::A0, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D1, m_prevRegs, regs) << " " << DispReg32(Registers::A1, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D2, m_prevRegs, regs) << " " << DispReg32(Registers::A2, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D3, m_prevRegs, regs) << " " << DispReg32(Registers::A3, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D4, m_prevRegs, regs) << " " << DispReg32(Registers::A4, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D5, m_prevRegs, regs) << " " << DispReg32(Registers::A5, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D6, m_prevRegs, regs) << " " << DispReg32(Registers::A6, m_prevRegs, regs) << "<br>";
    ref << DispReg32(Registers::D7, m_prevRegs, regs) << " " << DispReg32(Registers::A7, m_prevRegs, regs) << "<br>";


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

