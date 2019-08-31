/******************************************************************************
-- Client interface only (extracted from DCx_client_server.c)
******************************************************************************/

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

static void cleanup(void);
static SOCKET OpenServer(unsigned long IP_address, unsigned short port, char *server_name);
static int CloseServer(SOCKET m_socket);
static int Raw_DCx_Server_Exchange(DCX_MSG *request, void *data_send, DCX_MSG *reply, void **data_back);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static BOOL DCx_msg_socket_ok = FALSE;			/* Is the socket okay to use? */
static SOCKET DCx_msg_socket;						/* Socket opened for communication */

int main(int argc, char *argv[]) {

	DCX_IMAGE_INFO info;
	DCX_STATUS status;
	unsigned char *data = NULL;
	int rc, client_version, server_version;
	uint32_t i,j;
	char *server_IP;

	server_IP = LOOPBACK_SERVER_IP_ADDRESS;	/* Local test (server on same machine) */
//	server_IP = "128.253.129.74";					/* Machine in laser room */
//	server_IP = "128.253.129.71";					/* Machine in open lab room */

	if (argc > 1) server_IP = argv[1];			/* Use the first argument on command line if given */
	
	if ( (rc = Init_DCx_Client(server_IP)) != 0) {
		fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP);
		return 0;
	}

	client_version = DCx_Remote_Query_Client_Version();
	server_version = DCx_Remote_Query_Server_Version();
	printf("Client version: %4.4d\n", client_version); fflush(stdout);
	printf("Server version: %4.4d\n", DCx_Remote_Query_Server_Version()); fflush(stdout);
	if (client_version != server_version) {
		fprintf(stderr, "ERROR: Version mismatch between client and server.  Have to abort\n");
		return 3;
	}

	if ( (rc = DCx_Remote_Get_Camera_Info(&status)) == 0) {
		printf("Camera information\n");
		printf("  Manufacturer: %s\n", status.manufacturer);
		printf("  Model: %s\n", status.model);
		printf("  Serial: %s\n", status.serial);
		printf("  Version: %s\n", status.version);
		printf("  Date: %s\n", status.date);
		printf("  CameraID: %d\n", status.CameraID);
		printf("  ColorMode: %s\n", status.color_mode == IMAGE_MONOCHROME ? "monochrome" : "color");
		printf("  Pixel Pitch: %d\n", status.pixel_pitch);
		printf("  Frame rate: %.2f\n", status.fps);
		printf("  Exposure: %.2f ms\n", status.exposure);
		printf("  Gamma: %.2f\n", status.gamma);
		printf("  Gains: Master: %d   RGB: %d,%d,%d\n", status.master_gain, status.red_gain, status.green_gain, status.blue_gain);
		printf("  Color correction: %d %f\n", status.color_correction, status.color_correction_factor);
		fflush(stdout);
	} else {
		fprintf(stderr, "ERROR: Unable to get camera information\n"); fflush(stderr);
	}

	if ( (rc = DCx_Remote_Acquire_Image(&info, &data)) == 0) {
		printf("Image information\n");
		printf("  width: %d   height: %d\n", info.width, info.height);
		printf("  memory row pitch: %d\n", info.memory_pitch);
		printf("  exposure: %.2f ms\n", info.exposure);
		printf("  gamma: %.2f\n", info.gamma);
		printf("  Gains: Master: %d   RGB: %d,%d,%d\n", info.master_gain, info.red_gain, info.green_gain, info.blue_gain);
		printf("  Color correction: %d %f\n", info.color_correction, info.color_correction_factor);
		printf("  Saturated pixels: Red: %d  Green: %d  Blue: %d\n", info.red_saturate, info.green_saturate, info.blue_saturate);
		for (i=0; i<10; i++) {
			printf("  Row %d:", i);
			for (j=0; j<15; j++) printf(" %3.3d", data[i*info.memory_pitch+j]);
			printf("\n");
		}
	} else {
		fprintf(stderr, "ERROR: Failed to acquire image and data\n"); fflush(stderr);
	}
	fflush(NULL);

	Shutdown_DCx_Client();
	return 0;
}

