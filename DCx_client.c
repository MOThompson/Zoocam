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

	DCX_STATUS status;
	DCX_EXPOSURE_PARMS exposure;
	DCX_RING_INFO rings;
	DCX_REMOTE_RING_IMAGE *ring_image;

	unsigned char *data = NULL;
	int client_version, server_version;
	int i, rc, nframes;
	int old_ring_size;
	char *server_IP;

//	server_IP = LOOPBACK_SERVER_IP_ADDRESS;	/* Local test (server on same machine) */
//	server_IP = "128.253.129.74";					/* zoo-chess.mse.cornell.edu */
	server_IP = "128.253.129.71";					/* zoo-lsa.mse.cornell.edu */

	if (argc > 1) server_IP = argv[1];			/* Use the first argument on command line if given */

	/* Enable debug information */
	DebugSockets(2);									/* All warnings and errors */

	if ( (rc = Init_DCx_Client(server_IP)) != 0) {
		fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP);
		return 0;
	}

	client_version = DCxRemote_Query_Client_Version();
	server_version = DCxRemote_Query_Server_Version();
	printf("Client/Server versions: %4.4d/%4.4d\n", client_version, server_version); fflush(stderr);
	if (client_version != server_version) {
		fprintf(stderr, "ERROR: Version mismatch between client and server.  Have to abort\n");
		return 3;
	}

	if ( (rc = DCxRemote_Get_Camera_Info(&status)) == 0) {
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

#if 0
	if ( (rc = DCxRemote_Acquire_Image(&info, &data)) == 0) {
		printf("Image information\n");
		printf("  width: %d   height: %d\n", info.width, info.height);
		printf("  memory row pitch: %d\n", info.memory_pitch);
		printf("  exposure: %.2f ms\n", info.exposure);
		printf("  gamma: %.2f\n", info.gamma);
		printf("  Gains: Master: %d   RGB: %d,%d,%d\n", info.master_gain, info.red_gain, info.green_gain, info.blue_gain);
		printf("  Color correction: %d %f\n", info.color_correction, info.color_correction_factor);
		printf("  Saturated pixels: Red: %d  Green: %d  Blue: %d\n", info.red_saturate, info.green_saturate, info.blue_saturate);
		for (i=0; i<10; i++) {
			int j;
			if (i >= info.height) break;
			printf("  Row %d:", i);
			for (j=0; j<15; j++) printf(" %3.3d", data[i*info.memory_pitch+j]);
			printf("\n");
		}
	} else {
		fprintf(stderr, "ERROR: Failed to acquire image and data\n"); fflush(stderr);
	}
	fflush(NULL);
#endif

	fprintf(stderr, "Exposure = 300 ms  (rc=%d)\n", DCxRemote_Set_Exposure(300, 0, &exposure)); fflush(stderr);
	fprintf(stderr, "Exposure=%.3f ms  fps=%.3f  gamma=%d  master=%d  rgb=(%d %d %d)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stderr);
	Sleep(300);

	fprintf(stderr, "Exposure = 50 ms  (rc=%d)\n", DCxRemote_Set_Exposure(50, 0, &exposure)); fflush(stderr);
	fprintf(stderr, "Exposure=%.3f ms  fps=%.3f  gamma=%d  master=%d  rgb=(%d %d %d)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stderr);

	fprintf(stderr, "Gains = 50  25,30,25 (rc=%d)\n", DCxRemote_Set_Gains(-1,50,30,25,35, &exposure)); fflush(stderr);
	fprintf(stderr, "Exposure=%.3f ms  fps=%.3f  gamma=%d  master=%d  rgb=(%d %d %d)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stderr);

	rc = DCxRemote_Get_Ring_Info(&rings);
	fprintf(stderr, "Ring info: nSize=%d  nValid=%d  iLast=%d  iShow=%d\n", rings.nSize, rings.nValid, rings.iLast, rings.iShow); fflush(stderr);
	fprintf(stderr, "Ring size: %d\n", DCxRemote_Get_Ring_Size()); fflush(stderr);
	fprintf(stderr, "Frame count: %d\n", DCxRemote_Get_Ring_Frame_Cnt()); fflush(stderr);

#if 1
	old_ring_size = DCxRemote_Get_Ring_Size();
	fprintf(stderr, "Set ring size: %d\n", DCxRemote_Set_Ring_Size(100)); fflush(stderr);

	fprintf(stderr, "Burst arm:    %d\n", DCxRemote_Burst_Arm());
	for (i=0; i<20; i++) {							/* Wait for up to 20 seconds for completion */
		if ( (rc = DCxRemote_Burst_Wait(1000)) == 0) break;
	}
	fprintf(stderr, "Burst wait:   %d\n", rc);
	rc = DCxRemote_Burst_Status();
	fprintf(stderr, "Burst status: %d\n", rc);
	if (rc != 3) {
		fprintf(stderr, "Burst failed to complete.  Aborting ...\n"); fflush(stderr);
		fprintf(stderr, "Burst abort:  %d\n", DCxRemote_Burst_Abort()); fflush(stderr);
		fprintf(stderr, "Burst status: %d\n", DCxRemote_Burst_Status()); fflush(stderr);
	} else {
//	fprintf(stderr, "Video on: %d\n", DCxRemote_Video_Enable(TRUE));

		rc = DCxRemote_Get_Ring_Info(&rings);
		fprintf(stderr, "Ring info: nSize=%d  nValid=%d  iLast=%d  iShow=%d\n", rings.nSize, rings.nValid, rings.iLast, rings.iShow); fflush(stderr);
		nframes = DCxRemote_Get_Ring_Frame_Cnt();
		fprintf(stderr, "Frame count: %d\n", nframes); fflush(stderr);

		for (i=0; i<nframes; i++) {
			rc = DCxRemote_Frame_N_Data(i, &ring_image);
			if (rc == 0 && ring_image != NULL) {
//				int j,k;
				fprintf(stderr, "[%d] %.3f  width: %d  height: %d  pitch: %d\n", i, ring_image->tstamp, ring_image->width, ring_image->height, ring_image->pitch); 
/**
				for (j=0; j<ring_image->height; j+=100) {
					printf("  Row %d:", j);
					for (k=0; k<15; k++) printf(" %3.3d", ring_image->data[j*ring_image->pitch+k]);
					printf("\n");
				}
				fflush(stdout);
**/
				free(ring_image);	ring_image = NULL;
			} else {
				fprintf(stderr, "[%d] failed.  rc=%d  ring_image=%p\n", i, rc, ring_image);
				break;
			}
			fflush(stderr);
		}
	}
	
	/* Reset rings and restart video */
	fprintf(stderr, "Exposure = 300 ms  (rc=%d)\n", DCxRemote_Set_Exposure(300, 0, &exposure)); fflush(stderr);
	fprintf(stderr, "Exposure=%.3f ms  fps=%.3f  gamma=%d  master=%d  rgb=(%d %d %d)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stderr);
	fprintf(stderr, "Set frames: %d\n", DCxRemote_Set_Ring_Size(old_ring_size)); fflush(stderr);
	fprintf(stderr, "Enabling live video: %d\n", DCxRemote_Video_Enable(TRUE)); fflush(stderr);
#endif

/* Exercise the LED */
	fprintf(stderr, "Turning on  LED: %d\n", DCxRemote_LED_Set_State(1));	fflush(stderr);
	Sleep(1000);
	fprintf(stderr, "Turning off LED: %d\n", DCxRemote_LED_Set_State(0));	fflush(stderr);

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
	server_version = DCxRemote_Query_Server_Version();
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
--	Usage:  int DCxRemote_Query_Server_Version(void);
--         int DCxRemote_Query_Client_Version(void);
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
int DCxRemote_Query_Client_Version(void) {
	return DCX_CLIENT_SERVER_VERSION;
}

int DCxRemote_Query_Server_Version(void) {
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
--	Usage:  int DCxRemote_Get_Camera_Info(DCX_STATUS *status);
--
--	Inputs: status - pointer to variable to receive information
--		
--	Output: *status filled with information
--
-- Return: 0 if successful, otherwise error code from call
--         1 ==> no camera connected
=========================================================================== */
int DCxRemote_Get_Camera_Info(DCX_STATUS *status) {
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

	/* Copy results (if valid) */
	if (my_status != NULL) {
		if (status != NULL) memcpy(status, my_status, sizeof(DCX_STATUS));
		free(my_status);
	}

	return reply.rc;					/* 0 or failure error code */
}

/* ===========================================================================
--	Routine to acquire an image (local save)
--
--	Usage:  int DCxRemote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);
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
int DCxRemote_Acquire_Image(DCX_IMAGE_INFO *info, char **image) {
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
	if (my_info != NULL) {
		if (info != NULL) memcpy(info, my_info, sizeof(DCX_IMAGE_INFO));
		free(my_info);
	}

	/* Get the actual image data (will be big) */
	memset(&request, 0, sizeof(request));
	request.msg = DCX_GET_CURRENT_IMAGE;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, image);
	if (Error_Check(rc, &reply, DCX_GET_IMAGE_DATA) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Query exposure information (exposure, fps, gains, gamma)
--
--	Usage:  int DCxRemote_Get_Exposure_Parms(DCX_EXPOSURE_PARMS *exposure);
--
--	Inputs: exposure - pointer to buffer for exposure information
-- 
--	Output: structure filled with exposure information (detail)
--
-- Return:  0 if successful, other error indication
--         -1 => Server exchange failed
--         On error *exposure will be zero
=========================================================================== */
int DCxRemote_Get_Exposure_Parms(DCX_EXPOSURE_PARMS *exposure) {
	CS_MSG request, reply;
	DCX_EXPOSURE_PARMS *my_parms = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (exposure  != NULL) memset(exposure, 0, sizeof(DCX_EXPOSURE_PARMS));

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_GET_EXPOSURE_PARMS;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, &my_parms);
	if (Error_Check(rc, &reply, DCX_GET_EXPOSURE_PARMS) != 0) return -1;

	/* Copy the info over to user space */
	if (my_parms != NULL) {
		if (exposure != NULL) memcpy(exposure, my_parms, sizeof(DCX_EXPOSURE_PARMS));
		free(my_parms);
	}

	return reply.rc;
}

/* ===========================================================================
--	Set the exposure and/or frames per second values
--
--	Usage:  int DCxRemote_Set_Exposure(double ms_expose, double fps, DCX_EXPOSURE_PARMS *rvalues);
--
--	Inputs: ms_expose - if >0, requested exposure time in ms
--         fps       - if >0, requested frames per second (exposure has priority if conflict)
--         rvalues - pointer to buffer for actual values after setting
-- 
--	Output: modifies values.
--         *rvalues - actual settings (full structure) resulting from call
--
-- Return: 0 if successful, other error indication
--         -1 => Server exchange failed
--         On error *rvalues will be zero
=========================================================================== */
int DCxRemote_Set_Exposure(double ms_exposure, double fps, DCX_EXPOSURE_PARMS *rvalues) {
	CS_MSG request, reply;
	DCX_EXPOSURE_PARMS parms, *my_parms = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (rvalues != NULL) memset(rvalues, 0, sizeof(DCX_EXPOSURE_PARMS));

	/* Modify the exposure and frames per second values */
	memset(&request, 0, sizeof(request));
	memset(&parms, 0, sizeof(parms));
	request.msg      = DCX_SET_EXPOSURE_PARMS;
	request.data_len = sizeof(parms);
	if (ms_exposure > 0) { request.option |= DCXF_MODIFY_EXPOSURE;  parms.exposure = ms_exposure; }
	if (fps         > 0) { request.option |= DCXF_MODIFY_FPS;       parms.fps = fps; }

	rc = StandardServerExchange(DCx_Remote, request, &parms, &reply, &my_parms);
	if (Error_Check(rc, &reply, DCX_SET_EXPOSURE_PARMS) != 0) return -1;

	/* Copy the info over to user space */
	if (my_parms != NULL) {
		if (rvalues != NULL) memcpy(rvalues, my_parms, sizeof(DCX_EXPOSURE_PARMS));
		free(my_parms);
	}

	return reply.rc;
}


/* ===========================================================================
--	Set the exposure gamma and gains
--
--	Usage:  int DCxRemote_Set_Gains(int gamma, int master, int red, int green, int blue, DCX_EXPOSURE_PARMS *rvalues);
--
--	Inputs: gamma  - if >=0, sets gamma value [0-100] range
--         master - if >=0, sets master gain value [0-100]
--         red    - if >=0, sets red channel gain value [0-100]
--         green  - if >=0, sets green channel gain value [0-100]
--         blue   - if >=0, sets blue channel gain value [0-100]
--         rvalues - pointer to buffer for actual values after setting
-- 
--	Output: modifies values.
--         *rvalues - actual settings (full structure) resulting from call
--
-- Return: 0 if successful, other error indication
--         On error *rvalues will be zero
=========================================================================== */
int DCxRemote_Set_Gains(int gamma, int master, int red, int green, int blue, DCX_EXPOSURE_PARMS *rvalues) {
	CS_MSG request, reply;
	DCX_EXPOSURE_PARMS parms, *my_parms = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (rvalues != NULL) memset(rvalues, 0, sizeof(DCX_EXPOSURE_PARMS));

	/* Modify the exposure and frames per second values */
	memset(&request, 0, sizeof(request));
	memset(&parms, 0, sizeof(parms));
	request.msg      = DCX_SET_EXPOSURE_PARMS;
	request.data_len = sizeof(parms);
	if (gamma  >= 0) { request.option |= DCXF_MODIFY_GAMMA;		  parms.gamma       = gamma; }
	if (master >= 0) { request.option |= DCXF_MODIFY_MASTER_GAIN; parms.master_gain = master; }
	if (red    >= 0) { request.option |= DCXF_MODIFY_RED_GAIN;    parms.red_gain    = red; }
	if (green  >= 0) { request.option |= DCXF_MODIFY_GREEN_GAIN;  parms.green_gain  = green; }
	if (blue   >= 0) { request.option |= DCXF_MODIFY_BLUE_GAIN;   parms.blue_gain   = blue; }
	
	rc = StandardServerExchange(DCx_Remote, request, &parms, &reply, &my_parms);
	if (Error_Check(rc, &reply, DCX_SET_EXPOSURE_PARMS) != 0) return -1;

	/* Copy the info over to user space */
	if (my_parms != NULL) {
		if (rvalues != NULL) memcpy(rvalues, my_parms, sizeof(DCX_EXPOSURE_PARMS));
		free(my_parms);
	}

	return reply.rc;
}


/* ===========================================================================
--	Query image ring buffer information (number, valid, current, etc.)
--
--	Usage:  int DCxRemote_Get_Ring_Info(DCX_RING_INFO *rings);
--         int DCxRemote_Get_Ring_Size(void);
--         int DCxRemote_Get_Ring_Frame_Cnt(void);
--
--	Inputs: rings - pointer to buffer for ring information
-- 
--	Output: structure filled with ring buffer information (detail)
--
-- Return: For DCxRemote_Get_Ring_Info, 
--             0 if successful, otherwise error with *ring set to zero
--         For others,
--             Returns requested value ... or -n on error
=========================================================================== */
int DCxRemote_Get_Ring_Info(DCX_RING_INFO *rings) {
	CS_MSG request, reply;
	DCX_RING_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (rings != NULL) memset(rings, 0, sizeof(DCX_RING_INFO));

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_RING_GET_INFO;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, &my_info);
	if (Error_Check(rc, &reply, DCX_RING_GET_INFO) != 0) return -1;

	/* Copy the info over to user space */
	if (my_info != NULL) {
		if (rings != NULL) memcpy(rings, my_info, sizeof(DCX_RING_INFO));
		free(my_info);
	}

	return reply.rc;
}

/* --------------------------------------------------------------------------- */
int DCxRemote_Get_Ring_Size(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_RING_GET_SIZE;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_RING_GET_SIZE) != 0) return -1;

	return reply.rc;
}

/* --------------------------------------------------------------------------- */
int DCxRemote_Get_Ring_Frame_Cnt(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_RING_GET_FRAME_CNT;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_RING_GET_FRAME_CNT) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Set ring buffer size
--
--	Usage:  int DCxRemote_Set_Ring_Size(int nbuf);
--
--	Inputs: nbuf - number of ring buffers desired
-- 
--	Output: resets the number (as long as >0)
--
-- Return: actual number of rings, or negative on errors
=========================================================================== */
int DCxRemote_Set_Ring_Size(int nbuf) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = DCX_RING_SET_SIZE;
	request.option = nbuf;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_RING_SET_SIZE) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Routine to acquire data for a specific frame from the ring
--
--	Usage:  int DCxRemote_Frame_N_Data(int frame, DCX_REMOTE_RING_IMAGE **image);
--
--	Inputs: frame - index of frame to transfer (0 based)
--         image - pointer to get malloc'd memory with the image information and data
--                 caller responsible for releasing this memory
-- 
--	Output: *image has pointer to frame information and data
--
-- Return: 0 if successful, other error indication
--         On error *image will be NULL
=========================================================================== */
int DCxRemote_Frame_N_Data(int frame, DCX_REMOTE_RING_IMAGE **image) {
	CS_MSG request, reply;
	int rc;

	/* Fill in default response (no data) */
	if (image != NULL) *image = NULL;

	/* Acquire the image */
	memset(&request, 0, sizeof(request));
	request.msg    = DCX_RING_IMAGE_N_DATA;
	request.option = frame;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, image);
	if (Error_Check(rc, &reply, DCX_RING_IMAGE_N_DATA) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Routines to handle the burst mode capture
--
--	Usage:  int DCxRemote_Burst_Arm(void);
--	        int DCxRemote_Burst_Abort(void);
--	        int DCxRemote_Burst_Status(void);
--	        int DCxRemote_Burst_Wait(int msTimeout);
--
--	Inputs: msTimeout - maximum time in ms to wait for the burst
--                     capture to start and complete. (<= 1000 ms)
-- 
--	Output: modifies Burst capture parameters
-
-- Return: All return -1 on error
--         DCxRemote_Burst_Arm and DCxRemote_Burst_Abort return 0 if successful
--         DCxRemote_Burst_Wait returns 0 if action complete, or 1 on timeout
--         DCxRemote_Burst_Status returns a flag value indication current status
--				(0) BURST_STATUS_INIT				Initial value on program start ... no request ever received
--				(1) BURST_STATUS_ARM_REQUEST		An arm request received ... but thread not yet running
--				(2) BURST_STATUS_ARMED				Armed and awaiting a stripe start message
--				(3) BURST_STATUS_RUNNING			In stripe run
--				(4) BURST_STATUS_COMPLETE			Stripe completed with images saved
--				(5) BURST_STATUS_ABORT				Capture was aborted
--				(6) BURST_STATUS_FAIL				Capture failed for other reason (no semaphores, etc.)
=========================================================================== */
int DCxRemote_Burst_Arm(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = DCX_BURST_ARM;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_BURST_ARM) != 0) return -1;

	return reply.rc;
}

