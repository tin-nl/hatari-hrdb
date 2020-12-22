#include "targetmodel.h"

#include <iostream>
#include <QTimer>

//-----------------------------------------------------------------------------
const char* Registers::s_names[] =
{
	"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
	"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
	"PC", "SR", 
	"USP", "ISP",
	"EX",
    nullptr
};

Registers::Registers()
{
	for (int i = 0; i < Registers::REG_COUNT; ++i)
		m_value[i] = 0;
}

TargetModel::TargetModel() :
	QObject(),
    m_bConnected(false),
    m_bRunning(true)
{
    for (int i = 0; i < MemorySlot::kMemorySlotCount; ++i)
        m_pTestMemory[i] = nullptr;

    m_pTimer = new QTimer(this);
    connect(m_pTimer, &QTimer::timeout, this, &TargetModel::delayedTimer);
}

TargetModel::~TargetModel()
{
    for (int i = 0; i < MemorySlot::kMemorySlotCount; ++i)
        delete m_pTestMemory[i];

    delete m_pTimer;
}

void TargetModel::SetConnected(int connected)
{
    m_bConnected = connected;

    if (connected == 0)
    {
        // Clear out lots of data from the model
        SymbolTable dummy;
        SetSymbolTable(dummy, 0);

        Breakpoints dummyBreak;
        SetBreakpoints(dummyBreak, 0);
    }

    emit connectChangedSignal();
}

void TargetModel::SetStatus(int running, uint32_t pc)
{
	m_bRunning = running;
	m_pc = pc;
    emit startStopChangedSignal();

    m_pTimer->stop();

    //    if (!m_bRunning)
    {
        m_pTimer->setSingleShot(true);
        m_pTimer->start(500);
    }

}

void TargetModel::SetRegisters(const Registers& regs, uint64_t commandId)
{
	m_regs = regs;
    emit registersChangedSignal(commandId);
}

void TargetModel::SetMemory(MemorySlot slot, const Memory* pMem, uint64_t commandId)
{
    if (m_pTestMemory[slot])
        delete m_pTestMemory[slot];

    m_pTestMemory[slot] = pMem;
    emit memoryChangedSignal(slot, commandId);
}

void TargetModel::SetBreakpoints(const Breakpoints& bps, uint64_t commandId)
{
    m_breakpoints = bps;
    emit breakpointsChangedSignal(commandId);
}

void TargetModel::SetSymbolTable(const SymbolTable& syms, uint64_t commandId)
{
    m_symbolTable = syms;
    emit symbolTableChangedSignal(commandId);
}

void TargetModel::SetExceptionMask(const ExceptionMask &mask)
{
    m_exceptionMask = mask;
    emit exceptionMaskChanged();
}

void TargetModel::delayedTimer()
{
    m_pTimer->stop();
    emit startStopChangedSignalDelayed(m_bRunning);
}