/* ===========================================================================
-- Routine to open and initialize the socket to the DCx server
--
-- Usage: int Init_DCx_Client(char *IP_address);
--
-- Inputs: IP_address - IP address in normal form.  Use "127.0.0.1" for loopback test
--                      if NULL, uses DFLT_SERVER_IP_ADDRESS
--
-- Output: Creates MUTEX semaphores, opens socket, sets atexit() to ensure closure
--
-- Return:  0 - successful
--          1 - unable to create semaphores for controlling access to hardware
--          2 - unable to open the sockets (one or the other)
--          3 - unable to query the server version
--          4 - server / client version mismatch
--
-- Notes: Must be called before any attempt to communicate across the socket
=========================================================================== */
#ifdef _WIN32
	static HANDLE DCx_Client_Mutex = NULL;			/* access to the client/server communication */
#endif

int Init_DCx_Client(char *IP_address) {
	static char *rname = "Init_DCx_Client";
	int rc;
	static BOOL first = TRUE;

	/* As this is the first message from clients normally, put out a NL so get over startup line */
	fprintf(stderr, "\n"); fflush(stderr);

#ifdef DEBUG_ME
	fprintf(stderr, "Init_DCx_Client(%s)\n", IP_address != NULL ? IP_address : "default"); fflush(stderr);
#endif

	if (first) {
#ifdef _WIN32
		if (DCx_Client_Mutex == NULL) DCx_Client_Mutex = CreateMutex(NULL, FALSE, NULL);
		if (DCx_Client_Mutex == NULL) {
			fprintf(stderr, "ERROR[%s]: Unable to create the client access semaphores\n", rname); fflush(stderr);
			return 1;
		}
#endif
		atexit(cleanup);
		first = FALSE;
	}

	/* Shutdown sockets if already open (reinitialization allowed) */
	cleanup();													/* Close sockets if open */

	/* Create socket to the message server */
	DCx_msg_socket = OpenServer(inet_addr(IP_address != NULL ? IP_address : DFLT_SERVER_IP_ADDRESS), DCX_MSG_LISTEN_PORT, "DCx msg client");
	DCx_msg_socket_ok = (DCx_msg_socket != INVALID_SOCKET);

	/* If either fail, then just fail completely */
	if (DCx_msg_socket_ok) {
		rc = 0;			/* Assume we will be successful */
	} else {
		fprintf(stderr, "ERROR[%s]: Failed to open DCx client socket\n", rname);	fflush(stderr);
		rc = 2;
	}

	/* Immediately check the version numbers of the client (here) and the server (other end) */
	/* Report an error if they do not match.  Code version must match */
	if (rc == 0) {
		int server_version;
		server_version = DCx_Remote_Query_Server_Version();
		if (server_version != DCX_CLIENT_SERVER_VERSION) {
			fprintf(stderr, "ERROR[%s]: Version mismatch between server (%d) and client (%d)\n", rname, server_version, DCX_CLIENT_SERVER_VERSION); fflush(stderr);
			rc = 4;
		}
	}

	/* Report success, and if not close everything that has been started */
	if (rc == 0) {
		fprintf(stderr, "INFO: Connected to DCx server on %s\n", IP_address); fflush(stderr);
	} else {
		cleanup();
	}

	return rc;
}

/* ===========================================================================
-- Routine to shutdown high level DCx remote socket server
--
-- Usage: void Shutdown_DCx_Client(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_DCx_Client(void) {

	cleanup();										/* Close sockets if open */
	return 0;
}

