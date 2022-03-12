/* ZooCam_client.c */

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
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#define	ZOOCAM_CLIENT

/* Not loading windows.h, so need to define HWND and BOOL to avoid problems */
#include "camera.h"					/* Generic camera information */

#include "server_support.h"		/* Server support */
#include "ZooCam.h"					/* Access to the ZooCam info */
#include "ZooCam_client.h"			/* For prototypes				*/

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

	CAMERA_INFO info;
	EXPOSURE_PARMS exposure, expose2;
	RING_INFO rings;

	IMAGE_INFO image_info;
	TRIGGER_INFO trigger_info;
	unsigned char *image_data;

	size_t length;

	int client_version, server_version;
	int i,j, rc, nframes;
	int old_ring_size;
	char *server_IP;
	double hold;

	server_IP = LOOPBACK_SERVER_IP_ADDRESS;	/* Local test (server on same machine) */
//	server_IP = "128.253.129.74";					/* zoo-chess.mse.cornell.edu */
//	server_IP = "128.253.129.71";					/* zoo-lsa.mse.cornell.edu */

	if (argc > 1) server_IP = argv[1];			/* Use the first argument on command line if given */

	/* Enable debug information */
	DebugSockets(2);									/* All warnings and errors */

	if ( (rc = Init_ZooCam_Client(server_IP)) != 0) {
		fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP);
		return 0;
	}

	client_version = ZooCam_Query_Client_Version();
	server_version = ZooCam_Query_Server_Version();
	printf("Client/Server versions: %4.4d/%4.4d\n", client_version, server_version); fflush(stderr);
	if (client_version != server_version) {
		fprintf(stderr, "ERROR: Version mismatch between client and server.  Have to abort\n");
		return 3;
	}

	/* Test querying camera information */
	if ( (rc = ZooCam_Get_Camera_Info(&info)) == 0) {
		printf("Camera information\n");
		printf("  Type: %s\n", info.type == CAMERA_DCX ? "DCx" : info.type == CAMERA_TL ? "TL" : "unknown");
		printf("  Name: %s\n", info.name);
		printf("  Manufacturer: %s\n", info.manufacturer);
		printf("  Model: %s\n", info.model);
		printf("  Serial: %s\n", info.serial);
		printf("  Version: %s\n", info.version);
		printf("  Date: %s\n", info.date);
		printf("  ColorMode: %s\n", info.bColor ? "color" : "monochrome");
		printf("  Pixel Pitch (um): %.2f x %.2f\n", info.x_pixel_um, info.y_pixel_um);
		fflush(stdout);
	} else {
		fprintf(stderr, "ERROR: Unable to get camera information\n"); fflush(stderr);
	}

	/* Test changing exposure time */
