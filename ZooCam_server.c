/* ZooCam_server.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>						  /* for defining several useful types and macros */
#include <stdio.h>						  /* for performing input and output */
#include <stdlib.h>						  /* for performing a variety of operations */
#include <string.h>
#include <math.h>				           /* basic math functions */
#include <assert.h>
#include <stdint.h>					     /* C99 extension to get known width integers */

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "server_support.h"				/* Server support routine */
#include "camera.h"							/* Camera information */
#include "dcx.h"
#include "tl.h"
#include "ZooCam.h"							/* Access to the ZooCam info */
#include "Ki224.h"							/* Access to the current control */
#include "ZooCam_server.h"					/* Prototypes for main	  */
#include "ZooCam_client.h"					/* Version info and port  */

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

static int Remote_Get_Camera_Info(CAMERA_INFO *info);
static int Remote_Set_Exposure_Parms(int options, EXPOSURE_PARMS *request, EXPOSURE_PARMS *actual);
static int Remote_Ring_Actions(RING_ACTION request, int option, RING_INFO *response);
static int Remote_Get_Image_Info(int frame, IMAGE_INFO *info);
static int Remote_Get_Image_Data(int frame, void **data, size_t *length);

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
static BOOL ZooCam_Msg_Server_Up = FALSE;				/* Server has been started */
static HANDLE ZooCam_Server_Mutex = NULL;				/* access to the client/server communication */

