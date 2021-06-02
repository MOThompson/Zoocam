/* Server side common routines in client/server code */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>				  /* for defining several useful types and macros */
#include <stdio.h>				  /* for performing input and output */
#include <stdlib.h>				  /* for performing a variety of operations */
#include <string.h>
#include <math.h>               /* basic math functions */
#include <assert.h>
#include <stdint.h>             /* C99 extension to get known width integers */
#include <signal.h>

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "server_support.h"		/* For prototypes - includes system includes */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif

#define	CLIENT_MUTEX_WAIT	(30000)		/* 30 second time-out */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static uint32_t CRC32(void *buffer, int count);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
int DebugLevel = 2;									/* All errors by default */

/* ===========================================================================
-- Routine to start a server listening on specific port
--
-- Usage: int DebugSockets(int level);
--
-- Inputs: level - debug noise level
--            0 ==> only fatal errors
--            1 ==> all errors 
--            2 ==> all warnings
--            3 ==> verbose
--
-- Output: Sets internal flag
--
-- Return: previous level
=========================================================================== */
int DebugSockets(int level) {
	int rc;

	rc = DebugLevel;
	if (level < 0) level = 0;
	if (level > 3) level = 3;
	DebugLevel = level;
	return rc;
}

/* ===========================================================================
-- Routine to start a server listening on specific port
--
-- Usage: int RunServer      (char *name, unsigned short port, void (*ServerHandler)(void *), void (*reset)(void));
--        int RunServerThread(char *name, unsigned short port, void (*ServerHandler)(void *), void (*reset)(void));
--
-- Inputs: name                  - descriptive name of the server (LasGo, DCx, Spec, Focus, ...)
--         port                  - port to listen on
--         ServerHandler(SOCKET) - routine called when a client connects to the port
--         reset(void)           - if !NULL, routine called after socket is closed
--                                 to reset the server to known state
--
-- Output: RunServer() and RunServerThread() are essentially identical except;
--           RunServerThread - spawns a separate thread that calls RunServer and returns immediately
--           RunServer       - waits forever for connections and spawns handlers for each connection
--
-- Return: RunServerThread:
--           0 ==> thread started successfully
--           1 ==> unable to allocate memory
--           2 ==> _beginthread failed
--         RunServer:
--           0 ==> exited by command from a ServerHandler
--           3 ==> Unable to initiate sockets
--           4 ==> Unable to bind the socket
--           5 ==> Unable to listen on the socket
--
-- Notes: (1) The server connects to the port, listens for clients, and then spawns
--            threads running "ClientHandler() to handle each connection
--        (2) The most common mode is that RunServer will never return.  However, the
--	           spawned threads can cause the server to exit if really wanted.  See code.
=========================================================================== */
typedef struct {
	char name[32];
	unsigned short port;
	void (*ClientHandler)(void *);
	void (*reset)(void);
} STUB_SERVER;

static void RunServerStub(void *stub) {
	RunServer( ((STUB_SERVER *)stub)->name, ((STUB_SERVER *)stub)->port, ((STUB_SERVER *)stub)->ClientHandler, ((STUB_SERVER *)stub)->reset);
	free(stub);
	return;
}

int RunServerThread(char *name, unsigned short port, void (*ClientHandler)(void *), void (*reset)(void)) {
	int rc;
	STUB_SERVER *stub;

	if ( (stub = calloc(1, sizeof(*stub))) == NULL) {
		rc = 1;
	} else {
		strcpy_s(stub->name, sizeof(stub->name), name);
		stub->port          = port;
		stub->ClientHandler = ClientHandler;
		stub->reset         = reset;
		rc = (_beginthread( RunServerStub, 0, (void *) stub) != -1L) ? 0 : 2 ;
	}
	return rc;
}

