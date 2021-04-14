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
#define DCX_GET_EXPOSURE_PARMS (6)		/* Query the current image capture parameters */
#define DCX_SET_EXPOSURE_PARMS (7)		/* Set one or more image capture parms / returns current values */

/* See DCx_Ring_Actions() for return values */
#define DCX_RING_GET_INFO		 (8)		/* Return DCX_REMOTE_RING_INFO structure with parameters for buffer rings */
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
#define DCX_VIDEO_ENABLE		 (18)		/* Enable / disable video on (option) */
#define DCX_SAVE_IMAGE_BUFFERS (19)		/* (NOT IMPLEMENTED) Save image buffers based on template pathname */

/* Structures */

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
int DCxRemote_Query_Client_Version(void);
int DCxRemote_Query_Server_Version(void);


/* ===========================================================================
--	Routine to return information on the camera
--
--	Usage:  int DCxRemote_Get_Camera_Info(DCX_STATUS *status);
--
--	Inputs: status - pointer to variable to receive information
--		
--	Output: *status filled with information
--
-- Return: 0 if successful
=========================================================================== */
int DCxRemote_Get_Camera_Info(DCX_STATUS *status);

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
int DCxRemote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);

/* ===========================================================================
--	Routine to set the camera acquisition time
--
--	Usage:  int DCxRemote_Set_Exposure(double exposure, BOOL maximize_framerate);
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
int DCxRemote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);

/* ===========================================================================
--	Query exposure information (exposure, fps, gains, gamma)
--
--	Usage:  int DCxRemote_Get_Exposure_Parms(DCX_EXPOSURE_PARMS *exposure);
--
--	Inputs: exposure - pointer to buffer for exposure information
-- 
--	Output: structure filled with exposure information (detail)
--
-- Return: 0 if successful, other error indication
--         On error *exposure will be zero
=========================================================================== */
int DCxRemote_Get_Exposure_Parms(DCX_EXPOSURE_PARMS *exposure);

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
--         On error *rvalues will be zero
=========================================================================== */
int DCxRemote_Set_Exposure(double ms_exposure, double fps, DCX_EXPOSURE_PARMS *rvalues);

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
int DCxRemote_Set_Gains(int gamma, int master, int red, int green, int blue, DCX_EXPOSURE_PARMS *rvalues);


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
int DCxRemote_Get_Ring_Info(DCX_RING_INFO *rings);
int DCxRemote_Get_Ring_Size(void);
int DCxRemote_Get_Ring_Frame_Cnt(void);

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
int DCxRemote_Set_Ring_Size(int nbuf);

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
int DCxRemote_Frame_N_Data(int frame, DCX_REMOTE_RING_IMAGE **image);


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
int DCxRemote_Burst_Arm(void);
int DCxRemote_Burst_Abort(void);
int DCxRemote_Burst_Status(void);
int DCxRemote_Burst_Wait(int msTimeout);

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
int DCxRemote_LED_Set_State(int state);

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
int DCxRemote_Video_Enable(int state);


#endif		/* _DCx_CLIENT_INCLUDED */
