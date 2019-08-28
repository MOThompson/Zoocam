/******************************************************************************
-- extracted server code (with dummy camera stubs)
******************************************************************************/

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */
#undef	DEBUG_ME								/* Define for lots of messages */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "DCx.h"					/* Access to the DCX info */
#include "DCx_Client.h"			/* For prototypes				*/

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif

#define	DCX_MSG_LISTEN_PORT			(985)				/* Port for high level DCx function interface */

/* List of the allowed requests */
/* List of the allowed requests */
#define SERVER_END				(0)					/* Shut down server (please don't use) */
#define DCX_QUERY_VERSION		(1)					/* Return version of the server code */
#define DCX_GET_CAMERA_INFO	(2)					/* Return structure with camera data */
#define DCX_ACQUIRE_IMAGE		(3)					/* Acquire image only (local storage) */
#define DCX_GET_IMAGE_INFO		(4)					/* Return info on the image */
#define DCX_GET_IMAGE_DATA		(5)					/* Transfer the actual image data */

/* Standardized message to the server, expecting standardized response */
#pragma pack(4)
typedef struct _DCX_MSG {
	uint32_t msg;
	uint32_t msgid;
	uint32_t rc;
	uint32_t data_len;
} DCX_MSG;
#pragma pack()

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int Init_Sockets(void);
static int Shutdown_Sockets(void);
static void htond_me(double *val);							/* Handle doubles across network (my code) */
static void ntohd_me(double *val);							/* network to host for double */

static void DCx_Msg_Server(void *arglist);
static int RunServer(unsigned short port, int (*ClientHandler)(SOCKET), void (*reset)(void));
static int DCx_msg_work(SOCKET socket);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- Routine to initialize high level DCx remote socket server
--
-- Usage: void Init_DCx_Server(void)
--
-- Inputs: none
--
-- Output: Spawns thread running the DCx high function server(s)
--
-- Return:  0 if all was successful
--           -1 ==> Unable to create the access semaphore
--           otherwise lower byte is error byte from msg server
--                     upper byte is error byte from text server
--            1 ==> Unable to spawn the thread
--            2 ==> Server init was called, but thread still -1 (race?)
=========================================================================== */
#define	DCX_SERVER_WAIT	(30000)						/* 30 second time-out */

static uintptr_t DCx_Msg_Server_Thread = -1;		/* -1 means not actually running */
static BOOL DCx_Msg_Server_Up = FALSE;				/* Server has been started */
static HANDLE DCx_Server_Mutex = NULL;				/* access to the client/server communication */

int Init_DCx_Server(void) {
	static char *rname = "Init_DCx_Server";

	int rc_msg;

/* If the mutex has not been created, do so now */
	if (DCx_Server_Mutex == NULL) DCx_Server_Mutex = CreateMutex(NULL, FALSE, NULL);
	if (DCx_Server_Mutex == NULL) {
		fprintf(stderr, "ERROR[%s]: Unable to create the server access semaphores\n", rname); fflush(stderr);
		return -1;
	}

/* Bring up the message based server */
	if (! DCx_Msg_Server_Up) {
		rc_msg = 0;
		DCx_Msg_Server_Up = TRUE;								/* Set to avoid race conflicts if called twice */
		if ( (DCx_Msg_Server_Thread = _beginthread(DCx_Msg_Server, 0 , NULL)) == -1) {
			fprintf(stderr, "ERROR[%s]: Unable to start the DCx message based remote server\n", rname); fflush(stderr);
			DCx_Msg_Server_Up = FALSE;
			rc_msg = 1;
		}
	} else {
		rc_msg = (DCx_Msg_Server_Thread == -1) ? 2 : 0;	/* Verify we have a valid thread ID */
	}

	return rc_msg;
}


