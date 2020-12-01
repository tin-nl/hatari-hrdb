#ifndef REMOTECOMMAND_H
#define REMOTECOMMAND_H

#include <string>

class RemoteCommand
{
public:
	std::string		m_cmd;
	std::string		m_response;		// filled out by receiver thread
};

class RemoteNotification
{
public:
	std::string		m_payload;
};


#endif // REMOTECOMMAND_H
