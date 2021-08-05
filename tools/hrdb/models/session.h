#ifndef SESSION_H
#define SESSION_H

class QTcpSocket;

// Shared runtime data about the debugging session used by multiple UI components
// This data isn't persisted over runs (that is saved in Settings)
class Session
{
public:
    Session();
    ~Session();
    void Connect();
    void Disconnect();

    QTcpSocket*      m_pTcpSocket;
};

#endif // SESSION_H
