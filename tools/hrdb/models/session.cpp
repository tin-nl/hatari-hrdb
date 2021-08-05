#include "session.h"
#include <QtNetwork>


Session::Session()
{
    // Create the core data models, since other object want to connect to them.
    m_pTcpSocket = new QTcpSocket();
}

Session::~Session()
{
    delete m_pTcpSocket;
}



void Session::Connect()
{
    // Create the TCP socket and start listening
    QHostAddress qha(QHostAddress::LocalHost);
    m_pTcpSocket->connectToHost(qha, 56001);
}

void Session::Disconnect()
{
    m_pTcpSocket->disconnectFromHost();
}