int Init_ZooCam_Server(void) {
	static char *rname = "Init_ZooCam_Server";

	/* Don't start multiple times :-) */
	if (ZooCam_Msg_Server_Up) return 0;

/* Create mutex for work */
	if (ZooCam_Server_Mutex == NULL && (ZooCam_Server_Mutex = CreateMutex(NULL, FALSE, NULL)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Unable to create the server access semaphores\n", rname); fflush(stderr);
		return 1;
	}

/* Enable debug information */
	DebugSockets(2);									/* All warnings and errors */

/* Bring up the message based server */
	if ( ! (ZooCam_Msg_Server_Up = (RunServerThread("ZooCam", ZOOCAM_ACCESS_PORT, server_msg_handler, NULL) == 0)) ) {
		fprintf(stderr, "ERROR[%s]: Unable to start the ZooCam message based remote server\n", rname); fflush(stderr);
		return 2;
	}

	return 0;
}

/* ===========================================================================
-- Routine to shutdown high level DCx remote socket server
--
-- Usage: void Shutdown_ZooCam_Server(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_ZooCam_Server(void) {

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
	size_t length;

	/* These refer to the last captured image */
	IMAGE_INFO image_info;
	void *image_data;

	/* And more information buffers that get transferred - some need to be kept */
	CAMERA_INFO camera_info;
	EXPOSURE_PARMS exposure;
	TRIGGER_INFO trigger_info;
	RING_INFO ring_info;

/* Get standard request from client and process */
	ServerActive = TRUE;
	while (ServerActive && GetStandardServerRequest(block, &request, &received_data) == 0) {	/* Exit if client disappears */

		/* Create a default reply message */
		memcpy(&reply, &request, sizeof(reply));
		reply.rc = reply.data_len = 0;			/* All okay and no extra data */
		reply_data = NULL;							/* No extra data on return */

		/* Be very careful ... only allow one socket message to be in process at any time */
		/* The code should already protect, but not sure how interleaved messages may impact operations */
		if (WaitForSingleObject(ZooCam_Server_Mutex, ZOOCAM_SERVER_WAIT) != WAIT_OBJECT_0) {
			fprintf(stderr, "ERROR[%s]: Timeout waiting for the DCx semaphore\n", rname); fflush(stderr);
			reply.msg = -1; reply.rc = -1;

		} else switch (request.msg) {
			case SERVER_END:
				fprintf(stderr, "  ZooCam msg server: SERVER_END\n"); fflush(stderr);
				ServerActive = FALSE;
				break;

			case ZOOCAM_QUERY_VERSION:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_QUERY_VERSION()\n");	fflush(stderr);
				reply.rc = ZOOCAM_CLIENT_SERVER_VERSION;
				break;

			case ZOOCAM_GET_CAMERA_INFO:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_GET_CAMERA_INFO()\n");	fflush(stderr);
				reply.rc = Remote_Get_Camera_Info(&camera_info);			/* Get the information */
				reply.data_len = sizeof(camera_info);
				reply_data = (void *) &camera_info;
				break;

			case ZOOCAM_GET_EXPOSURE_PARMS:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_GET_EXPOSURE_PARMS()\n");	fflush(stderr);
				reply.rc = Remote_Set_Exposure_Parms(0, NULL, &exposure);
				reply.data_len = sizeof(exposure);
				reply_data = (void *) &exposure;
				break;

			case ZOOCAM_SET_EXPOSURE_PARMS:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_SET_EXPOSURE_PARMS()\n");	fflush(stderr);
				if (request.data_len < sizeof(EXPOSURE_PARMS)) received_data = NULL;
				reply.rc = Remote_Set_Exposure_Parms(request.option, received_data, &exposure);
				reply.data_len = sizeof(exposure);
				reply_data = (void *) &exposure;
				break;

			case ZOOCAM_GET_IMAGE_INFO:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_GET_IMAGE_INFO()\n");	fflush(stderr);
				reply.rc = Remote_Get_Image_Info(request.option, &image_info);
				reply.data_len = sizeof(image_info);
				reply_data = (void *) &image_info;
				break;

			case ZOOCAM_GET_IMAGE_DATA:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_GET_IMAGE_DATA()\n");	fflush(stderr);
				reply.rc = Remote_Get_Image_Data(request.option, &image_data, &length);
				reply.data_len = length;
				reply_data = (void *) image_data;
				break;

			case ZOOCAM_SAVE_FRAME:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_SAVE_FRAME()\n");	fflush(stderr);
				if (request.data_len < sizeof(FILE_SAVE_PARMS)) {
					reply.rc = 1;
				} else {
					FILE_SAVE_PARMS *parms;
					parms = (FILE_SAVE_PARMS *) received_data;
					reply.rc = Camera_SaveImage(NULL, parms->frame, parms->path, parms->format);
				}
				break;

			case ZOOCAM_SAVE_ALL:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_SAVE_ALL()\n");	fflush(stderr);
				if (request.data_len < sizeof(FILE_SAVE_PARMS)) {
					reply.rc = 1;
				} else {
					FILE_SAVE_PARMS *parms;
					parms = (FILE_SAVE_PARMS *) received_data;
					reply.rc = Camera_SaveAll(NULL, parms->path, parms->format);
				}
				break;

			case ZOOCAM_ARM:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_TRIGGER()\n");	fflush(stderr);
				reply.rc = Camera_Arm(NULL, request.option);
				break;

			case ZOOCAM_GET_TRIGGER_MODE:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_GET_TRIGGER_MODE()\n");	fflush(stderr);
				reply.rc = Camera_GetTriggerMode(NULL, &trigger_info);
				reply.data_len = sizeof(trigger_info);
				reply_data = (void *) &trigger_info;
				break;

			case ZOOCAM_SET_TRIGGER_MODE:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_SET_TRIGGER_MODE()\n");	fflush(stderr);
				if (request.data_len < sizeof(TRIGGER_INFO)) {
					memset(&trigger_info, 0, sizeof(trigger_info));
				} else {
					memcpy(&trigger_info, received_data, sizeof(trigger_info));
				}
				reply.rc = Camera_SetTriggerMode(NULL, request.option, &trigger_info);
				reply.data_len = sizeof(trigger_info);
				reply_data = (void *) &trigger_info;
				break;

			case ZOOCAM_TRIGGER:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_TRIGGER()\n");	fflush(stderr);
				reply.rc = Camera_Trigger(NULL);
				break;

			case ZOOCAM_RING_GET_INFO:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_RING_GET_INFO()\n");	fflush(stderr);
				reply.rc = Remote_Ring_Actions(RING_GET_INFO, 0, &ring_info);
				reply.data_len   = sizeof(ring_info);
				reply_data = (void *) &ring_info;
				break;
				
			case ZOOCAM_RING_GET_SIZE:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_RING_GET_SIZE()\n");	fflush(stderr);
				reply.rc = Remote_Ring_Actions(RING_GET_SIZE, 0, NULL);
				break;

			case ZOOCAM_RING_SET_SIZE:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_RING_SET_SIZE(%d)\n", request.option);	fflush(stderr);
				reply.rc = Remote_Ring_Actions(RING_SET_SIZE, request.option, NULL);
				break;

			case ZOOCAM_RING_RESET_COUNT:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_RING_RESET_COUNT\n");	fflush(stderr);
				Camera_ResetRingCounters(NULL);
				break;
				
			case ZOOCAM_RING_GET_FRAME_CNT:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_RING_GET_FRAME_CNT()\n");	fflush(stderr);
				reply.rc = Remote_Ring_Actions(RING_GET_ACTIVE_CNT, 0, NULL);
				break;

			case ZOOCAM_BURST_ARM:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_BURST_ARM()\n");	fflush(stderr);
				Burst_Actions(BURST_ARM, 0, &reply.rc);			/* Arm the burst */
				break;

			case ZOOCAM_BURST_ABORT:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_BURST_ABORT()\n");	fflush(stderr);
				Burst_Actions(BURST_ABORT, 0, &reply.rc);		/* Abort existing request */
				break;

			case ZOOCAM_BURST_STATUS:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_BURST_STATUS()\n");	fflush(stderr);
				Burst_Actions(BURST_STATUS, 0, &reply.rc);		/* Query the status */
				break;
				
			case ZOOCAM_BURST_WAIT:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_BURST_WAIT()\n");	fflush(stderr);
				Burst_Actions(BURST_WAIT, request.option, &reply.rc);	/* Wait for stripe to occur */
				break;
				
			/* 0 => off, 1 => on, otherwise no change; returns current on/off BOOL state */
			case ZOOCAM_LED_SET_STATE:
				fprintf(stderr, "  ZooCam msg server: ZOOCAM_LED_SET_STATE()\n");	fflush(stderr);
				reply.rc = Keith224_Output(request.option);
				break;

			default:
				fprintf(stderr, "ERROR: ZooCam server message received (%d) that was not recognized.\n"
						  "       Will be ignored with rc=-1 return code.\n", request.msg);
				fflush(stderr);
				reply.rc = -1;
				break;
		}
		ReleaseMutex(ZooCam_Server_Mutex);
		if (received_data != NULL) { free(received_data); received_data = NULL; }

		/* Send the standard response and any associated data */
		if (SendStandardServerResponse(block, reply, reply_data) != 0) {
			fprintf(stderr, "ERROR: ZooCam server failed to send response we requested.\n");
			fflush(stderr);
		}
	}

	EndServerHandler(block);								/* Cleanly exit the server structure always */
	return 0;
}