/* ===========================================================================
-- Routine to shutdown high level DCx remote socket server
--
-- Usage: void Shutdown_DCx_Server(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_DCx_Server(void) {

	Shutdown_Sockets();

	return 0;
}

/* ===========================================================================
-- Routine to start the server for the camera acquisition
--
-- Usage: void DCx_Msg_Server_Thread(void)
--
-- Inputs: none
--
-- Output: Registers the DCX_MSG_LISTEN_PORT and waits for messages
--
-- Return: none
=========================================================================== */
static void DCx_Msg_Server(void *arglist) {
	fprintf(stderr, "Starting DCx message remote server on %d port (version %d)\n", DCX_MSG_LISTEN_PORT, DCX_CLIENT_SERVER_VERSION);
	fflush(stderr);
	RunServer(DCX_MSG_LISTEN_PORT, DCx_msg_work, NULL);
	return;
}


/* ===========================================================================
-- Routine to start a server listening on specific port
--
-- Usage: static int RunServer(USHORT port);
--
-- Inputs: port                  - port to listen on
--         ClientHandler(SOCKET) - routine called when connected
--         reset                 - if !NULL, routine called after socket is
--                                 closed to reset the server to known state
--
-- Output: waits for a connection, calls ClientHandler to process
--
-- Return: 0 if successful, !0 on error
--
-- Notes: The server continues to listen for clients until the
--        clientHandler() returns a non-zero value on its return
=========================================================================== */
static int RunServer(unsigned short port, int (*ClientHandler)(SOCKET), void (*reset)(void)) {

	SOCKET m_socket;					/* Port listening socket */
	SOCKET c_socket;					/* Client work socket */
	SOCKADDR_IN service;
	int rc;

	static BOOL first = TRUE;

/* Make sure we have sockets available */
	if (Init_Sockets() != 0) return 3;

/* Create a socket to listen for clients */
	if ( (m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET ) {
		fprintf(stderr, "Error creating socket(): %ld\n", WSAGetLastError() ); fflush(stderr);
		return 3;
	}

/* Bind the socket to all adapters (INADDR_ANY) */
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = htonl( INADDR_ANY );
	service.sin_port = htons( port );
	if ( bind( m_socket, (SOCKADDR*) &service, sizeof(service) ) == SOCKET_ERROR ) {
		fprintf(stderr, "bind() failed.\n" ); fflush(stderr);
		closesocket(m_socket);
		return 3;
	}

/* Listen on the socket. */
	if ( listen( m_socket, 1 ) == SOCKET_ERROR ) {
		fprintf(stderr, "Error listening on socket.\n"); fflush(stderr);
		closesocket(m_socket);
		return 3;
	}

/* Now sit and accept connections until someone says uncle */
	do {
		fprintf(stderr, "\nDCx server: Waiting for a client on port %d\n", port ); fflush(stderr);
		c_socket = SOCKET_ERROR;
		while ( c_socket == SOCKET_ERROR ) c_socket = accept( m_socket, NULL, NULL );
		fprintf(stderr, "DCx server: Connection on port %d established.\n", port); fflush(stderr);

		rc = (*ClientHandler)(c_socket);
		shutdown(c_socket, SD_BOTH);
		closesocket(c_socket);
		if (reset != NULL) (*reset)();
	} while (rc == 0);

	/* Have received somehow a message to end (SERVER_END command or errors) */
	closesocket(m_socket);
	return 0;
}


/* ===========================================================================
-- Actual server routine to process messages received on the open socket.
--
-- Usage: int DCx_msg_work(SOCKET socket)
--
-- Inputs: socket - an open socket for exchanging messages with the client
--
-- Output: whatever needs to be done
--
-- Return: 0 ==> close this socket but continue listening for new clients
--         1 ==> Timeout waiting for semaphore
--         2 ==> socket appears to have been abandoned
=========================================================================== */
static int DCx_msg_work(SOCKET socket) {
	static char *rname = "DCx_msg_work";

	int icnt, buflen;
	DCX_MSG request, reply;
	void *buffer, *received_data;
	BOOL ServerActive;

	DCX_STATUS camera_info;

	/* These refer to the last captured image */
	static DCX_IMAGE_INFO image_info;
	static char *image_data;

/* Receive a command and process it */
/* recv returns SOCKET_ERROR on problem ... or when other side closes it */
	ServerActive = TRUE;
	received_data = buffer = NULL;
	while (ServerActive) {
		icnt = recv( socket, (char *) &request, sizeof(request), 0 );
		if (icnt == 0) break;
		if (icnt == SOCKET_ERROR) {
			fprintf(stderr, "ERROR: recv() returned SOCKET_ERROR -- assuming client has been terminated\n");
			fflush(stderr);
			return 0;
		}

		/* If we are to get additional data, grab it now */
		if (request.data_len > 0) {
			received_data = malloc(request.data_len);
			icnt = recv( socket, (char *) &received_data, request.data_len, 0 );
			if (icnt == 0) break;
			if (icnt == SOCKET_ERROR) {
				fprintf(stderr, "ERROR: recv() returned SOCKET_ERROR -- assuming client has been terminated\n");
				fflush(stderr);
				return 0;
			}
		}

		/* Create a default reply message */
		memcpy(&reply, &request, sizeof(reply));
		reply.rc = reply.data_len = 0;			/* All okay and no extra data */
		buffer = NULL; buflen = 0;					/* No extra data on return */

		/* Be very careful ... only allow one socket message to be in process at any time */
		/* The code should already protect, but not sure how interleaved messages may impact operations */
		if (WaitForSingleObject(DCx_Server_Mutex, DCX_SERVER_WAIT) != WAIT_OBJECT_0) {
			fprintf(stderr, "ERROR[%s]: Timeout waiting for the DCx semaphore\n", rname); fflush(stderr);
			reply.msg = htonl(-1); reply.rc = -1;

		} else switch (ntohl(request.msg)) {
			case SERVER_END:
				fprintf(stderr, "DCx msg server: SERVER_END\n"); fflush(stderr);
				ServerActive = FALSE;
				break;

			case DCX_QUERY_VERSION:
				fprintf(stderr, "DCx msg server: DCX_QUERY_VERSION()\n");	fflush(stderr);
				reply.rc = DCX_CLIENT_SERVER_VERSION;
				break;

			case DCX_GET_CAMERA_INFO:
				fprintf(stderr, "DCx msg server: DCX_GET_CAMERA_INFO()\n");	fflush(stderr);
				reply.rc = DCx_Status(&camera_info);			/* Get the information */
				reply.data_len = sizeof(camera_info);
				buffer = (void *) &camera_info;
				break;
				
			case DCX_ACQUIRE_IMAGE:
				fprintf(stderr, "DCx msg server: DCX_ACQUIRE_IMAGE()\n");	fflush(stderr);
				if (image_data != NULL) { free(image_data); image_data = NULL; }			/* Free last one */
				reply.rc = DCx_Acquire_Image(&image_info, &image_data);	/* Grab an image now */
				reply.data_len = 0;
				break;
				
			case DCX_GET_IMAGE_INFO:
				fprintf(stderr, "DCx msg server: DCX_GET_IMAGE_INFO()\n");	fflush(stderr);
				if (image_data == NULL) {
					reply.rc = -1;
				} else {
					reply.rc = 0; 
					reply.data_len = sizeof(DCX_IMAGE_INFO);
					buffer = (void *) &image_info;				}
				break;

			case DCX_GET_IMAGE_DATA:
				fprintf(stderr, "DCx msg server: DCX_GET_IMAGE_DATA()\n");	fflush(stderr);
				if (image_data == NULL) {
					reply.rc = -1;
				} else {
					reply.rc = 0; 
					reply.data_len = image_info.memory_pitch * image_info.height;
					buffer = (void *) image_data;				}
				break;

			default:
				fprintf(stderr, "ERROR: DCx server message received (%d) that was not recognized.\n"
						  "       Will be ignored with rc=0 return code.\n", ntohl(request.msg));
				fflush(stderr);
				reply.rc = -1;
				break;
		}
		ReleaseMutex(DCx_Server_Mutex);

		/* Network encode the values and send the immediate response */
		if (buffer == NULL) reply.data_len = 0;
		buflen = reply.data_len;
		
		/* Network encode the return values and send the message back */
		reply.rc       = htonl(reply.rc);
		reply.data_len = htonl(reply.data_len);
		icnt = send( socket, (char *) &reply, sizeof(reply), 0 );

		/* Send the message data if appropriate */
		if (buffer != NULL) {
			icnt = send(socket, buffer, buflen, 0);
			buffer = NULL;
		}

		if (received_data != NULL) { free(received_data); received_data = NULL; }
	}

	/* Avoid memory leak ... release if we somehow get here */
	if (received_data != NULL) { free(received_data); received_data = NULL; }

	return 0;				/* End -- go back for a new client */
}

/* ===========================================================================
   ===========================================================================
   General network support for transmitting doubles in IEEE 754 format
   ===========================================================================
   ===========================================================================  */

/* ===========================================================================
-- Routines to initialize socket support (OS dependent)
--
-- Usage: int Init_Sockets(void);
--        int Shutdown_Sockets(void);
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

static int Init_Sockets(void) {

	if (Socket_Interface_Initialized) return 0;

#ifdef _WIN32
	/* Initialize Winsock. */
	{
		int iResult;
		WSADATA wsaData;
		iResult = WSAStartup( MAKEWORD(2,2), &wsaData );
		if ( iResult != NO_ERROR ) {
			fprintf(stderr, "Error at WSAStartup()\n"); fflush(stderr);
			return 1;
		}
	}
#endif
	Socket_Interface_Initialized = TRUE;
	return 0;
}

