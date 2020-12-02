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

// A block of memory pulled from the target.
struct Memory
{
public:
	Memory(uint32_t addr, uint32_t size) :
		m_addr(addr),
		m_size(size)
	{
		m_pData = new uint8_t[size];		
	}

	~Memory()
	{
		delete [] m_pData;
	}

	void Set(uint32_t offset, uint8_t val)
	{
		assert(offset < m_size);
		m_pData[offset] = val;
	}

	uint8_t Get(uint32_t offset) const
	{
		return m_pData[offset];
	}

	uint32_t GetAddress() const
	{
		return m_addr;
	}

	uint32_t GetSize() const
	{
		return m_size;
	}

private:
	Memory(const Memory& other);	// hide to prevent accidental use

	uint32_t	m_addr;
	uint32_t	m_size;		// size in bytes
	uint8_t*	m_pData;
};

class TargetModel : public QObject
{
	Q_OBJECT
public:
	TargetModel();
	virtual ~TargetModel();

	void SetStatus(int running, uint32_t pc);
	void SetRegisters(const Registers& regs);
	void SetMemory(const Memory* pMem);

	// NOTE: all these return copies to avoid data contention
	int IsRunning() const { return m_bRunning; }
	uint32_t GetPC() const { return m_pc; }
	Registers GetRegs() const { return m_regs; }
	// NO CHECK unsafe
	const Memory* GetMemory() const { return m_pTestMemory; }

public slots:

signals:
	// When start/stop status is changed
    void startStopChangedSlot();
	// When new CPU registers are changed
    void registersChangedSlot();

	// When a block of fetched memory is changed
	// TODO: don't have every view listening to the same slot?
    void memoryChangedSlot();

private:

	int			m_bRunning;		// 0 == stopped, 1 == running
	uint32_t	m_pc;			// PC register (for next instruction)

	Registers	m_regs;			// Current register values

	const Memory*	m_pTestMemory;	// TODO handle many types of these
};

#endif // TARGET_MODEL_H