int DCxRemote_Burst_Abort(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = DCX_BURST_ABORT;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_BURST_ABORT) != 0) return -1;

	return reply.rc;
}

int DCxRemote_Burst_Status(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = DCX_BURST_STATUS;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_BURST_STATUS) != 0) return -1;

	return reply.rc;
}

int DCxRemote_Burst_Wait(int msTimeout) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = DCX_BURST_WAIT;
	request.option = msTimeout;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_BURST_WAIT) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Routines to set and query the LED enable state
--
--	Usage:  int DCxRemote_LED_Set_State(int state);
--
--	Inputs: state - 0 ==> disable
--                 1 ==> enable
--                 other ==> just query
-- 
--	Output: Enables output on the Keithley 224 (if enabled)
--
-- Return: Returns -1 on client/server error
--         Otherwise TRUE (1) / FALSE (0) for current/new LED status
=========================================================================== */
int DCxRemote_LED_Set_State(int state) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg    = DCX_LED_SET_STATE;
	request.option = state;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_LED_SET_STATE) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Routines to set and query the live video state
--
--	Usage:  BOOL DCxRemote_Video_Enable(BOOL enable);
--
--	Inputs: enable  0 ==> attempt to "turn on" video
--                 1 ==> attempt to "turn off" video
--                 other ==> just query
-- 
--	Output: Optionally starts/stops the live video mode
--
-- Return: Returns -1 on client/server error
--         Otherwise TRUE (1) / FALSE (0) indicating if video is live
=========================================================================== */
int DCxRemote_Video_Enable(int state) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg    = DCX_VIDEO_ENABLE;
	request.option = state;
	rc = StandardServerExchange(DCx_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, DCX_VIDEO_ENABLE) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
-- atexit routine to ensure we shut down the servers
=========================================================================== */
static void cleanup(void) {
	static char *rname = "cleanup";

	if (DCx_Remote != NULL) { CloseServerConnection(DCx_Remote); DCx_Remote = NULL; }
	ShutdownSockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}
