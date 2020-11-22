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
#include <netinet/in.h>
#include <arpa/inet.h>

#define REMOTE_DEBUG_PORT       (1667)

static int Remote_InitServer(void)
{
	// Create listening socket on port
	struct sockaddr_in sa;
	int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
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

	printf("Listening...\n");
	return 0;
}


void Remote_Init(void)
{
	printf("Starting remote debug\n");
	Remote_InitServer();
}
