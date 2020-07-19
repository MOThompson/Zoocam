/* DCx_client.c */

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
#include "DCx.h"						/* Access to the DCX info */
#include "DCx_client.h"				/* For prototypes				*/

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
	printf("Client/Server versions: %4.4d/%4.4d\n", client_version, server_version); fflush(stderr);
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

	return 0;
}

#endif		/* LOCAL_CLIENT_TEST */


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
static CLIENT_DATA_BLOCK *DCx_Remote = NULL;		/* Connection to the server */

int Init_DCx_Client(char *IP_address) {
	static char *rname = "Init_DCx_Client";
	int rc;
	int server_version;
	static BOOL first = TRUE;

	/* As this is the first message from clients normally, put out a NL so get over startup line */
	fprintf(stderr, "\n"); fflush(stderr);

	if (first) { atexit(cleanup); first = FALSE; }

	/* Shutdown sockets if already open (reinitialization allowed) */
	if (DCx_Remote != NULL) { CloseServerConnection(DCx_Remote); DCx_Remote = NULL; }

	if ( (DCx_Remote = ConnectToServer("DCx", IP_address, DCX_ACCESS_PORT, &rc)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Failed to connect to the server\n", rname); fflush(stderr);
		return -1;
	}

	/* Immediately check the version numbers of the client (here) and the server (other end) */
	/* Report an error if they do not match.  Code version must match */
	server_version = DCx_Remote_Query_Server_Version();
	if (server_version != DCX_CLIENT_SERVER_VERSION) {
		fprintf(stderr, "ERROR[%s]: Version mismatch between server (%d) and client (%d)\n", rname, server_version, DCX_CLIENT_SERVER_VERSION); fflush(stderr);
		CloseServerConnection(DCx_Remote); DCx_Remote = NULL;
		return 4;
	}

	/* Report success, and if not close everything that has been started */
	fprintf(stderr, "INFO: Connected to DCx server on %s\n", IP_address); fflush(stderr);
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
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = DCX_QUERY_VERSION;

	/* Get the response */
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
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
	CS_MSG request, reply;
	DCX_STATUS *my_status = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (status != NULL) memset(status, 0, sizeof(DCX_STATUS));

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_CAMERA_INFO;

	/* Get the response */
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, (void **) &my_status);
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
	CS_MSG request, reply;
	DCX_IMAGE_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (info  != NULL) memset(info, 0, sizeof(DCX_IMAGE_INFO));
	if (image != NULL) *image = NULL;

	/* Acquire the image */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_ACQUIRE_IMAGE;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_ACQUIRE_IMAGE) != 0) return -1;

	/* Acquire information about the image */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_IMAGE_INFO;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, &my_info);
	if (Error_Check(rc, &reply, DCX_GET_IMAGE_INFO) != 0) return -1;

	/* Copy the info over to user space */
	memcpy(info, my_info, sizeof(DCX_IMAGE_INFO));
	free(my_info);

	/* Get the actual image data (will be big) */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_IMAGE_DATA;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, image);
	if (Error_Check(rc, &reply, DCX_GET_IMAGE_DATA) != 0) return -1;

	return 0;
}

/* ===========================================================================
-- atexit routinen to ensure we shut down the servers
=========================================================================== */
static void cleanup(void) {
	static char *rname = "cleanup";

	if (DCx_Remote != NULL) { CloseServerConnection(DCx_Remote); DCx_Remote = NULL; }
	ShutdownSockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}
