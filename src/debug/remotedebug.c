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

#include "m68000.h"

#define REMOTE_DEBUG_PORT          (1667)
#define REMOTE_DEBUG_CMD_MAX_SIZE  (300)

typedef struct RemoteDebugState
{
	int SocketFD;								/* handle for the port/socket */
	int AcceptedFD;								/* handle for the accepted connection from client, or -1 */
	char cmd_buf[REMOTE_DEBUG_CMD_MAX_SIZE+1];	/* accumulated command string */
	int cmd_pos;								/* offset in cmd_buf for new data */
} RemoteDebugState;

static RemoteDebugState g_rdbState;

static void RemoteDebugState_Init(RemoteDebugState* state)
{
	state->SocketFD = -1;
	state->AcceptedFD = -1;
	memset(state->cmd_buf, 0, sizeof(state->cmd_buf));
	state->cmd_pos = 0;
}

/*
	Create a socket for the port and start to listen over TCP
*/
static int RemoteDebugState_InitServer(RemoteDebugState* state)
{
	// Create listening socket on port
	struct sockaddr_in sa;
	state->SocketFD = socket(PF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (state->SocketFD == -1) {
		fprintf(stderr, "Failed to open socket\n");
		return 1;
	}
  
	memset(&sa, 0, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = htons(REMOTE_DEBUG_PORT);
	sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(state->SocketFD,(struct sockaddr *)&sa, sizeof sa) == -1) {
		fprintf(stderr, "Failed to bind socket\n");
		close(state->SocketFD);
		state->SocketFD =-1;
		return 1;
	}
  
	if (listen(state->SocketFD, 1) == -1) {
		fprintf(stderr, "Failed to listen() on socket\n");
		close(state->SocketFD);
		state->SocketFD =-1;
		return 1;
	}

	// Socket is now in a listening state and could accept 
	printf("Remote Debug Listening on port %d\n", REMOTE_DEBUG_PORT);
	return 0;
}

static void RemoteDebugState_Update(RemoteDebugState* state)
{
	if (state->SocketFD == -1)
		return;

	int remaining;

	if (state->AcceptedFD != -1)
	{
		// Connection is active
		// Read input and accumulate a command
		remaining = REMOTE_DEBUG_CMD_MAX_SIZE - state->cmd_pos;
		int bytes = recv(state->AcceptedFD, 
			&state->cmd_buf[state->cmd_pos],
			remaining,
			MSG_DONTWAIT);

		if (bytes > 0)
		{
			// New data. Is there a command in there (null-terminated string)
			state->cmd_pos += bytes;
			while (1)
			{
				// Scan for a complete command
				char* endptr = memchr(state->cmd_buf, 0, state->cmd_pos);
				if (!endptr)
					break;
				int length = endptr - state->cmd_buf;

				const char* pCmd = state->cmd_buf;
				//printf("Hatari received command: %s\n", pCmd);

				// Process this command
				//usleep(200);

				// Post it back
				char tmp[200];
				sprintf(tmp, "OK:%.10s:%x", pCmd, regs.pc);
				send(state->AcceptedFD, tmp, strlen(tmp) + 1, 0);

				// Copy extra bytes to the start
				// -1 here is for the terminator
				int extra_length = state->cmd_pos - length - 1;
				memcpy(state->cmd_buf, endptr + 1, extra_length);
				state->cmd_pos = extra_length;
			}
		}
		else if (bytes == 0)
		{
			// This represents an orderly EOF
			printf("Remote Debug connection closed\n");
			close(state->AcceptedFD);
			state->AcceptedFD = -1;
		}
	}
	else
	{
		// Active accepted socket
		state->AcceptedFD = accept(state->SocketFD, NULL, NULL);
		if (state->AcceptedFD != -1)
		{
			printf("Remote Debug connection accepted\n");
		}
	}
}

void RemoteDebug_Init(void)
{
	printf("Starting remote debug\n");
	RemoteDebugState_Init(&g_rdbState);

	RemoteDebugState_InitServer(&g_rdbState);
}

void RemoteDebug_Update(void)
{
	RemoteDebugState_Update(&g_rdbState);
}
