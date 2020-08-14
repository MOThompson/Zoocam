/* focus_client.c */

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

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "server_support.h"		/* Server support */
#include "focus_client.h"			/* For prototypes				*/

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static void cleanup(void);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

#ifdef LOCAL_CLIENT_TEST
	
int main(int argc, char *argv[]) {

//	DCX_IMAGE_INFO info;
//	DCX_STATUS status;
	unsigned char *data = NULL;
	int i, rc, client_version, server_version;
	double zposn;
	char *server_IP;

	server_IP = LOOPBACK_SERVER_IP_ADDRESS;	/* Local test (server on same machine) */
//	server_IP = "128.253.129.74";					/* Machine in laser room */
//	server_IP = "128.253.129.71";					/* Machine in open lab room */

	if (argc > 1) server_IP = argv[1];			/* Use the first argument on command line if given */

	if ( (rc = Init_Focus_Client(server_IP)) != 0) {
		fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP);
		return 0;
	}

	client_version = Focus_Remote_Query_Client_Version();
	server_version = Focus_Remote_Query_Server_Version();
	printf("Client/Server versions: %4.4d/%4.4d\n", client_version, server_version); fflush(stderr);
	if (client_version != server_version) {
		fprintf(stderr, "ERROR: Version mismatch between client and server.  Have to abort\n");
		return 3;
	}

	for (i=0; i<10; i++) {
		rc = Focus_Remote_Get_Focus_Posn(&zposn);
		printf("Query position: rc=%d  zposn=%g\n", rc, zposn); fflush(stdout);
		Sleep(1000);
	}

	return 0;
}

#endif		/* LOCAL_CLIENT_TEST */

/* ===========================================================================
-- Routine to open and initialize the socket to the Focus server
--
-- Usage: int Init_Focus_Client(char *IP_address);
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
static CLIENT_DATA_BLOCK *Focus_Remote = NULL;		/* Connection to the server */

int Init_Focus_Client(char *IP_address) {
	static char *rname = "Init_Focus_Client";
	int rc;
	int server_version;
	static BOOL first = TRUE;

	/* As this is the first message from clients normally, put out a NL so get over startup line */
	fprintf(stderr, "\n"); fflush(stderr);

	if (first) { atexit(cleanup); first = FALSE; }

	/* Shutdown sockets if already open (reinitialization allowed) */
	if (Focus_Remote != NULL) { CloseServerConnection(Focus_Remote); Focus_Remote = NULL; }

	if ( (Focus_Remote = ConnectToServer("Focus", IP_address, FOCUS_ACCESS_PORT, &rc)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Failed to connect to the server\n", rname); fflush(stderr);
		return -1;
	}

	/* Immediately check the version numbers of the client (here) and the server (other end) */
	/* Report an error if they do not match.  Code version must match */
	server_version = Focus_Remote_Query_Server_Version();
	if (server_version != FOCUS_CLIENT_SERVER_VERSION) {
		fprintf(stderr, "ERROR[%s]: Version mismatch between server (%d) and client (%d)\n", rname, server_version, FOCUS_CLIENT_SERVER_VERSION); fflush(stderr);
		CloseServerConnection(Focus_Remote); Focus_Remote = NULL;
		return 4;
	}

	/* Report success, and if not close everything that has been started */
	fprintf(stderr, "INFO: Connected to Focus server on %s\n", IP_address); fflush(stderr);
	return 0;
}

/* ===========================================================================
-- Quick routine to check for errors and print messages for TCP transaction
--
-- Usage: int Error_Check(int rc, CS_MSG *reply, int expect_msg);
--
-- Inputs: rc - return code from StandardServerExchange()
--         reply - message returned from the exchange
--         expect_msg - the message sent, which should be returned
--
-- Output: prints error message if error or mismatch
--
-- Return: 0 if no errors, otherwise -1
=========================================================================== */
static int Error_Check(int rc, CS_MSG *reply, int expect_msg) {

	if (rc != 0) {
		fprintf(stderr, "ERROR: Unexpected error from StandardServerExchange (rc=%d)\n", rc); fflush(stderr);
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
--	Usage:  int Focus_Remote_Query_Server_Version(void);
--         int Focus_Remote_Query_Client_Version(void);
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
int Focus_Remote_Query_Client_Version(void) {
	return FOCUS_CLIENT_SERVER_VERSION;
}

int Focus_Remote_Query_Server_Version(void) {
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = FOCUS_QUERY_VERSION;

	/* Get the response */
	rc = StandardServerExchange(Focus_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, FOCUS_QUERY_VERSION) != 0) return -1;

	return reply.rc;					/* Will be the version number */
}

/* ===========================================================================
--	Routine to return the status of the focus motor
--
--	Usage:  int Focus_Remote_Get_Focus_Status(int *status);
--
--	Inputs: status - pointer to variable to current status of the focus motor
--		
--	Output: *status - bitwise status flag
--
-- Return: 0 if successful
=========================================================================== */
int Focus_Remote_Get_Focus_Status(int *status) {
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = FOCUS_QUERY_Z_MOTOR_STATUS;

	/* Get the response */
	rc = StandardServerExchange(Focus_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, FOCUS_QUERY_Z_MOTOR_STATUS) != 0) return rc;

	if (status != NULL) *status = reply.option;

	return 0;
}

/* ===========================================================================
--	Routine to return the position of the focus motor
--
--	Usage:  int Focus_Remote_Get_Focus_Posn(double *zposn);
--
--	Inputs: zposn - pointer to variable to current z position of the focus motor
--		
--	Output: *zposn - position
--
-- Return: 0 if successful
=========================================================================== */
int Focus_Remote_Get_Focus_Posn(double *zposn) {
	CS_MSG request, reply;
	double *p_zposn;
	int rc;

	/* Fill in default response (no data) */
	if (zposn != NULL) *zposn = 0.0;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = FOCUS_QUERY_Z_MOTOR_POSN;

	/* Get the response */
	rc = StandardServerExchange(Focus_Remote, request, NULL, &reply, (void **) &p_zposn);
	if (Error_Check(rc, &reply, FOCUS_QUERY_Z_MOTOR_POSN) != 0) return rc;

	if (zposn != NULL) *zposn = *p_zposn;
	free(p_zposn);

	return 0;
}

/* ===========================================================================
--	Routine to set the position of the focus motor
--
--	Usage:  int Focus_Remote_Set_Focus_Posn(double zposn, BOOL wait);
--
--	Inputs: zposn - z position to be requested from the motor 
--         wait  - if TRUE, wait for move to complete, otherwise return immediately
--		
--	Output: none
--
-- Return: 0 if successful
=========================================================================== */
int Focus_Remote_Set_Focus_Posn(double zposn, BOOL wait) {
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = wait ? FOCUS_SET_Z_MOTOR_POSN_WAIT : FOCUS_SET_Z_MOTOR_POSN ;
	request.data_len = sizeof(double);

	/* Get the response */
	rc = StandardServerExchange(Focus_Remote, request, &zposn, &reply, NULL);
	if (Error_Check(rc, &reply, request.msg) != 0) return rc;

	return 0;
}

/* ===========================================================================
-- atexit routinen to ensure we shut down the servers
=========================================================================== */
static void cleanup(void) {
	static char *rname = "cleanup";

	if (Focus_Remote != NULL) { CloseServerConnection(Focus_Remote); Focus_Remote = NULL; }
	ShutdownSockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}
