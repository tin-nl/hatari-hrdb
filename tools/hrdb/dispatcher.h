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
    void SendCommandPacket(const char* command);

    // Request a specific memory block
    void RequestMemory(MemorySlot slot, uint32_t address, uint32_t size);

private slots:

   void connected();
   void disconnected();

   // Called by the socket class to process incoming messages
   void readyRead();

private:

	void ReceiveResponsePacket(const RemoteCommand& command);
	void ReceiveNotification(const RemoteNotification& notification);
    void ReceivePacket(const char* response);

	std::deque<RemoteCommand*>		m_sentCommands;
	QTcpSocket*						m_pTcpSocket;
	TargetModel*					m_pTargetModel;

	std::string 					m_active_resp;
};

#endif // DISPATCHER_H