#if 0
	printf("Exposure information (rc=%d)\n", ZooCam_Get_Exposure(&exposure)); fflush(stdout);
	printf("   Exposure=%.3f ms  fps=%.3f  gamma=%f  master=%f  rgb=(%f %f %f)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stdout);
	hold = exposure.exposure;

	printf("Exposure = 300 ms  (rc=%d)\n", ZooCam_Set_Exposure(300, 0, &exposure)); fflush(stdout);
	printf("Exposure=%.3f ms  fps=%.3f  gamma=%.3f  master=%f  rgb=(%f %f %f)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stdout);
	printf("Trigger (rc=%d)\n", ZooCam_Trigger()); fflush(stdout);
	Sleep(1000);

	printf("Exposure = 50 ms  (rc=%d)\n", ZooCam_Set_Exposure(50, 0, &exposure)); fflush(stdout);
	printf("Exposure=%.3f ms  fps=%.3f  gamma=%f  master=%f  rgb=(%f %f %f)\n", exposure.exposure, exposure.fps, exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stdout);
	printf("Trigger (rc=%d)\n", ZooCam_Trigger()); fflush(stdout);
	Sleep(1000);

	printf("Reset exposure (rc=%d)\n", ZooCam_Set_Exposure(hold, 0, NULL)); fflush(stdout);
	printf("Trigger (rc=%d)\n", ZooCam_Trigger()); fflush(stdout);
	Sleep(1000);
#endif

	/* Test changing gains */
#if 0
	printf("Gain information (rc=%d): gamma=%.3f  master=%f  rgb=(%f %f %f)\n", ZooCam_Get_Gains(&exposure), exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stdout);
	expose2 = exposure;
	printf("Exposure = 20 ms (rc=%d)\n", ZooCam_Set_Exposure(20.0, 0, NULL)); fflush(stdout);
	printf("Set gains (50 25,30,25) (rc=%d): gamma=%.3f  master=%f  rgb=(%f %f %f)", ZooCam_Set_Gains(-1,50,30,25,35, &exposure), exposure.gamma, exposure.master_gain, exposure.red_gain, exposure.green_gain, exposure.blue_gain); fflush(stdout);
	printf("Trigger (rc=%d)\n", ZooCam_Trigger()); fflush(stdout);
	Sleep(1000);
	printf("Reset exposure (rc=%d)\n", ZooCam_Set_Exposure(hold, 0, NULL)); fflush(stdout);
	printf("Reset gains (rc=%d)\n", ZooCam_Set_Gains(expose2.gamma, expose2.master_gain, expose2.red_gain, expose2.green_gain, expose2.blue_gain, NULL)); fflush(stdout);
	printf("Trigger (rc=%d)\n", ZooCam_Trigger()); fflush(stdout);
	Sleep(1000);
#endif

	/* Test reading ring information */
#if 0
	rc = ZooCam_Get_Ring_Info(&rings);
	fprintf(stderr, "Ring info: nBuffers=%d  nValid=%d  iLast=%d  iShow=%d\n", rings.nBuffers, rings.nValid, rings.iLast, rings.iShow); fflush(stderr);
	fprintf(stderr, "Ring size: %d\n", ZooCam_Get_Ring_Size()); fflush(stderr);
	fprintf(stderr, "Frame count: %d\n", ZooCam_Get_Ring_Frame_Cnt()); fflush(stderr);
#endif

	/* Test changing ring size and collecting frames by triggering */
#if 0
	old_ring_size = ZooCam_Get_Ring_Size();
	printf("Set ring size: %d\n", ZooCam_Set_Ring_Size(50)); fflush(stdout);
	for (i=0; i<10; i++) { ZooCam_Trigger(); Sleep(200); }
	Sleep(1000);
	for (i=0; i<10; i++) { ZooCam_Trigger(); Sleep(200); }
	Sleep(1000);
	printf("Resetting ring size: %d\n", ZooCam_Set_Ring_Size(old_ring_size)); fflush(stdout);
#endif

	/* Test transferring images */
#if 0
	for (i=0; i<18; i++) { ZooCam_Trigger(); Sleep(200); }		/* Collect an unusual number */
	rc = ZooCam_Get_Image_Info(-1, &image_info);
	printf("Last image info (rc=%d): %s frame=%d times: %lld / %.3f  w/h: %d/%d pitch: %d exposure: %f\n", rc, 
			 info.type == CAMERA_DCX ? "DCx" : info.type == CAMERA_TL ? "TL" : "unknown",
			 image_info.frame,
			 image_info.timestamp, image_info.camera_time, 
			 image_info.width, image_info.height, image_info.memory_pitch, image_info.exposure);	fflush(stdout);
	for (i=0; i<ZooCam_Get_Ring_Frame_Cnt(); i++) {
		rc = ZooCam_Get_Image_Info(i, &image);
		printf("Image info %d: (rc=%d): %s frame=%d times: %lld / %.3f  w/h: %d/%d pitch: %d exposure: %f\n", 
				 i, rc,
				 info.type == CAMERA_DCX ? "DCx" : info.type == CAMERA_TL ? "TL" : "unknown",
				 image_info.frame,
				 image_info.timestamp, image_info.camera_time, 
				 image_info.width, image_info.height, image_info.memory_pitch, image_info.exposure);	
	}
	fflush(stdout);
#endif

	/* Test data transfer */
#if 0
	for (i=0; i<5; i++) { ZooCam_Trigger(); Sleep(200); }		/* Collect an unusual number */

	rc = ZooCam_Get_Image_Info(-1, &image_info);
	rc = ZooCam_Get_Image_Data(-1, &image_data, &length);
	printf("Last image data (rc=%d): image_data = %p  length = %d\n", rc, image_data, length); fflush(stdout);
	for (i=0; i<10; i++) {
		printf(" [%d]: ", i);
		for (j=0; j<10; j++) printf(" %.3d", image_data[image_info.memory_pitch*i+j]);
		printf("\n");
	}
	free(image_data);

	for (i=0; i<ZooCam_Get_Ring_Frame_Cnt(); i++) {
		rc = ZooCam_Get_Image_Data(i, &image_data, &length);
		printf("Image data %d: (rc=%d): image_data = %p  length = %d\n", i, rc, image_data, length); fflush(stdout);
		free(image_data);
	}
	fflush(stdout);
#endif
	
	/* Test saving data to files */
#if 0
	printf("Save file (rc=%d)\n", ZooCam_Save_Frame(-1, "burst/test.png", FILE_DFLT)); fflush(stdout);
	printf("Save file (rc=%d)\n", ZooCam_Save_Frame(-1, "burst/test.jpg", FILE_DFLT)); fflush(stdout);
	printf("Save file (rc=%d)\n", ZooCam_Save_Frame(-1, "burst/test.bmp", FILE_DFLT)); fflush(stdout);

	/* Test saving all the data */
	printf("Save burst (rc=%d)\n", ZooCam_Save_All("burst/basename", FILE_PNG)); fflush(stdout);
#endif

	/* Test arm / disarm */
#if 0
	printf("Current arm mode: %d\n", ZooCam_Arm(TRIG_ARM_QUERY)); fflush(stdout);
	printf("Arming: %d\n", ZooCam_Arm(TRIG_ARM)); fflush(stdout);
	Sleep(1000);
	printf("Disarming: %d\n", ZooCam_Arm(TRIG_DISARM)); fflush(stdout);
	Sleep(1000);
	printf("Arming: %d\n", ZooCam_Arm(TRIG_ARM)); fflush(stdout);
	Sleep(1000);
	printf("Current arm mode: %d\n", ZooCam_Arm(TRIG_ARM_QUERY)); fflush(stdout);
#endif

	rc = ZooCam_Get_Trigger_Mode(&trigger_info);
	printf("Trigger mode: %d  mode: %d armed: %d\n", rc, trigger_info.mode, trigger_info.bArmed); fflush(stdout);

	memset(&trigger_info, 0, sizeof(trigger_info));
	rc = ZooCam_Set_Trigger_Mode(TRIG_SOFTWARE, &trigger_info);
	printf("Trigger SOFTWARE: %d  mode: %d armed: %d\n", rc, trigger_info.mode, trigger_info.bArmed); fflush(stdout);
	Sleep(1000);
	for (i=0; i<5; i++) { ZooCam_Trigger(); Sleep(1000); }

	memset(&trigger_info, 0, sizeof(trigger_info));
	rc = ZooCam_Set_Trigger_Mode(TRIG_FREERUN, &trigger_info);
	printf("Trigger FREERUN: %d  mode: %d armed: %d\n", rc, trigger_info.mode, trigger_info.bArmed); fflush(stdout);

#if 0
	fprintf(stderr, "Burst arm:    %d\n", DCxZooCam_Burst_Arm());
	for (i=0; i<20; i++) {							/* Wait for up to 20 seconds for completion */
		if ( (rc = DCxZooCam_Burst_Wait(1000)) == 0) break;
	}
	fprintf(stderr, "Burst wait:   %d\n", rc);
	rc = DCxZooCam_Burst_Status();
	fprintf(stderr, "Burst status: %d\n", rc);
	if (rc != 3) {
		fprintf(stderr, "Burst failed to complete.  Aborting ...\n"); fflush(stderr);
		fprintf(stderr, "Burst abort:  %d\n", DCxZooCam_Burst_Abort()); fflush(stderr);
		fprintf(stderr, "Burst status: %d\n", DCxZooCam_Burst_Status()); fflush(stderr);
	} else {

	rc = ZooCam_Remote_Get_Ring_Info(&rings);
	fprintf(stderr, "Ring info: nBuffers=%d  nValid=%d  iLast=%d  iShow=%d\n", rings.nBuffers, rings.nValid, rings.iLast, rings.iShow); fflush(stderr);
	nframes = ZooCam_Remote_Get_Ring_Frame_Cnt();
	fprintf(stderr, "Frame count: %d\n", nframes); fflush(stderr);
#endif
		
/* Exercise the LED */
#if 0
	fprintf(stderr, "Turning on  LED: %d\n", ZooCam_LED_Set_State(1));	fflush(stderr);
	Sleep(1000);
	fprintf(stderr, "Turning off LED: %d\n", ZooCam_LED_Set_State(0));	fflush(stderr);
#endif

	return 0;
}

#endif		/* LOCAL_CLIENT_TEST */


/* ===========================================================================
-- Routine to open and initialize the socket to the ZooCam server
--
-- Usage: int Init_ZooCam_Client(char *IP_address);
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
static CLIENT_DATA_BLOCK *ZooCam_Remote = NULL;		/* Connection to the server */

int Init_ZooCam_Client(char *IP_address) {
	static char *rname = "Init_ZooCam_Client";
	int rc;
	int server_version;
	static BOOL first = TRUE;

	/* As this is the first message from clients normally, put out a NL so get over startup line */
	fprintf(stderr, "\n"); fflush(stderr);

	if (first) { atexit(cleanup); first = FALSE; }

	/* Shutdown sockets if already open (reinitialization allowed) */
	if (ZooCam_Remote != NULL) { CloseServerConnection(ZooCam_Remote); ZooCam_Remote = NULL; }

	if ( (ZooCam_Remote = ConnectToServer("ZooCam", IP_address, ZOOCAM_ACCESS_PORT, &rc)) == NULL) {
		fprintf(stderr, "ERROR[%s]: Failed to connect to the server\n", rname); fflush(stderr);
		return -1;
	}

	/* Immediately check the version numbers of the client (here) and the server (other end) */
	/* Report an error if they do not match.  Code version must match */
	server_version = ZooCam_Query_Server_Version();
	if (server_version != ZOOCAM_CLIENT_SERVER_VERSION) {
		fprintf(stderr, "ERROR[%s]: Version mismatch between server (%d) and client (%d)\n", rname, server_version, ZOOCAM_CLIENT_SERVER_VERSION); fflush(stderr);
		CloseServerConnection(ZooCam_Remote); ZooCam_Remote = NULL;
		return 4;
	}

	/* Report success, and if not close everything that has been started */
	fprintf(stderr, "INFO: Connected to ZooCam server on %s\n", IP_address); fflush(stderr);
	return 0;
}

/* ===========================================================================
-- Routine to shutdown cleanly an open interface to ZooCam server
--
-- Usage: int Shutdown_ZooCam_Client(void);
--
-- Inputs: none
--
-- Output: closes an open connection
--
-- Return:  0 - successful (and client was active)
--          1 - client already closed or never initialized
=========================================================================== */
int Shutdown_ZooCam_Client(void) {
	static char *rname = "Shutdown_ZooCam_Client"; 

	/* Nop if already closed */
	if (ZooCam_Remote == NULL) return 1;				/* Already closed */

	/* Shutdown sockets and mark closed */
	CloseServerConnection(ZooCam_Remote); 
	ZooCam_Remote = NULL;
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
--	Usage:  int ZooCam_Query_Server_Version(void);
--         int ZooCam_Query_Client_Version(void);
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
int ZooCam_Query_Client_Version(void) {
	return ZOOCAM_CLIENT_SERVER_VERSION;
}

int ZooCam_Query_Server_Version(void) {
	CS_MSG request, reply;
	int rc;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_QUERY_VERSION;

	/* Get the response */
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_QUERY_VERSION) != 0) return -1;

	return reply.rc;					/* Will be the version number */
}

/* ===========================================================================
--	Routine to return information on the camera
--
--	Usage:  int ZooCam_Get_Camera_Info(CAMERA_STATUS *status);
--
--	Inputs: status - pointer to variable to receive information
--		
--	Output: *status filled with information
--
-- Return: 0 if successful, otherwise error code from call
--         1 ==> no camera connected
=========================================================================== */
int ZooCam_Get_Camera_Info(CAMERA_INFO *info) {
	CS_MSG request, reply;
	CAMERA_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (info != NULL) memset(info, 0, sizeof(*info));

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_GET_CAMERA_INFO;

	/* Get the response */
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, (void **) &my_info);
	if (Error_Check(rc, &reply, ZOOCAM_GET_CAMERA_INFO) != 0) return rc;

	/* Copy results (if valid) */
	if (my_info != NULL) {
		if (info != NULL) memcpy(info, my_info, sizeof(*info));
		free(my_info);
	}

	return reply.rc;					/* 0 or failure error code */
}

