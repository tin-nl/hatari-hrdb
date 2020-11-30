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
#include "main.h"		/* For ARRAY_SIZE */
#include "debugui.h"	/* For DebugUI_RegisterRemoteDebug */

#define REMOTE_DEBUG_PORT          (1667)
#define REMOTE_DEBUG_CMD_MAX_SIZE  (300)

/* Remote debugging break command was sent from debugger */
static bool bRemoteBreakRequest = false;

/* Processing is stopped and the remote debug loop is active */
static bool bRemoteBreakIsActive = false;

// -----------------------------------------------------------------------------
static void send_string(int fd, const char* pStr)
{
	send(fd, pStr, strlen(pStr), 0);
}

// -----------------------------------------------------------------------------
// Send the out-of-band status to flag start/stop
static int RemoteDebug_NotifyState(int fd)
{
	char tmp[100];
	int len = sprintf(tmp, "!status %x %x", bRemoteBreakIsActive ? 0 : 1, M68000_GetPC());
	// +1 for the terminator
	send(fd, tmp, len + 1, 0);
	return 0;
}

// -----------------------------------------------------------------------------
/* Return short status info in a useful format, mainly whether it's running */
static int RemoteDebug_Status(int nArgc, char *psArgs[], int fd)
{
	char tmp[100];
	int len = sprintf(tmp, "status %x %x", bRemoteBreakIsActive ? 0 : 1, M68000_GetPC());
	send(fd, tmp, len, 0);
	return 0;
}

// -----------------------------------------------------------------------------
/* Put in a break request which is serviced elsewhere in the main loop */
static int RemoteDebug_Break(int nArgc, char *psArgs[], int fd)
{
	// Only set a break request if we are running
	if (!bRemoteBreakIsActive)
	{
		bRemoteBreakRequest = true;
		send_string(fd, "OK");
	}
	else
	{
		send_string(fd, "NG");
	}
	
	return 0;
}

// -----------------------------------------------------------------------------
/* Step next instruction. This is currently a passthrough to the normal debugui code. */
static int RemoteDebug_Step(int nArgc, char *psArgs[], int fd)
{
	send_string(fd, "OK");
	bRemoteBreakIsActive = false;
	return 0;
}

// -----------------------------------------------------------------------------
/* Step next instruction. This is currently a passthrough to the normal debugui code. */
static int RemoteDebug_Next(int nArgc, char *psArgs[], int fd)
{
	send_string(fd, "OK");
	bRemoteBreakIsActive = false;
	return 0;
}

// -----------------------------------------------------------------------------
static int RemoteDebug_Run(int nArgc, char *psArgs[], int fd)
{
	send_string(fd, "OK");
	bRemoteBreakIsActive = false;
	return 0;
}

// -----------------------------------------------------------------------------
/* DebugUI command structure */
typedef struct
{
	int (*pFunction)(int argc, char *argv[], int fd);
	const char *sName;
} rdbcommand_t;

/* Array of all remote debug command descriptors */
static const rdbcommand_t remoteDebugCommandList[] = {
	{ RemoteDebug_Status,   "status"	},
	{ RemoteDebug_Break,    "break"		},
	{ RemoteDebug_Run, 		"run"		},
	{ RemoteDebug_Step,     "step"		},
	{ RemoteDebug_Next,     "next"		},

	/* Terminator */
	{ NULL, NULL }
};

/**
 * Parse remote debug command and execute it.
 * Command should return 0 if successful.
 * Returns -1 if command not parsed
 */
static int RemoteDebug_Parse(const char *input_orig, int fd)
{
	char *psArgs[64], *input;
	const char *delim;
	int nArgc = -1;
	int retval;

	input = strdup(input_orig);
	psArgs[0] = strtok(input, " \t");

	/* Search the command ... */
	const rdbcommand_t* pCommand = remoteDebugCommandList;
	while (pCommand->pFunction)
	{
		if (!strcmp(psArgs[0], pCommand->sName))
			break;
		++pCommand;
	}
	if (!pCommand->pFunction)
	{
		free(input);
		return -1;
	}

	delim = " \t";

	/* Separate arguments and put the pointers into psArgs */
	for (nArgc = 1; nArgc < ARRAY_SIZE(psArgs); nArgc++)
	{
		psArgs[nArgc] = strtok(NULL, delim);
		if (psArgs[nArgc] == NULL)
			break;
		//printf("*ARGS**** %s ***\n", psArgs[nArgc]); 	// NO CHECK
	}
	if (nArgc >= ARRAY_SIZE(psArgs))
	{
		retval = -1;
	}
	else
	{
		/* ... and execute the function */
		printf("Execute: %s\n", pCommand->sName);
		retval = pCommand->pFunction(nArgc, psArgs, fd);
	}
	free(input);
	return retval;
}


