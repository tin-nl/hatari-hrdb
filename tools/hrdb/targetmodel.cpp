#include <iostream>
#include "targetmodel.h"

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
}

TargetModel::~TargetModel()
{
    for (int i = 0; i < MemorySlot::kMemorySlotCount; ++i)
        delete m_pTestMemory[i];
}

void TargetModel::SetConnected(int connected)
{
    m_bConnected = connected;
    emit connectChangedSignal();
}

void TargetModel::SetStatus(int running, uint32_t pc)
{
	m_bRunning = running;
	m_pc = pc;
    emit startStopChangedSignal();
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