/* ===========================================================================
--	Routine to return information on an image
--
--	Usage:  int ZooCam_Get_Image_Info(int frame, IMAGE_INFO *info);
--
--	Inputs: frame - image frame for information (-1 ==> current)
--         info  - pointer to structure to receive information
--		
--	Output: *info filled with information
--
-- Return: 0 if successful, otherwise error code from call
--           1 ==> no camera connected
--           2 ==> frame invalid
=========================================================================== */
int ZooCam_Get_Image_Info(int frame, IMAGE_INFO *info) {
	CS_MSG request, reply;
	IMAGE_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (info != NULL) memset(info, 0, sizeof(*info));

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_GET_IMAGE_INFO;
	request.option = frame;

	/* Get the response */
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, (void **) &my_info);
	if (Error_Check(rc, &reply, ZOOCAM_GET_IMAGE_INFO) != 0) return rc;

	/* Copy results (if valid) */
	if (my_info != NULL) {
		if (info != NULL) memcpy(info, my_info, sizeof(*info));
		free(my_info);
	}

	return reply.rc;					/* 0 or failure error code */
}


/* ===========================================================================
--	Routine to return data for the image
--
--	Usage:  int ZooCam_Get_Image_Data(int frame, unsigned char **data, size_t *length);
--
--	Inputs: frame  - image frame for information (-1 ==> current)
--         data   - pointer to get malloc'd raw data (caller must free())
--         length - pointer to get number of bytes in *data buffer
--		
--	Output: *data   - filled with raw data from the image
--         *length - filled with number of bytes in buffer
--
-- Return: 0 if successful, otherwise error code from call
--           1 ==> no camera connected
--           2 ==> frame invalid
=========================================================================== */
int ZooCam_Get_Image_Data(int frame, void **image_data, size_t *length) {
	CS_MSG request, reply;
	IMAGE_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (image_data != NULL) *image_data = NULL;
	if (length != NULL) *length = 0;

	/* Fill in the request */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_GET_IMAGE_DATA;
	request.option = frame;

	/* Get the response */
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, image_data);
	if (Error_Check(rc, &reply, ZOOCAM_GET_IMAGE_DATA) != 0) return rc;

	/* Copy results (if valid) */
	if (length != NULL) *length = (size_t) reply.data_len;
	
	return reply.rc;					/* 0 or failure error code */
}


