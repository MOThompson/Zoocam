/* DCx_server.c */

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
#include "server_support.h"		/* Server support routine */
#include "DCx.h"						/* Access to the DCX info */
#include "DCx_server.h"				/* Prototypes for main	  */
#include "DCx_client.h"				/* Version info and port  */

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
static int server_msg_handler(SERVER_DATA_BLOCK *block);

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
--            1 ==> Unable to create mutex
--            2 ==> Unable to spawn the server thread
=========================================================================== */
static BOOL DCx_Msg_Server_Up = FALSE;				/* Server has been started */
static HANDLE DCx_Server_Mutex = NULL;				/* access to the client/server communication */

int Init_DCx_Server(void) {
	static char *rname = "Init_DCx_Server";

	/* Don't start multiple times :-) */
	if (DCx_Msg_Server_Up) return 0;

/* Create mutex for work */
	if (DCx_Server_Mutex == NULL && (DCx_Server_Mutex = CreateMutex(NULL, FALSE, NULL)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Unable to create the server access semaphores\n", rname); fflush(stderr);
		return 1;
	}

/* Bring up the message based server */
	if ( ! (DCx_Msg_Server_Up = (RunServerThread("DCx", DCX_ACCESS_PORT, server_msg_handler, NULL) == 0)) ) {
		fprintf(stderr, "ERROR[%s]: Unable to start the DCx message based remote server\n", rname); fflush(stderr);
		return 2;
	}

	return 0;
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

	return 0;
}

/* ===========================================================================
-- Actual server routine to process messages received on the open socket.
--
-- Usage: int server_msg_handler(SERVER_DATA_BLOCK *block)
--
-- Inputs: socket - an open socket for exchanging messages with the client
--
-- Output: whatever needs to be done
--
-- Return: 0 ==> close this socket but continue listening for new clients
--         1 ==> Timeout waiting for semaphore
--         2 ==> socket appears to have been abandoned
=========================================================================== */
static int server_msg_handler(SERVER_DATA_BLOCK *block) {
	static char *rname = "server_msg_handler";

	CS_MSG request, reply;
	void *received_data, *reply_data;
	BOOL ServerActive;

	DCX_STATUS camera_info;

	/* These refer to the last captured image */
	static DCX_IMAGE_INFO image_info;
	static char *image_data;

/* Get standard request from client and process */
	ServerActive = TRUE;
	while (ServerActive && GetStandardServerRequest(block, &request, &received_data) == 0) {	/* Exit if client disappears */

		/* Create a default reply message */
		memcpy(&reply, &request, sizeof(reply));
		reply.rc = reply.data_len = 0;			/* All okay and no extra data */
		reply_data = NULL;							/* No extra data on return */

		/* Be very careful ... only allow one socket message to be in process at any time */
		/* The code should already protect, but not sure how interleaved messages may impact operations */
		if (WaitForSingleObject(DCx_Server_Mutex, DCX_SERVER_WAIT) != WAIT_OBJECT_0) {
			fprintf(stderr, "ERROR[%s]: Timeout waiting for the DCx semaphore\n", rname); fflush(stderr);
			reply.msg = -1; reply.rc = -1;

		} else switch (request.msg) {
			case SERVER_END:
				fprintf(stderr, "  DCx msg server: SERVER_END\n"); fflush(stderr);
				ServerActive = FALSE;
				break;

			case DCX_QUERY_VERSION:
				fprintf(stderr, "  DCx msg server: DCX_QUERY_VERSION()\n");	fflush(stderr);
				reply.rc = DCX_CLIENT_SERVER_VERSION;
				break;

			case DCX_GET_CAMERA_INFO:
				fprintf(stderr, "  DCx msg server: DCX_GET_CAMERA_INFO()\n");	fflush(stderr);
				reply.rc = DCx_Status(&camera_info);			/* Get the information */
				reply.data_len = sizeof(camera_info);
				reply_data = (void *) &camera_info;
				break;

			case DCX_ACQUIRE_IMAGE:
				fprintf(stderr, "  DCx msg server: DCX_ACQUIRE_IMAGE()\n");	fflush(stderr);
				if (image_data != NULL) { free(image_data); image_data = NULL; }			/* Free last one */
				reply.rc = DCx_Acquire_Image(&image_info, &image_data);	/* Grab an image now */
				reply.data_len = 0;
				break;

			case DCX_GET_IMAGE_INFO:
				fprintf(stderr, "  DCx msg server: DCX_GET_IMAGE_INFO()\n");	fflush(stderr);
				if (image_data == NULL) {
					reply.rc = -1;
				} else {
					reply.rc = 0; 
					reply.data_len = sizeof(DCX_IMAGE_INFO);
					reply_data = (void *) &image_info;				}
				break;

			case DCX_GET_IMAGE_DATA:
				fprintf(stderr, "  DCx msg server: DCX_GET_IMAGE_DATA()\n");	fflush(stderr);
				if (image_data == NULL) {
					reply.rc = -1;
				} else {
					reply.rc = 0; 
					reply.data_len = image_info.memory_pitch * image_info.height;
					reply_data = (void *) image_data;				}
				break;

			default:
				fprintf(stderr, "ERROR: DCx server message received (%d) that was not recognized.\n"
						  "       Will be ignored with rc=-1 return code.\n", request.msg);
				fflush(stderr);
				reply.rc = -1;
				break;
		}
		ReleaseMutex(DCx_Server_Mutex);
		if (received_data != NULL) { free(received_data); received_data = NULL; }

		/* Send the standard response and any associated data */
		if (SendStandardServerResponse(block, reply, reply_data) != 0) {
			fprintf(stderr, "ERROR: DCx server failed to send response we requested.\n");
			fflush(stderr);
		}
	}

	EndServerHandler(block);								/* Cleanly exit the server structure always */
	return 0;
}
