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
#include "exceptionmask.h"

class QTimer;

class TargetModel : public QObject
{
	Q_OBJECT
public:
	TargetModel();
	virtual ~TargetModel();

    // These are called by the Dispatcher when notifications/events arrive
    void SetConnected(int running);
    void SetStatus(int running, uint32_t pc);

    // These are called by the Dispatcher when responses arrive
    void SetRegisters(const Registers& regs, uint64_t commandId);
    void SetMemory(MemorySlot slot, const Memory* pMem, uint64_t commandId);
    void SetBreakpoints(const Breakpoints& bps, uint64_t commandId);
    void SetSymbolTable(const SymbolTable& syms, uint64_t commandId);
    void SetExceptionMask(const ExceptionMask& mask);
    void NotifyMemoryChanged(uint32_t address, uint32_t size);

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
    const ExceptionMask& GetExceptionMask() const { return m_exceptionMask; }

public slots:

signals:
    // connect/disconnect change
    void connectChangedSignal();

    // When start/stop status is changed
    void startStopChangedSignal();

    void startStopChangedSignalDelayed(int running);

	// When new CPU registers are changed
    void registersChangedSignal(uint64_t commandId);

	// When a block of fetched memory is changed
	// TODO: don't have every view listening to the same slot?
    void memoryChangedSignal(int memorySlot, uint64_t commandId);

    void breakpointsChangedSignal(uint64_t commandId);
    void symbolTableChangedSignal(uint64_t commandId);
    void exceptionMaskChanged();

    // UI BODGE
    // Qt seems to have no central message dispatch
    void addressRequested(int windowId, bool isMemory, uint32_t address);

    // Something edited memory
    void otherMemoryChanged(uint32_t address, uint32_t size);

private slots:

    // Called shortly after stop notification received
    void delayedTimer();

private:

    int         m_bConnected;   // 0 == disconnected, 1 == connected
	int			m_bRunning;		// 0 == stopped, 1 == running
	uint32_t	m_pc;			// PC register (for next instruction)

	Registers	m_regs;			// Current register values
    const Memory*	m_pTestMemory[MemorySlot::kMemorySlotCount];
    Breakpoints m_breakpoints;  // Current breakpoint list

    SymbolTable m_symbolTable;

    ExceptionMask   m_exceptionMask;

    QTimer*     m_pTimer;
};

#endif // TARGET_MODEL_H