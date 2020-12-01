#include "mainwindow.h"

#include <iostream>
#include <QtWidgets>
#include <QtNetwork>
#include <QShortcut>

#include "dispatcher.h"
#include "targetmodel.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tcpSocket(new QTcpSocket(this))
{
	// https://doc.qt.io/qt-5/qtwidgets-layouts-basiclayouts-example.html

	auto pGroupBox = new QGroupBox(this);
	//pGroupBox->size().setHeight(500);
    QVBoxLayout *layout = new QVBoxLayout;
    m_pStartStopButton = new QPushButton("STOP", pGroupBox);
    m_pSingleStepButton = new QPushButton("Step", pGroupBox);
    m_pRegistersTextEdit = new QTextEdit("", pGroupBox);
	m_pRegistersTextEdit->setReadOnly(true);

    layout->addWidget(m_pStartStopButton);
    layout->addWidget(m_pSingleStepButton);
    layout->addWidget(m_pRegistersTextEdit);
    pGroupBox->setLayout(layout);
	setCentralWidget(pGroupBox);

	// Create the core data model
	m_pTargetModel = new TargetModel();

	// Create the TCP socket and start listening
	QHostAddress qha(QHostAddress::LocalHost);
    tcpSocket->connectToHost(qha, 56001);

	m_pDispatcher = new Dispatcher(tcpSocket, m_pTargetModel);

	// Listen for target changes
    connect(m_pTargetModel, &TargetModel::startStopChangedSlot, this, &MainWindow::startStopChangedSlot);
    connect(m_pTargetModel, &TargetModel::registersChangedSlot, this, &MainWindow::registersChangedSlot);

	// Wire up buttons to actions
    connect(m_pStartStopButton, &QAbstractButton::clicked, this, &MainWindow::startStopClicked);
    connect(m_pSingleStepButton, &QAbstractButton::clicked, this, &MainWindow::singleStepClicked);

	// Keyboard shortcuts
	new QShortcut(QKeySequence(tr("F5", "Start/Stop")),
					this,
					SLOT(startStopClicked()));

	new QShortcut(QKeySequence(tr("F10", "Step")),
					this,
					SLOT(singleStepClicked()));
}

MainWindow::~MainWindow()
{
	delete m_pDispatcher;
	delete m_pTargetModel;
}

void MainWindow::startStopChangedSlot()
{
	// Update text here
	if (m_pTargetModel->IsRunning())
		m_pStartStopButton->setText("STOP");
	else
	{
		// TODO this is where all windows should put in requests for data
		m_pDispatcher->SendCommandPacket("regs");
		m_pStartStopButton->setText("START");	
	}
}

void MainWindow::registersChangedSlot()
{
	// Update text here
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


void MainWindow::PopulateRegisters()
{
	if (m_pTargetModel->IsRunning())
	{
		return;
	}

	// Build up the text area
	QString regsText;
	Registers regs = m_pTargetModel->GetRegs();
	for (int i = 0; i < Registers::REG_COUNT; ++i)
	{
		QString pc_text;
		pc_text = QString::asprintf("%s: %08x\n", Registers::s_names[i], regs.m_value[i]);
		regsText += pc_text;
	}

	m_pRegistersTextEdit->setAcceptRichText(false);
	m_pRegistersTextEdit->setPlainText(regsText);
}