/* ===========================================================================
--	Save a frame to specified filename in specified format
--
--	Usage:  int ZooCam_Save_Frame(int frame, char *path, FILE_FORMAT format);
--
--	Inputs: frame  - frame to save (-1 for last)
--         path   - filename (possibly fully qualified UNC)
--         format - format to save (FILE_DFLT will try to determine from path)
-- 
--	Output: saves a frame from ring buffers to a file
--
-- Return: 0 if successful, other error indication
--          -1 => Server exchange failed
--          >0 => value from Camera_SaveImage call
--
-- Note: If path is blank, will query via dialog box on remote computer.
--          This will block if remote is truly remote
=========================================================================== */
int ZooCam_Save_Frame(int frame, char *path, FILE_FORMAT format) {
	char static *rname = "ZooCam_Save_Frame";

	CS_MSG request, reply;
	FILE_SAVE_PARMS parms;
	int rc;

	/* Modify the exposure and frames per second values */
	memset(&request, 0, sizeof(request));
	memset(&parms, 0, sizeof(parms));
	request.msg = ZOOCAM_SAVE_FRAME;
	request.data_len = sizeof(parms);
	parms.frame = frame;
	parms.format = format;
	strcpy_s(parms.path, sizeof(parms.path), path);

	rc = StandardServerExchange(ZooCam_Remote, request, &parms, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_SAVE_FRAME) != 0) return -1;

	return reply.rc;
}