int RunServer(char *pname, unsigned short port, void (*ClientHandler)(void *), void (*reset)(void)) {

	SOCKET m_socket;					/* Port listening socket */
	SOCKET c_socket;					/* Client work socket */
	SOCKADDR_IN service;
	SERVER_DATA_BLOCK *block;
	sig_atomic_t thread_count;
	char name[32];

/* Copy over the name since we will be here a while and don't want it changed out from under us */
	strcpy_s(name, sizeof(name), pname);

/* Make sure we can inititiate and have sockets available */
	if (InitSockets() != 0) return 3;

/* Create a socket to listen for clients */
	if ( (m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET ) {
		fprintf(stderr, "TCP %s server: Error creating socket(): %ld\n", name, WSAGetLastError() ); fflush(stderr);
		return 3;
	}

/* Bind the socket to all adapters (INADDR_ANY) */
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl( INADDR_ANY );
	service.sin_port = htons( port );
	if ( bind( m_socket, (SOCKADDR*) &service, sizeof(service) ) == SOCKET_ERROR ) {
		fprintf(stderr, "TCP %s server: bind() failed.\n", name); fflush(stderr);
		closesocket(m_socket);
		return 4;
	}

/* Listen on the socket. */
	if ( listen( m_socket, 1 ) == SOCKET_ERROR ) {
		fprintf(stderr, "TCP %s server: Error listening on socket.\n", name); fflush(stderr);
		closesocket(m_socket);
		return 5;
	}

/* Now sit and accept connections until someone says uncle */
	thread_count = 0;
	if (DebugLevel >= 2) { fprintf(stderr, "TCP %s server: Waiting for clients on port %d\n", name, port); fflush(stderr); }
	while (TRUE) {
		c_socket = SOCKET_ERROR;
		while ( c_socket == SOCKET_ERROR ) c_socket = accept( m_socket, NULL, NULL );
		block = calloc(1, sizeof(*block));
		block->socket = c_socket;
		block->thread_count = &thread_count;
		block->reset  = reset;
		if (_beginthread(ClientHandler, 0, block) == -1L) {
			fprintf(stderr, "TCP %s server: Error starting thread for connection on port %d\n", name, port); fflush(stderr);
		} else {
			if (DebugLevel >= 2) { fprintf(stderr, "TCP %s server: Connection on port %d established\n", name, port); fflush(stderr); }
			thread_count++;
		}
	}

	/* Have received somehow a message to end (SERVER_END command or errors) */
	closesocket(m_socket);
	return 0;
}

/* ===========================================================================
-- Routine to be called at the end of every ServerHandler routine to release
-- the connection to the server
--
-- Usage: EndServerHandler(SERVER_DATA_BLOCK *socket_info);
--
-- Inputs: socket_info - pointer to structure passed from RunServer to 
--                       worker thread
--
-- Output: shuts down and closes the socket, runs *reset routine from 
--         RunServer if it was specified, reduces thread count, and
--         frees memory
--
-- Return: none
=========================================================================== */
void EndServerHandler(SERVER_DATA_BLOCK *socket_info) {

	shutdown(socket_info->socket, SD_BOTH);
	closesocket(socket_info->socket);
	if (socket_info->reset != NULL) (*socket_info->reset)();
	socket_info->thread_count--;
	free(socket_info);
	return;
}

/* ===========================================================================
-- Routine to connect to a server
--
-- Usage: CLIENT_DATA_BLOCK *ConnectToServer(char *name, char *IP_address, int port, int *err);
--
-- Inputs: name - descriptive name for the connection (DCx, Focus, ...)
--         IP_address - IP address in normal form.  Use "127.0.0.1" for loopback test
--                      if NULL, uses DFLT_SERVER_IP_ADDRESS
--
-- Output: Opens socket and creates MUTEX to manage
--
-- Return:  ! NULL - pointer to a data block to be sent for communication with server
--            NULL - some error
=========================================================================== */
#ifndef DFLT_SERVER_IP_ADDRESS
#define	DFLT_SERVER_IP_ADDRESS	"127.0.0.1"
#endif

static CLIENT_DATA_BLOCK **list = NULL;
static int nlist = 0;

CLIENT_DATA_BLOCK *ConnectToServer(char *name, char *IP_address, int port, int *err) {
	static char *rname = "ConnectToServer";

	int i, rc;
	unsigned long ip_addr;
	HANDLE mutex;
	SOCKADDR_IN service;
	SOCKET m_socket;
	CLIENT_DATA_BLOCK *block;

	/* Validate parameters */
	if (IP_address == NULL) IP_address = DFLT_SERVER_IP_ADDRESS;
	if (err == NULL) err = &rc;				/* So don't need to track */
	*err = 0;										/* And assume success */

	/* Make sure we can actually use sockets in this program */
	if (InitSockets() != 0) {						/* Make sure socket support has been initialized */
		fprintf(stderr, "ERROR[%s]: Socket initialization failed\n", rname); fflush(stderr);
		*err = 1; return NULL;
	}

	/* Convert given IP address to a standardized (comparable) format */
	if ( (ip_addr = inet_addr(IP_address)) == -1) {
		fprintf(stderr, "ERROR[%s]: Socket initialization failed\n", rname); fflush(stderr);
		*err = 2; return NULL;
	}

	/* See if we already have the ip and port */
	for (i=0; i<nlist; i++) {
		if (list[i] != NULL && list[i]->ip_addr == ip_addr && list[i]->port == port && list[i]->active) return list[i];
	}

	/* Create a socket to the server */
	if ( (m_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP )) == INVALID_SOCKET) {
		fprintf(stderr, "ERROR[%s]: Failed to create socket for \"%s\": %ld\n", rname, name, WSAGetLastError() ); fflush(stderr);
		*err = 3; return NULL;
	}

/* Connect to the service */
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = ip_addr;
	service.sin_port = htons(port);
	if ( connect( m_socket, (SOCKADDR*) &service, sizeof(service) ) == SOCKET_ERROR) {
		fprintf(stderr, "ERROR[%s]: Failed to connect to service \"%s\"\n", rname, name); fflush(stderr);
		closesocket(m_socket);
		*err = 4; return NULL;
	}

	/* Create the mutex to limit control */
	if ( (mutex = CreateMutex(NULL, FALSE, NULL)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Unable to create the client access semaphores\n", rname); fflush(stderr);
		closesocket(m_socket);
		*err = 5; return NULL;
	}

	if ( (block = calloc(1, sizeof(*block))) == NULL) {
		fprintf(stderr, "ERROR[%s]: Unable to allocate memory for client block structure\n", rname); fflush(stderr);
		closesocket(m_socket);
		CloseHandle(mutex);
		*err = 6; return NULL;
	}

	block->magic   = CLIENT_MAGIC;			/* Copy over parameters */
	block->ip_addr = ip_addr;
	block->port    = port;
	block->socket  = m_socket;
	block->mutex   = mutex;
	block->active  = TRUE;

	/* Find a place to save this connection information */
	for (i=0; i<nlist; i++) {
		if (list[i] == NULL) break;
	}
	if (i >= nlist) {
		list = realloc(list, (nlist+10)*sizeof(*list));
		for (i=0; i<10; i++) list[nlist+i] = NULL;
		i = nlist;													/* So put in the right place */
		nlist += 10;												/* And mark our increase */
	}
	list[i] = block;

	return block;
}


/* ===========================================================================
-- Routine to close a server connection
--
-- Usage: int CloseServerConnection(CLIENT_DATA_BLOCK *block);
--
-- Inputs: block - pointer to a data block returned from ConnectToServer()
--
-- Output: Closes down the connection to the server
--
-- Return: 0 ==> successful
--         1 ==> connection already closed
============================================================================ */
int CloseServerConnection(CLIENT_DATA_BLOCK *block) {
	static char *rname = "CloseServerConnection";

	int i;

	/* If not a valid block, just return error */
	if (block->magic != CLIENT_MAGIC || ! block->active) return 1;

	/* Shutdown and close the socket */
	shutdown(block->socket, SD_BOTH);
	closesocket(block->socket);
	block->active = FALSE;
	free(block);

	/* Finally, remove this entry from the list of known connections */
	for (i=0; i<nlist; i++) {
		if (list[i] == block) list[i] = NULL;
	}

	return 0;
}


/* ===========================================================================
-- Receive a standard server request from the client
--
-- Usage: int GetStandardServerRequest(SERVER_DATA_BLOCK *block, CS_MSG *request, void **pdata);
--        int GetStandardServerResponse(SERVER_DATA_BLOCK *block, CS_MSG *request, void **pdata);
--        (at moment, these are identical, but may not be in future)
--
-- Inputs: block - structure passed from listen() that spawned us
--         request - pointer to block to receive the client request
--         pdata   - pointer to a void * variable to receive data
--
-- Output: *request - filled with the data request block from the client
--         *pdata   - malloc'd structure containing the request data (if any)
--
-- Return: 0 if successful
--         1 ==> client appears to have terminated
--         2 ==> recv() returned SOCKET_ERROR - assume client is terminated
--
-- Notes: Sends/receives the standard message exchange block defined
--         for this server implementation.
=========================================================================== */
int GetStandardServerResponse(CLIENT_DATA_BLOCK *block, CS_MSG *reply, void **pdata) {
	return GetSocketMsg(block->socket, reply, pdata);
}
int GetStandardServerRequest(SERVER_DATA_BLOCK *block, CS_MSG *request, void **pdata) {
	return GetSocketMsg(block->socket, request, pdata);
}
int GetSocketMsg(SOCKET socket, CS_MSG *request, void **pdata) {
	static char *rname = "GetSocketMsg";
	int icnt;
	
	/* Initialize all the results in case there is any failure */
	memset(request, 0, sizeof(*request));
	if (pdata != NULL) *pdata = NULL;

	/* Get the data from the socket */
	icnt = recv(socket, (char *) request, sizeof(*request), 0);
	if (icnt == 0) return 1;
	if (icnt == SOCKET_ERROR) {
		if (DebugLevel >= 1) { fprintf(stderr, "ERROR: recv() returned SOCKET_ERROR - assuming client terminated\n"); fflush(stderr); }
		return 2;
	}
	request->msg      = ntohl(request->msg);
	request->msgid    = ntohl(request->msgid);
	request->option   = ntohl(request->option);
	request->rc       = ntohl(request->rc);
	request->data_len = ntohl(request->data_len);
	request->crc32    = ntohl(request->crc32);

	/* If we are to get additional data, grab it now */
	if (request->data_len > 0) {
#define	MAX_RETRIES	(10)
		char *data, *aptr;
		int ineed, itry;

		ineed = request->data_len;										/* How many bytes of additional data should be coming */
		aptr = data = calloc(1, ineed);								/* Storage plus pointer for next pieces */
		for (itry=0; itry<MAX_RETRIES && ineed>0; itry++) {
			if (itry != 0 && DebugLevel >= 2) { fprintf(stderr, "recv() retry [%d]: expect=%d  remaining=%d\n", itry, request->data_len, ineed); fflush(stderr); }
			icnt = recv(socket, aptr, ineed, 0);
			if (icnt == 0) {
				return 1;
			} else if (icnt == SOCKET_ERROR) {
				fprintf(stderr, "ERROR[%s]: recv() returned SOCKET_ERROR -- assuming client has been terminated\n", rname);
				fflush(stderr);
				return 2;
			}
			ineed -= icnt;
			aptr  += icnt;
			Sleep(100);
		}

		/* At least warn if we were unsuccessful ... but go ahead and return */
		if (ineed != 0 && DebugLevel >= 1) { fprintf(stderr, "ERROR[%s]: Expected %d bytes but only got %d\n", rname, request->data_len, icnt); fflush(stderr); }

		/* If crc32 is set, verify or output an error */
		if (request->crc32 != 0) {
			uint32_t crc;
			crc = CRC32(data, request->data_len);
			if (crc != request->crc32 && DebugLevel >= 1) {
				fprintf(stderr, "ERROR[%s]: CRC32 mistmatch (0x%8.8x versus 0x%8.8x)\n", rname, crc, request->crc32); fflush(stderr);
			}
		}
		
		/* Either return or dump the data */
		if (pdata != NULL) {
			*pdata = data;
		} else {
			free(data);
		}
	}

	return 0;
}

/* ===========================================================================
-- Send standard server response to a standard request from a client 
--
-- Usage: int SendStandardServerResponse(SERVER_DATA_BLOCK *block, CS_MSG reply, void *data);
--        int SendStandardServerRequest(SERVER_DATA_BLOCK *block, CS_MSG reply, void *data);
--        (at moment, these are identical, but may not be in future)
--
-- Inputs: block - structure passed from listen() that spawned us
--         reply - structure with response information
--         data  - pointer to data to be sent as supplemental data
--
-- Output: none
--
-- Return: 0 if successful
--
-- Notes: If data is NULL, reply.data_len will be set to 0
=========================================================================== */
int SendStandardServerRequest(CLIENT_DATA_BLOCK *block, CS_MSG reply, void *data) {
	return SendSocketMsg(block->socket, reply, data);
}
int SendStandardServerResponse(SERVER_DATA_BLOCK *block, CS_MSG reply, void *data) {
	return SendSocketMsg(block->socket, reply, data);
}
int SendSocketMsg(SOCKET socket, CS_MSG reply, void *data) {
	int icnt, isend;

	/* Validate request and save length of data to send */
	if (data == NULL) reply.data_len = 0;			/* Can't send data if no pointer provided */
	isend = reply.data_len;								/* Amount of data to send */
	if (isend > 0) reply.crc32 = CRC32(data, isend);

	/* Network encode the return values and send the message back */
	reply.msg      = htonl(reply.msg);
	reply.msgid    = htonl(reply.msgid);
	reply.option   = htonl(reply.option);
	reply.rc       = htonl(reply.rc);
	reply.data_len = htonl(reply.data_len);
	reply.crc32    = htonl(reply.crc32);
	icnt = send(socket, (char *) &reply, sizeof(reply), 0);

	/* If there is any additional data to send, do so now */
	if (isend > 0) icnt = send(socket, data, isend, 0);

	return 0;
}

/* ===========================================================================
-- Efficient exchange on client side ... send request, receive response
--
-- Usage: int StandardServerExchange(CLIENT_DATA_BLOCK *block, CS_MSG request, void *send_data, CS_MSG *reply, void **reply_data);
--
-- Inputs: block      - structure passed from ConnectToServer()
--         request    - structure with the request
--         send_data  - data to be sent with the request
--         reply      - pointer to structure to get the reply
--         reply_data - pointer to variable that gets data returned with reply (if ! NULL)
--
-- Output: *reply     - filled with the reply from the server
--         *reply_data - filled with a malloc'd pointer containing message specific data from server
--
-- Return: 0 if successful
=========================================================================== */
int StandardServerExchange(CLIENT_DATA_BLOCK *block, CS_MSG request, void *send_data, CS_MSG *reply, void **reply_data) {
	static char *rname = "StandardServerExchange";
	int rc;

	/* Initial return values */
	memset(reply, 0, sizeof(*reply));
	if (reply_data != NULL) *reply_data = NULL;

/* Make sure we can do it */
	if (block == NULL || ! block->active) {
		fprintf(stderr, "ERROR[%s]: Client block invalid.  Server not connected\n", rname); fflush(stderr);
		return 1;
	}

/* Get control of the server semaphore */
#ifdef _WIN32
	if (WaitForSingleObject(block->mutex, CLIENT_MUTEX_WAIT) != WAIT_OBJECT_0) {
		fprintf(stderr, "ERROR[%s]: Timeout waiting for the semaphore\n", rname); fflush(stderr);
		return 2;
	}
#endif

/* Send request and wait for response */
	if ( (rc = SendStandardServerRequest(block, request, send_data)) == 0) {
		rc = GetStandardServerResponse(block, reply, reply_data);
	}

#ifdef _WIN32
	ReleaseMutex(block->mutex);
#endif
	if (rc != 0) { fprintf(stderr, "ERROR[%s]: Returned error %d\n", rname, rc); fflush(stderr); }

	return rc;
}

/* ===========================================================================
-- Routines to initialize socket support (OS dependent)
--
-- Usage: int InitSockets(void);
--        int ShutdownSockets(void);
--
-- Inputs: none
--
-- Output: Performs whatever initialization is required to enable IP sockets
--
-- Return: 0 if successful, !0 for errors
--
-- Notes: (1) It is safe to call Init_Sockets() multiple times.  Will simply
--            return 0 if already initialized
--        (2) For WIN32, calls WSAStartup() for socket interfaces and then
--            WSACleanup() at shutdown.  The WSACleanup() isn't really 
--            necessary but good to implement to be clean
--        (3) For Linux, sockets are automagically available
=========================================================================== */
static BOOL Socket_Interface_Initialized = FALSE;
static sig_atomic_t socket_init_count = 0;

int InitSockets(void) {

#ifdef _WIN32
	if (socket_init_count == 0) {
		int iResult;
		WSADATA wsaData;

	/* Initialize Winsock. */
		iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
		if ( iResult != NO_ERROR ) {
			fprintf(stderr, "Error at WSAStartup()\n"); fflush(stderr);
			return 1;
		}
	}
#endif

	socket_init_count++;				/* Keep track for shutdowns */
	return 0;
}

/* For moment, this just keeps track of the count.  For Win32, might
 * call WSACleanup() when nothing left needed */
int ShutdownSockets(void) {

	if (socket_init_count != 0) socket_init_count--;
	return 0;
}

/* ===========================================================================
-- Routines to convert a floating point double to/from network byte ordering
-- There is an assumption here that the format of the double is the same on
-- on the server and client architectures.  Could be more elaborate, but for 
-- now, assume this is true.
--
-- Usage: htond_me(double *var);
--        ntohd_me(double *var);
--
-- Input: var - pointer to 8 bytes which represent a double
--
-- Output: Byte-reversed is this machine is little-endien
--
-- Return: none
--
-- Notes: Routine, on first pass, will determine if architecture is big 
--        or little endien based on how an integer is stored.a
=========================================================================== */
void ntohd_me(double *val) {
	htond_me(val);
	return;
}

void htond_me(double *val) {

	static BOOL first = TRUE;							/* On first trial, determine byte ordering */
	static BOOL big_endien = FALSE;					/* Network format is big_endien */

	union {
		uint8_t bytes[8];
		uint64_t ival;
		double rval;
	} mydouble;
	uint8_t tmp;

	if (first) {
		if (sizeof(int) != 4 || sizeof(double) != 8) {
			fprintf(stderr, "ERROR: Code assume sizeof(int) = 4 (saw %zu) and sizeof(double) = 8 (saw %zu)\n", sizeof(int), sizeof(double));
			fflush(stderr);
			abort();
		}			
		mydouble.ival = 0x0102030405060708;
		big_endien = mydouble.bytes[0] == 0x01;

		/* Check that the format of the double matches expections */
		mydouble.rval = 37037.125;						/* random value that can be exactly represented */
		if (mydouble.ival != 0x40e215a400000000) {
			fprintf(stderr, "ERROR: Format of DOUBLE is not the same as windows\n"
					  "       Expected the value %f to be pattern 0x040e215a400000000\n", mydouble.rval);
			if (big_endien) {
				fprintf(stderr, "       Instead got %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\n",
						  mydouble.bytes[0], mydouble.bytes[1], mydouble.bytes[2], mydouble.bytes[3], mydouble.bytes[4], mydouble.bytes[5], mydouble.bytes[6], mydouble.bytes[7]);
			} else {
				fprintf(stderr, "       Instead got %.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x\n",
						  mydouble.bytes[7], mydouble.bytes[6], mydouble.bytes[5], mydouble.bytes[4], mydouble.bytes[3], mydouble.bytes[2], mydouble.bytes[1], mydouble.bytes[0]);
			}
			fflush(stderr);
			abort();
		}
		first = FALSE;
	}

	/* If we are big endien, nothing to do.  Otherwise swap byte order */
	if (! big_endien) {
		mydouble.rval = *val;
		tmp = mydouble.bytes[0]; mydouble.bytes[0] = mydouble.bytes[7]; mydouble.bytes[7] = tmp;
		tmp = mydouble.bytes[1]; mydouble.bytes[1] = mydouble.bytes[6]; mydouble.bytes[6] = tmp;
		tmp = mydouble.bytes[2]; mydouble.bytes[2] = mydouble.bytes[5]; mydouble.bytes[5] = tmp;
		tmp = mydouble.bytes[3]; mydouble.bytes[3] = mydouble.bytes[4]; mydouble.bytes[4] = tmp;
		memcpy(&mydouble.rval, val, sizeof(*val));
	}

	return;
}


/* ===========================================================================
-- Routine to reflect a bit pattern (0->7, 1->6, etc.)
--
-- Usage: int32_t reflect(uint32_t crc, int isize);
--
-- Inputs: crc   - value to be reflected
--         isize - number of bits for the reflection (8 or 32 usually)
--
-- Output: none
--
-- Return: reflected bit pattern
--
-- Note: Required by the CRC32 standard
=========================================================================== */
static uint32_t reflect(uint32_t crc, int isize) {
	int i;
	uint32_t new;

	new = 0;									/* Empty return value */
	for (i=0; i<isize; i++) if (crc & (0x01<<i)) new |= ( 0x01 << (isize-i-1) );
	return new;
}

/* ===========================================================================
-- Routine to calculate the CRC32 checksum of a buffer
--
-- Usage: uint32_t CRC32(void *buffer, int count);
--
-- Inputs: buffer - pointer to a buffer to be read
--         count  - number of bytes in the bufer
--
-- Output: none
--
-- Return: CRC32 checksum of the buffer
--
-- Note: This returns the "CRC-32" standard checksum
--         (1) polynomial    0x04C11DB7
--         (2) initial value 0xFFFFFFFF
--         (3) reflection of input value
--         (4) reflection of output value
--
-- Values can be verified with string based buffers at crccalc.com
--
-- CRC32 generator ... based partly on algorithm published on-line by NetworkDLS 2010
--   https://github.com/NTDLS/NSWFL/blob/master/NSWFL_CRC32.Cpp
--   https://github.com/NTDLS/NSWFL/blob/master/NSWFL_CRC32.H
=========================================================================== */
#define POLYNOMIAL (0x04C11DB7)

static uint32_t CRC32(void *buffer, int count) {

	static uint32_t *table = NULL;
	int i,j;
	uint32_t crc;
	unsigned char *data;

	/* On first call, initialize the lookup table with reflection (per standard) */
	if (table == NULL) {
		table = calloc(256, sizeof(*table));
		for (i=0; i<256; i++) {
			crc = reflect(i,8) << 24;
			for (j=0; j<8; j++) {
				crc = (crc << 1) ^ ((crc & 0x80000000) ? POLYNOMIAL : 0);
			}
			table[i] = reflect(crc, 32);
		}
	}

	/* Just go through the data */
	crc = 0xffffffff;								/* Initial value */
	data = (unsigned char *) buffer;
	for (i=0; i<count; i++) {
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ data[i]];
	}
	crc ^= 0xffffffff;							/* Finalize */
	return crc;
}
