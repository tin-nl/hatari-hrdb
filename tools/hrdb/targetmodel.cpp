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
	NULL
};

Registers::Registers()
{
	for (int i = 0; i < Registers::REG_COUNT; ++i)
		m_value[i] = 0;
}

TargetModel::TargetModel() :
	QObject(),
	m_bRunning(true)
{

}

TargetModel::~TargetModel()
{

}

void TargetModel::SetStatus(int running, uint32_t pc)
{
	m_bRunning = running;
	m_pc = pc;
	emit startStopChangedSlot();
}

void TargetModel::SetRegisters(const Registers& regs)
{
	m_regs = regs;
	emit registersChangedSlot();
}