/* ===========================================================================
--	Save all valid frame using a specific filename pattern
--
--	Usage:  int ZooCam_Save_All(char *pattern, FILE_FORMAT format);
--
--	Inputs: pattern - root name for images ... append ddd.jpg
--         format  - format to save (FILE_DFLT will use FILE_BMP)
-- 
--	Output: saves all valid images in the ring to disk
--           <pattern>.xls will contain details on each file
--
-- Return: 0 if successful, other error indication
--          -1 => Server exchange failed
--          >0 => value from Camera_SaveImage call
=========================================================================== */
int ZooCam_Save_All(char *pattern, FILE_FORMAT format) {
	char static *rname = "ZooCam_Save_All";

	CS_MSG request, reply;
	FILE_SAVE_PARMS parms;
	int rc;

	/* Modify the exposure and frames per second values */
	memset(&request, 0, sizeof(request));
	memset(&parms, 0, sizeof(parms));
	request.msg = ZOOCAM_SAVE_ALL;
	request.data_len = sizeof(parms);
	parms.frame	 = 0;								/* Ignored */
	parms.format = format;
	strcpy_s(parms.path, sizeof(parms.path), pattern);

	rc = StandardServerExchange(ZooCam_Remote, request, &parms, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_SAVE_ALL) != 0) return -1;

	return reply.rc;
}



#if 0
/* ===========================================================================
--	Routine to acquire an image (local save)
--
--	Usage:  int DCxZooCam_Acquire_Image(DCX_IMAGE_INFO *info, char **image);
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
--         (1) ZOOCAM_ACQUIRE_IMAGE   [captures the image]
--         (2) ZOOCAM_GET_IMAGE_INFO  [transmits information about image]
--         (3) ZOOCAM_GET_IMAGE_DATA  [transmits actual image bytes]
=========================================================================== */
int DCxZooCam_Acquire_Image(IMAGE_INFO *info, char **image) {
	CS_MSG request, reply;
	IMAGE_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (info  != NULL) memset(info, 0, sizeof(IMAGE_INFO));
	if (image != NULL) *image = NULL;

	/* Acquire the image */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_ACQUIRE_IMAGE;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_ACQUIRE_IMAGE) != 0) return -1;

	/* Acquire information about the image */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_GET_IMAGE_INFO;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, &my_info);
	if (Error_Check(rc, &reply, ZOOCAM_GET_IMAGE_INFO) != 0) return -1;

	/* Copy the info over to user space */
	if (my_info != NULL) {
		if (info != NULL) memcpy(info, my_info, sizeof(IMAGE_INFO));
		free(my_info);
	}

	/* Get the actual image data (will be big) */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_GET_CURRENT_IMAGE;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, image);
	if (Error_Check(rc, &reply, ZOOCAM_GET_IMAGE_DATA) != 0) return -1;

	return reply.rc;
}
#endif

/* ===========================================================================
--	Query image capture information (exposure, fps, gains, gamma)
-- Since both are returned in one structure, have alternate entries for user
--
--	Usage:  int ZooCam_Get_Exposure(EXPOSURE_PARMS *exposure);
--	        int ZooCam_Get_Gains(EXPOSURE_PARMS *exposure);
--
--	Inputs: exposure - pointer to buffer for exposure information
-- 
--	Output: structure filled with exposure information (detail)
--
-- Return:  0 if successful, other error indication
--         -1 => Server exchange failed
--         On error *exposure will be zero
=========================================================================== */
static int ZooCam_Get_Gains(EXPOSURE_PARMS *exposure) {
	return ZooCam_Get_Exposure(exposure);
}