// -----------------------------------------------------------------------------

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

/* Process any command data that has been read into the pending
	command buffer, and execute them.
*/
static void RemoteDebug_ProcessBuffer(RemoteDebugState* state)
{
	int cmd_ret;
	while (1)
	{
		// Scan for a complete command
		char* endptr = memchr(state->cmd_buf, 0, state->cmd_pos);
		if (!endptr)
			break;

		int length = endptr - state->cmd_buf;

		const char* pCmd = state->cmd_buf;

		// Process this command
		printf("Received: %s\n", pCmd);
		cmd_ret = RemoteDebug_Parse(pCmd, state->AcceptedFD);

		// TODO return an error over the network

		// Write packet terminator
		char terminator;
		terminator = 0;
		send(state->AcceptedFD, &terminator, 1, 0);

		// Copy extra bytes to the start
		// -1 here is for the terminator
		int extra_length = state->cmd_pos - length - 1;
		memcpy(state->cmd_buf, endptr + 1, extra_length);
		state->cmd_pos = extra_length;
	}
}

/*
	Listen to the connection for remote-debug messages,
	some of which will release us from the break loop
*/
static bool RemoteDebug_BreakLoop(void)
{
	int remaining;
	RemoteDebugState* state;

	// TODO set socket as blocking
	printf("RemoteDebug_BreakLoop\n");
	state = &g_rdbState;

	// NO CHECK cope with no connection!

	bRemoteBreakIsActive = true;
	// Notify after state change happens
	RemoteDebug_NotifyState(state->AcceptedFD);

	// NO CHECK handle no connection

	while (bRemoteBreakIsActive)
	{
		if (state->AcceptedFD == -1 || 
			state->SocketFD == -1)
		{
			break;
		}

		// Read input and accumulate a command (blocking)
		remaining = REMOTE_DEBUG_CMD_MAX_SIZE - state->cmd_pos;
		int bytes = recv(state->AcceptedFD, 
			&state->cmd_buf[state->cmd_pos],
			remaining,
			0);
		if (bytes > 0)
		{
			// New data. Is there a command in there (null-terminated string)
			state->cmd_pos += bytes;
			RemoteDebug_ProcessBuffer(state);
		}
		else if (bytes == 0)
		{
			// This represents an orderly EOF
			printf("Remote Debug connection closed\n");
			DebugUI_RegisterRemoteDebug(NULL);
			close(state->AcceptedFD);
			state->AcceptedFD = -1;

			// Bail out of the loop here so we don't just spin
			break;
		}
		else
		{
			printf("Unknown cmd\n");
		}
	}
	bRemoteBreakIsActive = false;
	// Clear any break request that might have been set
	bRemoteBreakRequest = false;

	printf("RemoteDebug_CheckUpdates complete, restarting\n");
	RemoteDebug_NotifyState(state->AcceptedFD);
	return true;
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
	{
		// TODO: we should try to re-init periodically
		return;
	}

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
			RemoteDebug_ProcessBuffer(state);
		}
		else if (bytes == 0)
		{
			// This represents an orderly EOF
			printf("Remote Debug connection closed\n");
			DebugUI_RegisterRemoteDebug(NULL);
			close(state->AcceptedFD);
			state->AcceptedFD = -1;
			return;
		}
	}
	else
	{
		// Active accepted socket
		state->AcceptedFD = accept(state->SocketFD, NULL, NULL);
		if (state->AcceptedFD != -1)
		{
			printf("Remote Debug connection accepted\n");
			DebugUI_RegisterRemoteDebug(RemoteDebug_BreakLoop);
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

/**
 * Debugger invocation if requested by remote debugger.
 * 
 * NOTE: we could just run our own loop here, rather than going through DebugUI?
 */
void RemoteDebug_CheckRemoteBreak(void)
{
	if (bRemoteBreakRequest)
	{
		bRemoteBreakRequest = false;
		// Stop and wait for inputs from the control socket
		DebugUI(REASON_USER);
	}
}
