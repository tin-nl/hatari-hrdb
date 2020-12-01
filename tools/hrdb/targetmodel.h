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

struct Registers
{
	Registers();
	enum
	{
		D0 = 0,
		D1,
		D2,
		D3,
		D4,
		D5,
		D6,
		D7,
		A0,
		A1,
		A2,
		A3,
		A4,
		A5,
		A6,
		A7,
		PC,
		SR,
		USP,
		ISP,
		EX,			// Exception number
		REG_COUNT
	};
	uint32_t	m_value[REG_COUNT];
	// Null-terminated 1:1 array of register names
	static const char* s_names[];
};

class TargetModel : public QObject
{
	Q_OBJECT
public:
	TargetModel();
	virtual ~TargetModel();

	void SetStatus(int running, uint32_t pc);
	void SetRegisters(const Registers& regs);

	// NOTE: all these return copies to avoid data contention
	int IsRunning() const { return m_bRunning; }
	uint32_t GetPC() const { return m_pc; }
	Registers GetRegs() const { return m_regs; }

public slots:

signals:
	// When start/stop status is changed
    void startStopChangedSlot();
	// When new CPU registers are changed
    void registersChangedSlot();

private:

	int			m_bRunning;		// 0 == stopped, 1 == running
	uint32_t	m_pc;			// PC register (for next instruction)

	Registers	m_regs;			// Current register values
};

#endif // TARGET_MODEL_H