static int ZooCam_Get_Exposure(EXPOSURE_PARMS *exposure) {
	CS_MSG request, reply;
	EXPOSURE_PARMS *my_parms = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (exposure  != NULL) memset(exposure, 0, sizeof(EXPOSURE_PARMS));

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_GET_EXPOSURE_PARMS;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, &my_parms);
	if (Error_Check(rc, &reply, ZOOCAM_GET_EXPOSURE_PARMS) != 0) return -1;

	/* Copy the info over to user space */
	if (my_parms != NULL) {
		if (exposure != NULL) memcpy(exposure, my_parms, sizeof(EXPOSURE_PARMS));
		free(my_parms);
	}

	return reply.rc;
}

/* ===========================================================================
--	Set the exposure and/or frames per second values
--
--	Usage:  int ZooCam_Set_Exposure(double ms_expose, double fps, EXPOSURE_PARMS *rvalues);
--
--	Inputs: ms_expose - if >0, requested exposure time in ms
--         fps       - if >0, requested frames per second (exposure has priority if conflict)
--         rvalues   - pointer to buffer for actual values after setting (or NULL)
-- 
--	Output: modifies values.
--         *rvalues - actual settings (full structure) resulting from call
--
-- Return: 0 if successful, other error indication
--         -1 => Server exchange failed
--         On error *rvalues will be zero
=========================================================================== */
int ZooCam_Set_Exposure(double ms_exposure, double fps, EXPOSURE_PARMS *rvalues) {
	CS_MSG request, reply;
	EXPOSURE_PARMS parms, *my_parms = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (rvalues != NULL) memset(rvalues, 0, sizeof(EXPOSURE_PARMS));

	/* Modify the exposure and frames per second values */
	memset(&request, 0, sizeof(request));
	memset(&parms, 0, sizeof(parms));
	request.msg      = ZOOCAM_SET_EXPOSURE_PARMS;
	request.data_len = sizeof(parms);
	if (ms_exposure > 0) { request.option |= MODIFY_EXPOSURE;  parms.exposure = ms_exposure; }
	if (fps         > 0) { request.option |= MODIFY_FPS;       parms.fps = fps; }

	rc = StandardServerExchange(ZooCam_Remote, request, &parms, &reply, &my_parms);
	if (Error_Check(rc, &reply, ZOOCAM_SET_EXPOSURE_PARMS) != 0) return -1;

	/* Copy the info over to user space */
	if (my_parms != NULL) {
		if (rvalues != NULL) memcpy(rvalues, my_parms, sizeof(EXPOSURE_PARMS));
		free(my_parms);
	}

	return reply.rc;
}


/* ===========================================================================
--	Set imaging gamma and gains
--
--	Usage:  int ZooCam_Set_Gains(double gamma, double master, double red, double green, double blue,
--										  EXPOSURE_PARMS *rvalues);
--
--	Inputs: gamma  - if >=0, sets gamma value
--         master - if >=0, sets master gain value
--         red    - if >=0, sets red channel gain value
--         green  - if >=0, sets green channel gain value
--         blue   - if >=0, sets blue channel gain value
--         rvalues - pointer to buffer for actual values after setting
-- 
--	Output: modifies values.
--         *rvalues - actual settings (full structure) resulting from call
--
-- Return: 0 if successful, other error indication
--         On error *rvalues will be zero
=========================================================================== */
int ZooCam_Set_Gains(double gamma, double master, double red, double green, double blue, EXPOSURE_PARMS *rvalues) {
	CS_MSG request, reply;
	EXPOSURE_PARMS parms, *my_parms = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (rvalues != NULL) memset(rvalues, 0, sizeof(EXPOSURE_PARMS));

	/* Initialize the request */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_SET_EXPOSURE_PARMS;
	request.data_len = sizeof(parms);

	/* Copy values into a parameter block (use option to indicate which read) */
	memset(&parms, 0, sizeof(parms));
	parms.gamma       = gamma;
	parms.master_gain = master;
	parms.red_gain = red; parms.green_gain = green; parms.blue_gain = blue;
	if (gamma  >= 0) request.option |= MODIFY_GAMMA;
	if (master >= 0) request.option |= MODIFY_MASTER_GAIN;
	if (red    >= 0) request.option |= MODIFY_RED_GAIN;
	if (green  >= 0) request.option |= MODIFY_GREEN_GAIN;
	if (blue   >= 0) request.option |= MODIFY_BLUE_GAIN;
	
	rc = StandardServerExchange(ZooCam_Remote, request, &parms, &reply, &my_parms);
	if (Error_Check(rc, &reply, ZOOCAM_SET_EXPOSURE_PARMS) != 0) return -1;

	/* Copy the info over to user space */
	if (my_parms != NULL) {
		if (rvalues != NULL) memcpy(rvalues, my_parms, sizeof(EXPOSURE_PARMS));
		free(my_parms);
	}

	return reply.rc;
}