static int Shutdown_Sockets(void) {

	if (Socket_Interface_Initialized) {
		WSACleanup();
		Socket_Interface_Initialized = FALSE;
	}
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
static void ntohd_me(double *val) {
	htond_me(val);
	return;
}

static void htond_me(double *val) {

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
#ifdef __linux__
			fprintf(stderr, "ERROR: Code assume sizeof(int) = 4 (saw %zd) and sizeof(double) = 8 (saw %zd)\n", sizeof(int), sizeof(double));
#else
			fprintf(stderr, "ERROR: Code assume sizeof(int) = 4 (saw %d) and sizeof(double) = 8 (saw %d)\n", sizeof(int), sizeof(double));
#endif
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


/* =========================================================================== */
/* =========================================================================== */
/* =========================================================================== */
/* =========================================================================== */
/* Dummy DCx camera calls w/ physically okay return values */

int DCx_Acquire_Image(DCX_IMAGE_INFO *info, char **buffer) {

	uint32_t i,j,pitch;
	char *image;
	
	/* Fake an RGB image */
	info->height = 1024;
	info->width  = 1280;
	info->memory_pitch = 3840;
	info->exposure = 99.87;
	info->gamma = 1.00;
	info->master_gain = 100;
	info->red_gain = 15;
	info->green_gain = 0;
	info->blue_gain = 25;
	info->color_correction = 1;
	info->color_correction_factor = 0.00;
	info->red_saturate = 283;
	info->green_saturate = 8273;
	info->blue_saturate = 82;

	*buffer = image = malloc(info->memory_pitch * info->height);
	pitch = info->memory_pitch;
	for (i=0; i<info->height; i++) {
		for (j=0; j<info->width; j++) {
			image[i*pitch+3*j+0] = (j) % 255 ;
			image[i*pitch+3*j+1] = (info->width-j) % 255;
			image[i*pitch+3*j+2] = (j+128) % 255;
		}
	}
	return 0;
}

int DCx_Status(DCX_STATUS *status) {
	strcpy_s(status->manufacturer, sizeof(status->manufacturer), "Manufacturer: Thorlabs GmbH");
	strcpy_s(status->model, sizeof(status->model), "C1284R13C");
	strcpy_s(status->serial, sizeof(status->serial), "Serial: 4103534309");
	strcpy_s(status->version, sizeof(status->version),"Version: V1.0");
	strcpy_s(status->date, sizeof(status->date), "Date: 22.05.2019");
	status->CameraID = 1;
	status->color_mode = IMAGE_COLOR;
	status->pixel_pitch = 360;
	status->fps = 5.0;
	status->exposure = 99.87;
	status->gamma = 1.00;
	status->master_gain = 0;
	status->red_gain    = 15;
	status->green_gain  = 0;
	status->blue_gain   = 25;
	status->color_correction = 1;
	status->color_correction_factor = 0.0;
	return 0;
}

int main(int argc, char *argv[]) {

	Init_DCx_Server();

	while (DCx_Msg_Server_Up) Sleep(500);
	return 0;
}

