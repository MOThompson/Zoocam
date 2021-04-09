#ifndef _DCX_CLIENT_INCLUDED

#define	_DCX_CLIENT_INCLUDED

/* ===========================================================================
-- IT IS CRITICAL THAT THE VERSION BE MODIFEID anytime code changes and
-- would BREAK EXISTING COMPILATIONS.  Version is checked by the client
-- open routine, so as long as this changes, don't expect problems.
=========================================================================== */
#define	DCX_CLIENT_SERVER_VERSION	(1002)			/* Version of this code */

/* =============================
-- Port that the server runs
============================= */
#define	DCX_ACCESS_PORT		(1916)				/* Port for client/server connections */

#define	DFLT_SERVER_IP_ADDRESS	"128.253.129.93"		/* "127.0.0.1" for loop-back */
#define	LOOPBACK_SERVER_IP_ADDRESS	"127.0.0.1"			/* Server on this computer */

/* List of the allowed requests */
#define SERVER_END				 (0)		/* Shut down server (please don't use) */
#define DCX_QUERY_VERSION		 (1)		/* Return version of the server code */
#define DCX_GET_CAMERA_INFO	 (2)		/* Return structure with camera data */
#define DCX_ACQUIRE_IMAGE		 (3)		/* Acquire image only (local storage) */
#define DCX_GET_IMAGE_INFO		 (4)		/* Return info on the image acquired by DCX_ACQUIRE_IMAGE */
#define DCX_GET_CURRENT_IMAGE	 (5)		/* Transfer the actual image data from DCX_ACQUIRE_IMAGE */
	#define DCX_GET_IMAGE_DATA	 (5)		/* Older #define for DCX_GET_CURRENT_IMAGE */
#define DCX_GET_CAMERA_PARMS	 (6)		/* Query the current image capture parameters */
#define DCX_SET_CAMERA_PARMS 	 (7)		/* Set one or more of the image capture parameters */


/* See DCx_Ring_Actions() for return values */
#define DCX_RING_INFO			 (8)		/* Return DCX_REMOTE_RING_INFO structure with parameters for buffer rings */
#define DCX_RING_GET_SIZE		 (9)		/* Return current number of buffers (maximum frames) in the ring */
#define DCX_RING_SET_SIZE		 (10)		/* Set the number of buffers in the ring (will reset all frames) */
#define DCX_RING_GET_FRAME_CNT (11)		/* Get number of frames in ring that are active */
#define DCX_RING_IMAGE_N_DATA	 (12)		/* Return image data for frame buffer N (option) */

/* See DCx_Burst_Actions() for return values */
#define DCX_BURST_ARM			 (13)		/* Arm the burst (returns immediately) */
#define DCX_BURST_ABORT			 (14)		/* Abort awaiting burst (returns immediately) */
#define DCX_BURST_STATUS		 (15)		/* Query status of the burst (is it armed, complete, etc.) */
#define DCX_BURST_WAIT			 (16)		/* Wait for burst to complete (request.option = msTimeout) */

#define DCX_LED_SET_STATE		 (17)		/* Set LED current supply either on or off (or query) */
#define DCX_SAVE_IMAGE_BUFFERS (18)		/* Save image buffers based on template pathname */

/* Structures */
#pragma pack(4)
typedef struct _DCX_REMOTE_RING_INFO {
	int nRing,									/* Number of buffers in the ring */
	nLast,										/* Last buffer index used (from events) */
	nShow,										/* Currently display frame */
	nValid;										/* Highest buffer index used since reset */
} DCX_REMOTE_RING_INFO;
#pragma pack()

#pragma pack(4)
typedef struct _DCX_REMOTE_RING_IMAGE {
	double tstamp;								/* Time of image (seconds w/ ms resolution) */
	int width, height;						/* Image size */
	int pitch;									/* Byte offset between rows (also gives bytes/pixel) */
	char data[0];								/* width x height data immediately follows */
} DCX_REMOTE_RING_IMAGE;
#pragma pack()

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
--
-- Notes: Must be called before any attempt to communicate across the socket
=========================================================================== */
int Init_DCx_Client(char *IP_address);

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
int Shutdown_DCx_Client(void);

/* ===========================================================================
--	Routine to return current version of this code
--
--	Usage:  int DCx_Query_Server_Version(void);
--         int DCx_Query_Client_Version(void);
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
int DCx_Remote_Query_Client_Version(void);
int DCx_Remote_Query_Server_Version(void);


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
int DCx_Remote_Get_Camera_Info(DCX_STATUS *status);

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
int DCx_Remote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);

/* ===========================================================================
--	Routine to set the camera acquisition time
--
--	Usage:  int DCx_Remote_Set_Exposure(double exposure, BOOL maximize_framerate);
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
int DCx_Remote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);

#endif		/* _DCx_CLIENT_INCLUDED */