/* ===========================================================================
--	Query image ring buffer information (number, valid, current, etc.)
--
--	Usage:  int ZooCam_Get_Ring_Info(RING_INFO *rings);
--         int ZooCam_Get_Ring_Size(void);
--         int ZooCam_Get_Ring_Frame_Cnt(void);
--
--	Inputs: rings - pointer to buffer for ring information
-- 
--	Output: structure filled with ring buffer information (detail)
--
-- Return: For ZooCam_Get_Ring_Info, 
--             0 if successful, otherwise error with *ring set to zero
--         For others,
--             Returns requested value ... or -n on error
=========================================================================== */
int ZooCam_Get_Ring_Info(RING_INFO *rings) {
	static char *rname = "ZooCam_Get_Ring_Info";

	CS_MSG request, reply;
	RING_INFO *my_info = NULL;
	int rc;

	/* Fill in default response (no data) */
	if (rings != NULL) memset(rings, 0, sizeof(RING_INFO));

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_RING_GET_INFO;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, &my_info);
	if (Error_Check(rc, &reply, ZOOCAM_RING_GET_INFO) != 0) return -1;

	/* Copy the info over to user space */
	if (my_info != NULL) {
		if (rings != NULL) memcpy(rings, my_info, sizeof(RING_INFO));
		free(my_info);
	}

	return reply.rc;
}

/* --------------------------------------------------------------------------- */
int ZooCam_Get_Ring_Size(void) {
	static char *rname = "ZooCam_Get_Ring_Size";

	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_RING_GET_SIZE;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_RING_GET_SIZE) != 0) return -1;

	return reply.rc;
}

/* --------------------------------------------------------------------------- */
int ZooCam_Get_Ring_Frame_Cnt(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg   = ZOOCAM_RING_GET_FRAME_CNT;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_RING_GET_FRAME_CNT) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Set ring buffer size
--
--	Usage:  int ZooCam_Set_Ring_Size(int nbuf);
--
--	Inputs: nbuf - number of ring buffers desired
-- 
--	Output: resets the number (as long as >0)
--
-- Return: actual number of rings, or negative on errors
=========================================================================== */
int ZooCam_Set_Ring_Size(int nbuf) {
	static char *rname = "ZooCam_Set_Ring_Size";
	
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_RING_SET_SIZE;
	request.option = nbuf;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_RING_SET_SIZE) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Reset the ring buffer so next image will be in buffer 0
--
--	Usage:  int ZooCam_Reset_Ring_Count(void);
--
--	Inputs: nbuf - number of ring buffers desired
-- 
--	Output: resets the number (as long as >0)
--
-- Return: actual number of rings, or negative on errors
=========================================================================== */
int ZooCam_Reset_Ring_Count(void) {
	static char *rname = "ZooCam_Reset_Ring_Count";

	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_RING_RESET_COUNT;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_RING_RESET_COUNT) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Arm, disarm or quesry status
--
--	Usage: int ZooCam_Arm(TRIG_ARM_ACTION action);
--
--	Inputs: action - one of TRIG_ARM_QUERY, TRIG_ARM, TRIG_DISARM, 
-- 
--	Output: requaest change in status of camera
--
-- Return: current armed state (TRIG_ARM, TRIG_DISARM or TRIG_UNKNOWN)
=========================================================================== */
int ZooCam_Arm(TRIG_ARM_ACTION action) {
	static char *rname = "ZooCam_Arm";

	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_ARM;
	request.option = action;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_ARM) != 0) return -1;

	return reply.rc;
}


/* ===========================================================================
--	Query trigger information
--
--	Usage: TRIGGER_MODE ZooCam_Get_Trigger_Mode(TRIGGER_INFO *info);
--
--	Inputs: info - pointer to structure to receive information
-- 
--	Output: *info - filled with data if not NULL
--
-- Return: mode (or -1 on error)
=========================================================================== */
TRIGGER_MODE ZooCam_Get_Trigger_Mode(TRIGGER_INFO *info) {
	static char *rname = "ZooCam_Get_Trigger_Mode";

	CS_MSG request, reply;
	int rc;
	TRIGGER_INFO *myinfo;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_GET_TRIGGER_MODE;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, &myinfo);
	if (Error_Check(rc, &reply, ZOOCAM_GET_TRIGGER_MODE) != 0) return -1;

	/* Copy the info over to user space */
	if (myinfo != NULL) {
		if (info != NULL) memcpy(info, myinfo, sizeof(TRIGGER_INFO));
		free(myinfo);
	}

	return reply.rc;
}

