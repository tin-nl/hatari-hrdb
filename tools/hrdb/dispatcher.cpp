#include "dispatcher.h"
#include <QtWidgets>
#include <QtNetwork>

#include <iostream>

#include "targetmodel.h"
#include "stringsplitter.h"

//#define DISPATCHER_DEBUG

//-----------------------------------------------------------------------------
int RegNameToEnum(const char* name)
{
	const char** pCurrName = Registers::s_names;
	int i = 0;
	while (*pCurrName)
	{
		if (strcmp(name, *pCurrName) == 0)
			return i;
		++pCurrName;
		++i;
	}
	return Registers::REG_COUNT;
}

//-----------------------------------------------------------------------------
uint8_t charToHexNybble(char c)
{
	if (c >= '0' && c <= '9')
		return (uint8_t)(c - '0');
	if (c >= 'A' && c <= 'F')
        return (uint8_t)(10 + c - 'A');
	return 0;
}

//-----------------------------------------------------------------------------
Dispatcher::Dispatcher(QTcpSocket* tcpSocket, TargetModel* pTargetModel) :
	m_pTcpSocket(tcpSocket),
	m_pTargetModel(pTargetModel)
{
	connect(m_pTcpSocket, &QAbstractSocket::connected, this, &Dispatcher::connected);
    connect(m_pTcpSocket, &QAbstractSocket::disconnected, this, &Dispatcher::disconnected);
    connect(m_pTcpSocket, &QAbstractSocket::readyRead, this, &Dispatcher::readyRead);
}

Dispatcher::~Dispatcher()
{
    // NO CHECK delete all pending commands
}

void Dispatcher::RequestMemory(MemorySlot slot, std::string address, std::string size)
{
    std::string command = std::string("mem " + address + " " + size);
    SendCommandShared(slot, command);
}

void Dispatcher::SetBreakpoint(std::string expression)
{
    std::string command = std::string("bp " + expression);
    SendCommandShared(MemorySlot::kNone, command);
    SendCommandShared(MemorySlot::kNone, "bplist"); // update state
}

void Dispatcher::SendCommandPacket(const char *command)
{
    SendCommandShared(MemorySlot::kNone, command);
}

void Dispatcher::ReceivePacket(const char* response)
{
	// THIS HAPPENS ON THE EVENT LOOP
    std::string new_resp(response);

	// Check for a notification
	if (new_resp.size() > 0)
	{
		if (new_resp[0] == '!')
		{
			RemoteNotification notif;
			notif.m_payload = new_resp;
			this->ReceiveNotification(notif);
			return;
		}
	}

	// Find the last "sent" packet
	size_t checkIndex = m_sentCommands.size();
	if (checkIndex != 0)
	{
		// Pair to the last entry
		RemoteCommand* pPending = m_sentCommands[checkIndex - 1];
		pPending->m_response = new_resp;

		m_sentCommands.pop_back();

    	// Add these to a list and process them at a safe time?
    	// Need to understand the Qt threading mechanism
	    //std::cout << "Cmd: " << pPending->m_cmd << ", Response: " << pPending->m_response << "***" << std::endl;

		// At this point we can notify others that new data has arrived
		this->ReceiveResponsePacket(*pPending);
		delete pPending;
	}
	else
	{
		std::cout << "No pending command??" << std::endl;
	}
	
}

void Dispatcher::connected()
{
    // Do this before the UI gets to do its requests
    this->SendCommandPacket("status");
    m_pTargetModel->SetConnected(1);
	// THIS HAPPENS ON THE EVENT LOOP
	printf("Host connected\n");
}

void Dispatcher::disconnected()
{
    m_pTargetModel->SetConnected(0);
    // THIS HAPPENS ON THE EVENT LOOP
    printf("Host disconnected\n");
}

void Dispatcher::readyRead()
{
	// THIS HAPPENS ON THE EVENT LOOP
	qint64 byteCount = m_pTcpSocket->bytesAvailable();
	char* data = new char[byteCount];

	m_pTcpSocket->read(data, byteCount);

	// Read completed commands from this and process in turn
	for (int i = 0; i < byteCount; ++i)
	{
		if (data[i] == 0)
		{
			// End of response
			this->ReceivePacket(m_active_resp.c_str());
			m_active_resp = std::string();
		}
		else
		{
			m_active_resp += data[i];
		}
	}
    delete[] data;
}