/* ===========================================================================
	==============================================================================
	-- Client/server support calls ... prefixed with Remote_
	==============================================================================
	=========================================================================== */

/* ===========================================================================
-- Client/server routine to query camera information (model, etc)
--
-- Usage: int Remote_Get_Camera_Info(CAMERA_INFO *info);
--
-- Inputs: info - pointer to variable to receive data
--
-- Output: *info - camera details
--
-- Return: 0 ==> successful
--         1 ==> no camera initialized
=========================================================================== */
static int Remote_Get_Camera_Info(CAMERA_INFO *info) {
	static char *rname = "Remote_GetCamera_Info";

	/* Equivalent to main routine ... just call with pointer to WND_INFO * */
	return Camera_GetCameraInfo(NULL, info);

}


/* ===========================================================================
-- Client/server routine to set/query exposure parameters
--
-- Usage: int Remote_Set_Exposure_Parms(int options, EXPOSURE_PARMS *request, EXPOSURE_PARMS *actual);
--
-- Inputs: options - OR'd bitwise flag indicating parameters to modify
--         request - pointer to structure with new values
--         actual  - pointer to receive actual settings (all updated)
--
-- Output: *actual - if ! NULL, values of all parameters after modification
--
-- Return: 0 ==> successful
--         1 ==> no camera initialized
--
-- Notes:
--    1) Parameters are validated so out-of-bound will not generate failure
--    2) exposure is prioritized if both DCX_MODIFY_EXPOSURE and DCX_MODIFY_FPS
--       are specified.  FPS will be modified only if lower than max possible
--       and camera has capbility
--    3) If MODIFY_EXPOSURE is given without MODIFY_FPS, maximum FPS inferred
--    4) MODIFY_BLUE_GAIN on a monochrome camera is a NOP
=========================================================================== */
static int Remote_Set_Exposure_Parms(int options, EXPOSURE_PARMS *request, EXPOSURE_PARMS *actual) {
	static char *rname = "Remote_Set_Exposure_Parms";

	EXPOSURE_PARMS mine;
	double rvals[4];

	/* Set response code to -1 to indicate major error */
	if (actual == NULL) actual = &mine;							/* So don't have to check */
	memset(actual, 0, sizeof(*actual));

	/* If we don't have data, we can't have any options for setting */
	if (request == NULL) options = 0;

	/* Should we change gamma? */
	if (options & MODIFY_GAMMA) Camera_SetGamma(NULL, request->gamma);

	/* Modify any of the gains */
	if (options & (MODIFY_MASTER_GAIN | MODIFY_RED_GAIN | MODIFY_GREEN_GAIN | MODIFY_BLUE_GAIN)) {
		if (options & MODIFY_MASTER_GAIN) Camera_SetGains(NULL, M_CHAN, IS_VALUE, request->master_gain);
		if (options & MODIFY_RED_GAIN)    Camera_SetGains(NULL, R_CHAN, IS_VALUE, request->red_gain);
		if (options & MODIFY_GREEN_GAIN)  Camera_SetGains(NULL, G_CHAN, IS_VALUE, request->green_gain);
		if (options & MODIFY_BLUE_GAIN)   Camera_SetGains(NULL, B_CHAN, IS_VALUE, request->blue_gain);
	}

	/* First set FPS if requested with expectation that EXPOSURE may need to change */
	if (options & (MODIFY_EXPOSURE || MODIFY_FPS)) {
		if (options & MODIFY_FPS)      Camera_SetFPSControl(NULL, request->fps);
		if (options & MODIFY_EXPOSURE) Camera_SetExposure(NULL, request->exposure);
	}

	/* Retrieve the current values now */
	actual->exposure = Camera_GetExposure(NULL);
	actual->fps      = Camera_GetFPSControl(NULL);
	actual->gamma    = Camera_GetGamma(NULL);
	Camera_GetGains(NULL, rvals, NULL);
	actual->master_gain = rvals[0];
	actual->red_gain    = rvals[1];
	actual->green_gain  = rvals[2];
	actual->blue_gain   = rvals[3];

	return 0;
}


