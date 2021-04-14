#ifndef _SERVER_SUPPORT_H_LOADED
#define _SERVER_SUPPORT_H_LOADED

/* Make sure we have enough #includes to run */
#include <stdint.h>             /* C99 extension to get known width integers */
#include <signal.h>

/* from standard Windows library */
#ifdef _WIN32
	#define STRICT					  /* define before including windows.h for stricter type checking */
	#include <windows.h>			  /* master include file for Windows applications */
	#include <winsock.h>

	#undef _POSIX_
		#include <process.h>			  /* for process control fuctions (e.g. threads, programs) */
	#define _POSIX_

	#ifndef SD_BOTH
		#define	SD_RECEIVE	(0)		/* From MSDN documentation on ShutDown routine */
		#define	SD_SEND		(1)
		#define	SD_BOTH		(2)
	#endif

#elif __linux__

	#include <arpa/inet.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <unistd.h>

	typedef int SOCKET;
	typedef struct sockaddr SOCKADDR;
	typedef struct sockaddr_in SOCKADDR_IN;

	#define INVALID_SOCKET (-1)
	#define SOCKET_ERROR (-1)
	#define WSAECONNRESET (-1)
	#define closesocket close

	#ifndef SD_BOTH
		#define	SD_RECEIVE	(SHUT_RD)	/* From MSDN documentation on ShutDown routine */
		#define	SD_SEND		(SHUT_WR)
		#define	SD_BOTH		(SHUT_RDWR)
	#endif

#else
	#error "Unsupported OS"
#endif

/* Standardized message to the server/client, expecting standardized response */
#pragma pack(4)
typedef struct _CS_MSG {
	uint32_t msg;							/* Command message (see message #defines) */
	int32_t msgid;							/* user ID for potential queued operations */ 
	int32_t option;						/* single parameter for enhancing some msgs */ 
	int32_t rc;								/* return code from message (0 generally ok) */
	uint32_t data_len;					/* length (bytes) of extra data transmitted with message */
	uint32_t crc32;						/* CRC32 checksum of transmitted data (if any) */
} CS_MSG;
#pragma pack()

/* Information block on thread doing actual client work */
typedef struct _SERVER_DATA_BLOCK {
	SOCKET socket;
	sig_atomic_t *thread_count;
	void (*reset)(void);
} SERVER_DATA_BLOCK;

/* Information block on thread doing actual client work */
#define	CLIENT_MAGIC	(0x32716543)
typedef struct _CLIENT_DATA_BLOCK {
	int magic;									/* Pattern to identify it as mine */
	BOOL active;								/* Flag indicating it is still active */
	unsigned long ip_addr;					/* Standardized IP address */
	int port;									/* Port connection */
	SOCKET socket;								/* Socket for this connection */
	HANDLE mutex;								/* Semaphore to limit multiple access to this connection */
} CLIENT_DATA_BLOCK;

int InitSockets(void);
int ShutdownSockets(void);
int DebugSockets(int level);				/* Enable a level of debug messages for sockets (all to stderr) */

/* Routines to start a server activity on a particular port */
int RunServer      (char *name, unsigned short port, void (*ServerHandler)(void *), void (*reset)(void));
int RunServerThread(char *name, unsigned short port, void (*ServerHandler)(void *), void (*reset)(void));
void EndServerHandler(SERVER_DATA_BLOCK *socket_info);

/* Routines to connect to a server */
CLIENT_DATA_BLOCK *ConnectToServer(char *name, char *IP_address, int port, int *err);
int CloseServerConnection(CLIENT_DATA_BLOCK *block);

/* Standard messages across network */
/* Generic */
	int SendSocketMsg(SOCKET socket, CS_MSG msg, void *data);
	int GetSocketMsg (SOCKET socket, CS_MSG *msg, void **pdata);
/* Server calls */
   int GetStandardServerRequest(SERVER_DATA_BLOCK *block, CS_MSG *request, void **pdata);
   int SendStandardServerResponse(SERVER_DATA_BLOCK *block, CS_MSG reply, void *data);
/* Client calls */
	int SendStandardServerRequest(CLIENT_DATA_BLOCK *block, CS_MSG request, void *data);
   int GetStandardServerResponse(CLIENT_DATA_BLOCK *block, CS_MSG *reply,  void **pdata);
	int StandardServerExchange(CLIENT_DATA_BLOCK *block, CS_MSG request, void *send_data, CS_MSG *reply, void **reply_data);

void htond_me(double *val);							/* Handle doubles across network (my code) */
void ntohd_me(double *val);							/* network to host for double */

#endif			/* #ifndef _SERVER_SUPPORT_H_LOADED */