void Dispatcher::SendCommandShared(MemorySlot slot, std::string command)
{
    RemoteCommand* pNewCmd = new RemoteCommand();
    pNewCmd->m_cmd = command;
    pNewCmd->m_memorySlot = slot;
    m_sentCommands.push_front(pNewCmd);
    m_pTcpSocket->write(command.c_str(), command.size() + 1);
}

void Dispatcher::ReceiveResponsePacket(const RemoteCommand& cmd)
{
#ifdef DISPATCHER_DEBUG
	std::cout << "REPONSE:" << cmd.m_cmd << "//" << cmd.m_response << std::endl;
#endif

	// Our handling depends on the original command type
	// e.g. "break"
	StringSplitter splitCmd(cmd.m_cmd);
	std::string type = splitCmd.Split(' ');

    StringSplitter splitResp(cmd.m_response);
    std::string cmd_status = splitResp.Split(' ');
    if (cmd_status != std::string("OK"))
    {
        std::cout << "Repsonse dropped: " << cmd.m_response;
        return;
    }

	if (type == "regs")
	{
        Registers regs;
		while (true)
        {

            std::string reg = splitResp.Split(':');
			if (reg.size() == 0)
				break;
			std::string valueStr = splitResp.Split(' ');
			char* endpr;
			uint32_t value = std::strtol(valueStr.c_str(), &endpr, 16);

			// Write this value into register structure
			int reg_id = RegNameToEnum(reg.c_str());
			if (reg_id != Registers::REG_COUNT)
				regs.m_value[reg_id] = value;
		}
		m_pTargetModel->SetRegisters(regs);
	}
	else if (type == "mem")
	{
		std::string addrStr = splitResp.Split(' ');
		std::string sizeStr = splitResp.Split(' ');
		char* endpr;
		uint32_t addr = std::strtol(addrStr.c_str(), &endpr, 16);
		uint32_t size = std::strtol(sizeStr.c_str(), &endpr, 16);
		
		// Create a new memory block to pass to the data model
		Memory* pMem = new Memory(addr, size);

		// Now parse the hex data
		int readPos = splitResp.GetPos();
		for (uint32_t off = 0; off < size; ++off)
		{
			uint8_t nybbleHigh = charToHexNybble(cmd.m_response[readPos++]);
			uint8_t nybbleLow = charToHexNybble(cmd.m_response[readPos++]);

			uint8_t byte = (nybbleHigh << 4) | nybbleLow;
			pMem->Set(off, byte);
		}

        m_pTargetModel->SetMemory(cmd.m_memorySlot, pMem);
	}
    else if (type == "bplist")
    {
        // Breakpoints
        std::string countStr = splitResp.Split(' ');
        char* endpr;
        uint32_t count = std::strtol(countStr.c_str(), &endpr, 16);

        Breakpoints bps;
        for (uint32_t i = 0; i < count; ++i)
        {
            Breakpoint bp;
            bp.SetExpression(splitResp.Split('`'));
            std::string ccount = splitResp.Split(' ');
            std::string hits = splitResp.Split(' ');
            bps.m_breakpoints.push_back(bp);
        }

        m_pTargetModel->SetBreakpoints(bps);
    }
}

void Dispatcher::ReceiveNotification(const RemoteNotification& cmd)
{
#ifdef DISPATCHER_DEBUG
	std::cout << "NOTIFICATION:" << cmd.m_payload << std::endl;
#endif
	StringSplitter s(cmd.m_payload);

	std::string type = s.Split(' ');
	if (type == "!status")
	{
		std::string runningStr = s.Split(' ');
		std::string pcStr = s.Split(' ');

		char* endpr;
		int running = std::strtol(runningStr.c_str(), &endpr, 16);
		int pc = std::strtol(pcStr.c_str(), &endpr, 16);
		m_pTargetModel->SetStatus(running, pc);
	}
}

