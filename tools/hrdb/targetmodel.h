#ifndef TARGET_MODEL_H
#define TARGET_MODEL_H

/*
	Core central data model reflecting the state of the target.

	May contain sub-models later, to persist the data relating to particular views.
	(e.g. memory window, )

*/

#include <stdint.h>
#include <QObject>

#include "remotecommand.h"
#include "breakpoint.h"
#include "memory.h"
#include "symboltable.h"
#include "registers.h"

class TargetModel : public QObject
{
	Q_OBJECT
public:
	TargetModel();
	virtual ~TargetModel();

    void SetConnected(int running);
    void SetStatus(int running, uint32_t pc);
	void SetRegisters(const Registers& regs);
    void SetMemory(MemorySlot slot, const Memory* pMem);
    void SetBreakpoints(const Breakpoints& bps);
    void SetSymbolTable(const SymbolTable& syms);

	// NOTE: all these return copies to avoid data contention
    int IsConnected() const { return m_bConnected; }
    int IsRunning() const { return m_bRunning; }
	uint32_t GetPC() const { return m_pc; }
	Registers GetRegs() const { return m_regs; }
    const Memory* GetMemory(MemorySlot slot) const
    {
        return m_pTestMemory[slot];
    }
    const Breakpoints& GetBreakpoints() const { return m_breakpoints; }
    const SymbolTable& GetSymbolTable() const { return m_symbolTable; }

public slots:

signals:
    // connect/disconnect change
    void connectChangedSignal();

    // When start/stop status is changed
    void startStopChangedSignal();
	// When new CPU registers are changed
    void registersChangedSignal();

	// When a block of fetched memory is changed
	// TODO: don't have every view listening to the same slot?
    void memoryChangedSignal(int memorySlot);

    void breakpointsChangedSignal();
    void symbolTableChangedSignal();

private:

    int         m_bConnected;   // 0 == disconnected, 1 == connected
	int			m_bRunning;		// 0 == stopped, 1 == running
	uint32_t	m_pc;			// PC register (for next instruction)

	Registers	m_regs;			// Current register values
    const Memory*	m_pTestMemory[MemorySlot::kMemorySlotCount];
    Breakpoints m_breakpoints;  // Current breakpoint list

    SymbolTable m_symbolTable;
};

#endif // TARGET_MODEL_H