/* ===========================================================================
-- Quick routine to check for errors and print messages for TCP transaction
--
-- Usage: int Error_Check(int rc, DCX_MSG *reply, int expect_msg);
--
-- Inputs: rc - return code from Raw_DCx_Server_Exchange()
--              is normally number of bytes received in the exchange
--         reply - message returned from the exchange
--         expect_msg - the message sent, which should be returned
--
-- Output: prints error message if error or mismatch
--
-- Return: 0 if no errors, otherwise -1
=========================================================================== */
static int Error_Check(int rc, DCX_MSG *reply, int expect_msg) {

	if (rc < 0) {
		fprintf(stderr, "ERROR: Unexpected error from Raw_DCx_Server_Exchange (rc=%d)\n"); fflush(stderr);
		return -1;
	} else if (reply->msg != expect_msg) {
		fprintf(stderr, "ERROR: Expected %d in reply message but got %d back\n", expect_msg, reply->msg); fflush(stderr);
		return -1;
	}
	return 0;
}

/* ===========================================================================
--	Routine to return current version of this code
--
--	Usage:  int DCx_Remote_Query_Server_Version(void);
--         int DCx_Remote_Query_Client_Version(void);
--
--	Inputs: none
--		
--	Output: none
--
-- Return: Integer version number.  
--
--	Notes: The verison number returned is that given in this code when
--        compiled. The routine simply returns this version value and allows
--        a program that may be running in the client/server model to verify
--        that the server is actually running the expected version.  Programs
--        should always call and verify the expected returns.
=========================================================================== */
int DCx_Remote_Query_Client_Version(void) {
	return DCX_CLIENT_SERVER_VERSION;
}

int DCx_Remote_Query_Server_Version(void) {

	DCX_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_QUERY_VERSION;
	
	/* Get the response */
	rc = Raw_DCx_Server_Exchange(&request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_QUERY_VERSION) != 0) return -1;

	return reply.rc;					/* Will be the version number */
}

/* ===========================================================================
--	Routine to return information on the camera
--
--	Usage:  int DCx_Remote_Get_Camera_Info(DCX_STATUS *status);
--
--	Inputs: status - pointer to variable to receive information
--		
--	Output: *status filled with information
--
-- Return: 0 if successful
=========================================================================== */
int DCx_Remote_Get_Camera_Info(DCX_STATUS *status) {

	DCX_MSG request, reply;
	DCX_STATUS *my_status = NULL;

	int rc;

	/* Fill in default response (no data) */
	if (status != NULL) memset(status, 0, sizeof(DCX_STATUS));

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_CAMERA_INFO;

	/* Get the response */
	rc = Raw_DCx_Server_Exchange(&request, NULL, &reply, (void **) &my_status);
	if (Error_Check(rc, &reply, DCX_GET_CAMERA_INFO) != 0) return rc;

	memcpy(status, my_status, sizeof(DCX_STATUS));
	free(my_status);

	return 0;
}

/* ===========================================================================
--	Routine to acquire an image (local save)
--
--	Usage:  int DCx_Remote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);
--
--	Inputs: info - pointer to buffer to receive information about image
--         image - pointer to get malloc'd memory with the image itself
--                 caller responsible for releasing this memory
-- 
--	Output: info and image defined if new image obtained
--
-- Return: 0 if successful, other error indication
--         On error *image will be NULL and *info will be zero
--
-- Note: This is really 3 transactions with the server
--         (1) DCX_ACQUIRE_IMAGE   [captures the image]
--         (2) DCX_GET_IMAGE_INFO  [transmits information about image]
--         (3) DCX_GET_IMAGE_DATA  [transmits actual image bytes]
=========================================================================== */
int DCx_Remote_Acquire_Image(DCX_IMAGE_INFO *info, char **image) {

	DCX_MSG request, reply;
	DCX_IMAGE_INFO *my_info = NULL;

	int rc;

	/* Fill in default response (no data) */
	if (info  != NULL) memset(info, 0, sizeof(DCX_IMAGE_INFO));
	if (image != NULL) *image = NULL;

	/* Acquire the image */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_ACQUIRE_IMAGE;
	rc = Raw_DCx_Server_Exchange(&request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_ACQUIRE_IMAGE) != 0) return -1;

	/* Acquire information about the image */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_IMAGE_INFO;
	rc = Raw_DCx_Server_Exchange(&request, NULL, &reply, &my_info);
	if (Error_Check(rc, &reply, DCX_GET_IMAGE_INFO) != 0) return -1;

	/* Copy the info over to user space */
	memcpy(info, my_info, sizeof(DCX_IMAGE_INFO));
	free(my_info);

	/* Get the actual image data (will be big) */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_IMAGE_DATA;
	rc = Raw_DCx_Server_Exchange(&request, NULL, &reply, image);
	if (Error_Check(rc, &reply, DCX_GET_IMAGE_DATA) != 0) return -1;
	
	return 0;
}