/* ===========================================================================
--	Set trigger information
--
--	Usage: TRIGGER_MODE ZooCam_Set_Trigger_Mode(TRIGGER_MODE mode, TRIGGER_INFO *info);
--
--	Inputs: info - pointer to structure with possibly details for trigger
--                should be set to 0 to avoid changing anything but mode
-- 
--	Output: *info - filled with data if not NULL
--
-- Return: mode (or -1 on error)
=========================================================================== */
TRIGGER_MODE ZooCam_Set_Trigger_Mode(TRIGGER_MODE mode, TRIGGER_INFO *info) {
	static char *rname = "ZooCam_Get_Trigger_Mode";

	CS_MSG request, reply;
	int rc;
	TRIGGER_INFO *myinfo;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_SET_TRIGGER_MODE;
	request.option = mode;
	if (info != NULL) request.data_len = sizeof(TRIGGER_INFO);

	rc = StandardServerExchange(ZooCam_Remote, request, info, &reply, &myinfo);
	if (Error_Check(rc, &reply, ZOOCAM_SET_TRIGGER_MODE) != 0) return -1;

	/* Copy the info over to user space */
	if (myinfo != NULL) {
		if (info != NULL) memcpy(info, myinfo, sizeof(TRIGGER_INFO));
		free(myinfo);
	}

	return reply.rc;
}

/* ===========================================================================
--	Remotely trigger once
--
--	Usage: int ZooCam_Trigger(void);
--
--	Inputs: none
-- 
--	Output: sends request to trigger
--
-- Return: 0 on success
=========================================================================== */
int ZooCam_Trigger(void) {
	static char *rname = "ZooCam_Trigger";

	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_TRIGGER;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_TRIGGER) != 0) return -1;

	return reply.rc;
}


/* ===========================================================================
--	Routines to handle the burst mode capture
--
--	Usage:  int DCxZooCam_Burst_Arm(void);
--	        int DCxZooCam_Burst_Abort(void);
--	        int DCxZooCam_Burst_Status(void);
--	        int DCxZooCam_Burst_Wait(int msTimeout);
--
--	Inputs: msTimeout - maximum time in ms to wait for the burst
--                     capture to start and complete. (<= 1000 ms)
-- 
--	Output: modifies Burst capture parameters
-
-- Return: All return -1 on error
--         DCxZooCam_Burst_Arm and DCxZooCam_Burst_Abort return 0 if successful
--         DCxZooCam_Burst_Wait returns 0 if action complete, or 1 on timeout
--         DCxZooCam_Burst_Status returns a flag value indication current status
--				(0) BURST_STATUS_INIT				Initial value on program start ... no request ever received
--				(1) BURST_STATUS_ARM_REQUEST		An arm request received ... but thread not yet running
--				(2) BURST_STATUS_ARMED				Armed and awaiting a stripe start message
--				(3) BURST_STATUS_RUNNING			In stripe run
--				(4) BURST_STATUS_COMPLETE			Stripe completed with images saved
--				(5) BURST_STATUS_ABORT				Capture was aborted
--				(6) BURST_STATUS_FAIL				Capture failed for other reason (no semaphores, etc.)
=========================================================================== */
int DCxZooCam_Burst_Arm(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_BURST_ARM;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_BURST_ARM) != 0) return -1;

	return reply.rc;
}

int DCxZooCam_Burst_Abort(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_BURST_ABORT;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_BURST_ABORT) != 0) return -1;

	return reply.rc;
}

int DCxZooCam_Burst_Status(void) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_BURST_STATUS;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_BURST_STATUS) != 0) return -1;

	return reply.rc;
}

int DCxZooCam_Burst_Wait(int msTimeout) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg = ZOOCAM_BURST_WAIT;
	request.option = msTimeout;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_BURST_WAIT) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Routines to set and query the LED enable state
--
--	Usage:  int ZooCam_LED_Set_State(int state);
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
int ZooCam_LED_Set_State(int state) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg    = ZOOCAM_LED_SET_STATE;
	request.option = state;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_LED_SET_STATE) != 0) return -1;

	return reply.rc;
}

/* ===========================================================================
--	Routines to set and query the live video state
--
--	Usage:  BOOL DCxZooCam_Video_Enable(BOOL enable);
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
#if 0
int DCxZooCam_Video_Enable(int state) {
	CS_MSG request, reply;
	int rc;

	/* Query the exposure parameters */
	memset(&request, 0, sizeof(request));
	request.msg    = ZOOCAM_VIDEO_ENABLE;
	request.option = state;
	rc = StandardServerExchange(ZooCam_Remote, request, NULL, &reply, NULL);
	if (Error_Check(rc, &reply, ZOOCAM_VIDEO_ENABLE) != 0) return -1;

	return reply.rc;
}
#endif

/* ===========================================================================
-- atexit routine to ensure we shut down the servers
=========================================================================== */
static void cleanup(void) {
	static char *rname = "cleanup";

	if (ZooCam_Remote != NULL) { CloseServerConnection(ZooCam_Remote); ZooCam_Remote = NULL; }
	ShutdownSockets();
	fprintf(stderr, "Performed socket shutdown activities\n"); fflush(stderr);

	return;
}
