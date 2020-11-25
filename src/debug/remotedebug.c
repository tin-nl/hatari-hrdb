/*
 * Hatari - remote.c
 * 
 * This file is distributed under the GNU General Public License, version 2
 * or at your option any later version. Read the file gpl.txt for details.
 * 
 * Remote debugging support via a network port.
 * 
 */

#include "remotedebug.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define REMOTE_DEBUG_PORT       (1667)
int SocketFD = -1;
int AcceptedFD = -1;

/*
static void set_nonblock(int socket)
{
    int flags;
    flags = fcntl(socket,F_GETFL,0);
    assert(flags != -1);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}
*/

static int RemoteDebug_InitServer(void)
{
	// Create listening socket on port
	struct sockaddr_in sa;
	SocketFD = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (SocketFD == -1) {
		perror("cannot create socket");
		return 1;
	}
  
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(REMOTE_DEBUG_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(SocketFD,(struct sockaddr *)&sa, sizeof sa) == -1) {
	  perror("bind failed");
	  close(SocketFD);
	  return 1;
	}
  
	if (listen(SocketFD, 10) == -1) {
	  perror("listen failed");
	  close(SocketFD);
	  return 1;
	}

	// Socket is now in a listening state and could accept 
	printf("Listening...\n");
	return 0;
}


void RemoteDebug_Init(void)
{
	printf("Starting remote debug\n");
	RemoteDebug_InitServer();
}

void RemoteDebug_Update(void)
{
	char buf[101];
	if (AcceptedFD != -1)
	{
		printf("*\n");
		// Read input
		int bytes = recv(AcceptedFD, buf, 100, MSG_DONTWAIT);
		if (bytes > 0)
		{
			buf[bytes] = 0;
			printf("Rec **%s**\n", buf);
		}
	}
	else
	{
		// Active accepted socket
		AcceptedFD = accept(SocketFD, NULL, NULL);
		if (AcceptedFD == -1) {
			return;
		}
		printf("Accepted:%d\n", AcceptedFD);
	}
}