/* ===========================================================================
-- Routine to connect a server on a specific machine and a specific port
--
-- Usage: SOCKET OpenServer(ULONG IP_address, USHORT port, char *server_name);
--
-- Inputs: IP_address  - address of the server.  This is encoded and
--                       typically sent as inet_addr("128.84.249.249")
--         port        - port to be connected
--         server_name - name (informational only - included on error messages)
--
-- Output: opens a socket to the specified server port
--
-- Return: Socket for communication with the server if successful
--         SOCKET_INVALID if unsuccessful for any reason.  Reason printed to stderr
=========================================================================== */
static SOCKET OpenServer(unsigned long IP_address, unsigned short port, char *server_name) {
	static char *rname = "OpenServer";

	SOCKADDR_IN clientService;
	SOCKET m_socket;

	if ( Init_Sockets() != 0) return INVALID_SOCKET;		/* Make sure socket support has been initialized */

/* Create a socket for my use */
	m_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if ( m_socket == INVALID_SOCKET ) {
#ifdef _WIN32
		fprintf(stderr, "ERROR[%s]: Failed to create socket for \"%s\": %ld\n", rname, server_name, WSAGetLastError() ); fflush(stderr);
#elif __linux__
		fprintf(stderr, "ERROR[%s]: Failed to create socket for \"%s\": %m\n", rname, server_name); fflush(stderr);
#endif
		return INVALID_SOCKET;
	}

/* Connect to the service */
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = IP_address;
	clientService.sin_port = htons(port);
	if ( connect( m_socket, (SOCKADDR*) &clientService, sizeof(clientService) ) == SOCKET_ERROR) {
		fprintf(stderr, "ERROR[%s]: OpenServer failed to connect to service \"%s\"\n", rname, server_name); fflush(stderr);
		closesocket(m_socket);
		return INVALID_SOCKET;
	}

	return m_socket;
}


/* ===========================================================================
-- Routine to close a server previously opened by OpenServer
--
-- Usage: int CloseServer(SOCKET socket);
--
-- Inputs: socket - a socket previously returned by OpenServer
--
-- Output: closes the socket
--
-- Return: 0 if successful.  !0 on error
=========================================================================== */
static int CloseServer(SOCKET m_socket) {
	shutdown(m_socket, SD_BOTH);
	closesocket(m_socket);
	return 0;
}	



/* ===========================================================================
-- atexit routinen to ensure we shut down the servers
=========================================================================== */
static void cleanup(void) {
	static char *rname = "cleanup";

	if (DCx_msg_socket_ok) {					/* Shut down the message based socket */
		CloseServer(DCx_msg_socket); 
		DCx_msg_socket_ok = FALSE;
	}

	/* And finally shutdown the sockets */
	Shutdown_Sockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}

/* ===========================================================================
-- Routine to make an exchange with the msg server
--
-- Usage: int Raw_DCx_Server_Exchange(DCX_MSG *request, void *data_send, 
--											     DCX_MSG *reply,  void **data_back)
--
-- Inputs: request     - pointer to buffer with information to be sent to server
--         data_send   - if request.data_len != 0 and data_send != NULL, additional data sent after the request block
--         reply_len   - size of the reply buffer (max number of bytes)
--
-- Output: *reply      - filled with response from the server
--
-- Return:  >0 - all okay and number of bytes returned in received message
--          -1 - Unable to create the access control semaphore
--          -2 - The socket is not okay.  Something crashed on initialization
--          -3 - TImeout waiting for the semaphore (already in use?)
--          -4 - The connection was apparently closed on the server side
--          -5 - Error in the recv function -- no data received
--
-- Notes: Sends request, gets response.  Limited error checking.
=========================================================================== */
#define	DCX_CLIENT_WAIT	(30000)		/* 30 second time-out */

