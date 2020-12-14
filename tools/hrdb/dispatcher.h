#ifndef DISPATCHER_H
#define DISPATCHER_H

#include <string>
#include <deque>
#include "remotecommand.h"
#include <QObject>

class QTcpSocket;
class TargetModel;

// Keeps track of messages between target and host, and matches up commands to responses,
// then passes them to the model.
class Dispatcher : public QObject
{
public:
    Dispatcher(QTcpSocket* tcpSocket, TargetModel* pTargetModel);
    virtual ~Dispatcher();

    // TODO replace with strings
    uint64_t SendCommandPacket(const char* command);

    // Request a specific memory block.
    // Allows strings so expressions can evaluate
    uint64_t RequestMemory(MemorySlot slot, uint32_t address, uint32_t size);

    uint64_t RunToPC(uint32_t pc);

    uint64_t SetBreakpoint(std::string expression);

private slots:

   void connected();
   void disconnected();

   // Called by the socket class to process incoming messages
   void readyRead();

private:
    uint64_t SendCommandShared(MemorySlot slot, std::string command);

	void ReceiveResponsePacket(const RemoteCommand& command);
	void ReceiveNotification(const RemoteNotification& notification);
    void ReceivePacket(const char* response);

	std::deque<RemoteCommand*>		m_sentCommands;
	QTcpSocket*						m_pTcpSocket;
	TargetModel*					m_pTargetModel;

	std::string 					m_active_resp;
    uint64_t                        m_responseUid;
};

#endif // DISPATCHER_H