/* ===========================================================================
-- Interface to the RING functions
--
-- Usage: int Remote_Ring_Actions(RING_ACTION request, int option, RING_INFO *response);
--
-- Inputs: hdlg    - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd     - handle to the main information structure
--         request - what to do
--           (0) RING_GET_INFO        ==> return structure ... also # of frames
--           (0) RING_GET_SIZE        ==> return number of frames in the ring
--           (1) RING_SET_SIZE        ==> set number of frames in the ring
--           (2) RING_GET_ACTIVE_CNT  ==> returns number of frames currently with data
--         option - For RING_SET_SIZE, desired number
--         response - pointer to for return of RING_INFO data
--
-- Output: *response - if !NULL, gets current RING_INFO data
--
-- Return: On error, -1 ==> or -2 ==> invalid request (not in enum)
--             RING_GET_INFO:       configured number of rings
--             RING_GET_SIZE:			configured number of rings
--		         RING_SET_SIZE:			new configured number of rings
--		         RING_GET_ACTIVE_CNT:	number of buffers with image data
=========================================================================== */
static int Remote_Ring_Actions(RING_ACTION request, int option, RING_INFO *response) {

	RING_INFO rings;

	/* First, make the request if valid */
	if (request == RING_SET_SIZE && option > 1) Camera_SetRingBufferSize(NULL, option);

	/* Always return the structure and then either nValid or nBuffers */
	Camera_GetRingInfo(NULL, &rings);
	if (response != NULL) memcpy(response, &rings, sizeof(*response));
	return (request == RING_GET_ACTIVE_CNT) ? rings.nValid : rings.nBuffers ;
}

/* ===========================================================================
-- Client/server routine to query image information
--
-- Usage: int Remote_Get_Image_Info(int frame, IMAGE_INFO *info);
--
-- Inputs: frame - which frame of data (-1 for current)
--         info  - pointer to variable to receive data
--
-- Output: *info - image details
--
-- Return: 0 ==> successful
--           1 ==> no camera initialized
--           2 ==> frame request invalid
=========================================================================== */
static int Remote_Get_Image_Info(int frame, IMAGE_INFO *info) {
	static char *rname = "Remote_GetCamera_Info";

	/* Equivalent to main routine ... just call with pointer to WND_INFO * */
	return Camera_GetImageInfo(NULL, frame, info);
}

/* ===========================================================================
-- Client/server routine to grab actual image data
--
-- Usage: int Remote_Get_Image_Data(int frame, void **data, size_t length);
--
-- Inputs: frame      - which frame of data (-1 for current)
--         image_data - pointer to receive data buffer pointer
--                      data may be unsigned byte * or unsigned short * depending on camera
--         length     - number of bytes in *data
--
-- Output: *image_data - pointer to a malloc'd buffer with raw image data
--                       calling routine responsible to release memory when done
--         *length     - number of bytes in *data
--
-- Return: 0 ==> successful
--           1 ==> no camera initialized
--           2 ==> frame request invalid
=========================================================================== */
static int Remote_Get_Image_Data(int frame, void **image_data, size_t *length) {
	static char *rname = "Remote_GetCamera_Info";

	void *image_memory;
	int rc;

	/* Equivalent to main routine ... just call with pointer to WND_INFO * */
	*image_data = NULL;
	rc = Camera_GetImageData(NULL, frame, &image_memory, length);
	if (*length >= 0 && image_memory != NULL) {
		*image_data = malloc(*length);
		memcpy(*image_data, image_memory, *length);
	}
	return rc;
}