static int Raw_DCx_Server_Exchange(DCX_MSG *request, void *data_send, DCX_MSG *reply, void **data_back) {
	static char *rname = "DCx_Msg_Server_Query";

	DCX_MSG msg;
	int rc, bytesSent, bytesRecv;

#ifdef DEBUG_ME
	fprintf(stderr, "Raw_DCx_Server_Exchange msg=%d  L1=%d L2=%d\n", ((REQUEST *) request)->msg, request_len, reply_len); fflush(stderr);
#endif

	/* Initial return values */
	if (data_back != NULL) *data_back = NULL;

/* Make sure we can do it */
	if (! DCx_msg_socket_ok) {
		fprintf(stderr, "ERROR[%s]: DCx message socket not okay.  Server not connected\n", rname); fflush(stderr);
		return -2;
	}

/* Get control of the server semaphore */
#ifdef _WIN32
	if (WaitForSingleObject(DCx_Client_Mutex, DCX_CLIENT_WAIT) != WAIT_OBJECT_0) {
		fprintf(stderr, "ERROR[%s]: Timeout waiting for the DCx semaphore\n", rname); fflush(stderr);
		return -2;
	}
#endif

/* Now, send the request and then additional data if specified in the request */
	memcpy(&msg, request, sizeof(DCX_MSG));
	msg.msg      = htonl(msg.msg);
	msg.msgid    = htonl(msg.msgid);
	msg.rc       = htonl(msg.rc);
	msg.data_len = htonl(msg.data_len);

	bytesRecv = SOCKET_ERROR;
	bytesSent = send(DCx_msg_socket, (char *) &msg, sizeof(DCX_MSG), 0);
	if (request->data_len != 0 && data_send != NULL) send(DCx_msg_socket, (char *) data_send, request->data_len, 0);

	rc = 0;											/* Assume success */
	while (bytesRecv == SOCKET_ERROR) {
		bytesRecv = recv(DCx_msg_socket, (char *) reply, sizeof(DCX_MSG), 0);
		if (bytesRecv == 0 || bytesRecv == WSAECONNRESET) {
			fprintf(stderr, "ERROR[%s]: DCx msg connection closed unexpectedly\n", rname); fflush(stderr);
			rc = -3; break;
		}
		if (bytesRecv < 0) { rc = -4; break; }
	}

	/* Convert the returned message and possibly grab more data if indicated */
	if (rc == 0) {
		reply->msg      = ntohl(reply->msg);
		reply->msgid    = ntohl(reply->msgid);
		reply->rc       = ntohl(reply->rc);
		reply->data_len = ntohl(reply->data_len);
		if (reply->data_len > 0) {
			void *buf;
			buf = calloc(1, reply->data_len);
			bytesRecv += recv(DCx_msg_socket, (char *) buf, reply->data_len, 0);
			if (data_back != NULL) *data_back = buf;
			if (data_back == NULL) free(buf);
		}
	}

#ifdef _WIN32
	ReleaseMutex(DCx_Client_Mutex);
#endif
	if (rc < 0) { fprintf(stderr, "ERROR[%s]: Returned error %d\n", rname, rc); fflush(stderr); }

#ifdef DEBUG_ME
	fprintf(stderr, "Raw_DCx_Server_Exchange(): msg=%d return %d: bytes_sent=%d  bytes_rcvd=%d\n", ((REQUEST *) request)->msg, rc, bytesSent, bytesRecv); fflush(stderr);
#endif
	
	return (rc == 0) ? bytesRecv : rc ;
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
