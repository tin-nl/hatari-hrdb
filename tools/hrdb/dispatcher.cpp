#include "dispatcher.h"
#include <QtWidgets>
#include <QtNetwork>

#include <iostream>

#include "targetmodel.h"

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
class StringSplitter
{
public:
	explicit StringSplitter(const std::string& str) :
		m_str(str),
		m_pos(0)
	{
	}

	std::string Split(const char c)
	{
		if (m_pos == std::string::npos)
			return "";

		// Skip this char at the start
		//while (m_pos < m_str.size() && m_str[m_pos] == c)
		//	++m_pos;

		if (m_pos == m_str.size())
			return "";

		std::size_t start = m_pos;
		m_pos = m_str.find(c, m_pos);
		std::size_t endpos = m_pos;

		if (m_pos == std::string::npos)
			m_pos = endpos = m_str.size();
		else
		{
			// Skip any extra occurences of the char
			while (m_pos < m_str.size() && m_str[m_pos] == c)
				++m_pos;
		}
		

		return m_str.substr(start, endpos - start);
	}

private:
	const std::string&	m_str;
	std::size_t			m_pos;
};

//-----------------------------------------------------------------------------
Dispatcher::Dispatcher(QTcpSocket* tcpSocket, TargetModel* pTargetModel) :
	m_pTcpSocket(tcpSocket),
	m_pTargetModel(pTargetModel)
{
	connect(m_pTcpSocket, &QAbstractSocket::connected, this, &Dispatcher::connected);
	connect(m_pTcpSocket, &QAbstractSocket::readyRead, this, &Dispatcher::readyRead);
}

Dispatcher::~Dispatcher()
{
	// NO CHECK delete all pending commands
}

void Dispatcher::SendCommandPacket(const char *command)
{
	// NO CHECK ensure that the packet was sent before adding to the pending list...
	RemoteCommand* pNewCmd = new RemoteCommand();
	pNewCmd->m_cmd = std::string(command);
	m_sentCommands.push_front(pNewCmd);
	m_pTcpSocket->write(command, strlen(command) + 1);
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
	// THIS HAPPENS ON THE EVENT LOOP
	printf("Host connected\n");
	this->SendCommandPacket("status");
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

/*
 * stringstream ss( "1,1,1,1, or something else ,1,1,1,0" );
vector<string> result;

while( ss.good() )
{
    string substr;
    getline( ss, substr, ',' );
    result.push_back( substr );
}*/

void Dispatcher::ReceiveResponsePacket(const RemoteCommand& cmd)
{
	std::cout << "REPONSE:" << cmd.m_cmd << "//" << cmd.m_response << std::endl;

	// Our handling depends on the original command type
	// e.g. "break"
	StringSplitter splitCmd(cmd.m_cmd);
	std::string type = splitCmd.Split(' ');

	StringSplitter splitResp(cmd.m_response);

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
}

void Dispatcher::ReceiveNotification(const RemoteNotification& cmd)
{
	std::cout << "NOTIFICATION:" << cmd.m_payload << std::endl;
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

