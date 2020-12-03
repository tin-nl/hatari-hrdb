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

    void SendCommandPacket(const char* command);
	void ReceivePacket(const char* response);

private slots:

   void connected();
   void disconnected();
   void readyRead();

private:

	void ReceiveResponsePacket(const RemoteCommand& command);
	void ReceiveNotification(const RemoteNotification& notification);

	std::deque<RemoteCommand*>		m_sentCommands;
	QTcpSocket*						m_pTcpSocket;
	TargetModel*					m_pTargetModel;

	std::string 					m_active_resp;
};

#endif // DISPATCHER_H
