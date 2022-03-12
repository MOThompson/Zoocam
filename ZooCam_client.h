#ifndef _ZOOCAM_CLIENT_INCLUDED

#define	_ZOOCAM_CLIENT_INCLUDED

/* ===========================================================================
-- IT IS CRITICAL THAT THE VERSION BE MODIFEID anytime code changes and
-- would BREAK EXISTING COMPILATIONS.  Version is checked by the client
-- open routine, so as long as this changes, don't expect problems.
=========================================================================== */
#define	ZOOCAM_CLIENT_SERVER_VERSION	(2001)	/* v.2 with generic camera support */

/* =============================
-- Port that the server runs
============================= */
#define	ZOOCAM_ACCESS_PORT		(1916)					/* Port for client/server connections */

#define	DFLT_SERVER_IP_ADDRESS	"128.253.129.93"		/* "127.0.0.1" for loop-back */
#define	LOOPBACK_SERVER_IP_ADDRESS	"127.0.0.1"			/* Server on this computer */

/* List of the allowed requests */
#define SERVER_END					 (0)		/* Shut down server (please don't use) */
#define ZOOCAM_QUERY_VERSION		 (1)		/* Return version of the server code */
#define ZOOCAM_GET_CAMERA_INFO	 (2)		/* Return structure with camera data */
#define ZOOCAM_GET_EXPOSURE_PARMS (3)		/* Query the current image capture parameters */
#define ZOOCAM_SET_EXPOSURE_PARMS (4)		/* Set one or more image capture parms / returns current values */

#define ZOOCAM_TRIGGER				 (5)		/* Trigger the camera (action depends on trigger setting) */
#define ZOOCAM_GET_TRIGGER_MODE	 (6)		/* Query trigger mode and parameters */
#define ZOOCAM_SET_TRIGGER_MODE	 (7)		/* Set trigger mode and/or parameters */
#define ZOOCAM_ARM					 (8)		/* Arm, disarm, or quesry trigger arm status */

#define ZOOCAM_GET_IMAGE_INFO		 (9)		/* Return info on specific (or last) frame in ring */
#define ZOOCAM_GET_IMAGE_DATA    (10)		/* Return data for specific (or last) frame in ring */
#define ZOOCAM_SAVE_FRAME			(11)		/* Save a specific frame to file in a specified format */
#define ZOOCAM_SAVE_ALL				(12)		/* Save all valid frames to files in specified format */

/* See DCx_Ring_Actions() for return values */
#define ZOOCAM_RING_GET_INFO		 (13)		/* Return DCX_ZooCam_RING_INFO structure with parameters for buffer rings */
#define ZOOCAM_RING_GET_SIZE		 (14)		/* Return current number of buffers (maximum frames) in the ring */
#define ZOOCAM_RING_SET_SIZE		 (15)		/* Set the number of buffers in the ring (will reset all frames) */
#define ZOOCAM_RING_RESET_COUNT	 (16)		/* Reset the counter in ring beffering to 0 (no active images) */
#define ZOOCAM_RING_GET_FRAME_CNT (17)		/* Get number of frames in ring that are active */

/* See DCx_Burst_Actions() for return values */
#define ZOOCAM_BURST_ARM			 (18)		/* Arm the burst (returns immediately) */
#define ZOOCAM_BURST_ABORT			 (19)		/* Abort awaiting burst (returns immediately) */
#define ZOOCAM_BURST_STATUS		 (20)		/* Query status of the burst (is it armed, complete, etc.) */
#define ZOOCAM_BURST_WAIT			 (21)		/* Wait for burst to complete (request.option = msTimeout) */

#define ZOOCAM_LED_SET_STATE		 (22)		/* Set LED current supply either on or off (or query) */

/* Structure for saving single frame or all frames */
#pragma pack(4)
typedef struct _FILE_SAVE_PARMS {
	int frame;										/* Frame number of -1 for current (ignored for SAVE_ALL) */
	FILE_FORMAT format;							/* File format (FILE_BMP, FILE_RAW, etc.) */
	char path[260];								/* Path for SAVE_FRAME, or pattern for SAVE_ALL */
} FILE_SAVE_PARMS;
#pragma pack()

/* Structures for query/modify exposure and gain settings */
#pragma pack(4)
/* Or'd bit-flags in option to control setting parameters */
/* Exposure always has priority over FPS, but FPS will be maximized if not modified with exposure */
#define	MODIFY_EXPOSURE		(0x01)			/* Modify exposure (value in ms) */
#define	MODIFY_FPS				(0x02)			/* Modify frames per second */
#define	MODIFY_GAMMA			(0x04)			/* Modify gamma */
#define	MODIFY_MASTER_GAIN	(0x08)			/* Modify master gain */
#define	MODIFY_RED_GAIN		(0x10)			/* Red channel gain */
#define	MODIFY_GREEN_GAIN		(0x20)			/* Green channel gain */
#define	MODIFY_BLUE_GAIN		(0x40)			/* Blue channel gain */
typedef struct _EXPOSURE_PARMS {
	double exposure;									/* Exposure time in ms				*/
	double fps;											/* Frame rate (per second)			*/
	double gamma;										/* Gamma value (0 < gamma < 100)	*/
	double master_gain;								/* Master gain (0 < gain < 100)	*/
	double red_gain, green_gain, blue_gain;	/* Individual channel gains		*/
} EXPOSURE_PARMS;
#pragma pack()

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
int Init_ZooCam_Client(char *IP_address);

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
int Shutdown_ZooCam_Client(void);

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
int ZooCam_Query_Client_Version(void);
int ZooCam_Query_Server_Version(void);

/* ===========================================================================
--	Routine to return information on the camera
--
--	Usage:  int ZooCam_Get_Camera_Info(CAMERA_INFO *info);
--
--	Inputs: info - pointer to variable to receive information
--		
--	Output: if (info != NULL) *info filled with information
--
-- Return: 0 if successful; 1 if camera is not initialized
--
-- Note: Call with info=NULL will just return 0 if camera  active, otherwise 1
=========================================================================== */
int ZooCam_Get_Camera_Info(CAMERA_INFO *info);

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
--         1 ==> no camera connected
=========================================================================== */
int ZooCam_Get_Image_Info(int frame, IMAGE_INFO *info);

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
int ZooCam_Get_Image_Data(int frame, void **image_data, size_t *length);

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
int ZooCam_Save_Frame(int frame, char *path, FILE_FORMAT format);

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
int ZooCam_Save_All(char *pattern, FILE_FORMAT format);

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
int ZooCam_Get_Exposure(EXPOSURE_PARMS *exposure);
int ZooCam_Get_Gains(EXPOSURE_PARMS *exposure);

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
int ZooCam_Set_Exposure(double ms_exposure, double fps, EXPOSURE_PARMS *rvalues);

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
int ZooCam_Set_Gains(double gamma, double master, double red, double green, double blue, EXPOSURE_PARMS *rvalues);

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
int ZooCam_Get_Ring_Info(RING_INFO *rings);
int ZooCam_Get_Ring_Size(void);
int ZooCam_Get_Ring_Frame_Cnt(void);

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
int ZooCam_Set_Ring_Size(int nbuf);

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
int ZooCam_Reset_Ring_Count(void);


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
int ZooCam_Arm(TRIG_ARM_ACTION action);


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
TRIGGER_MODE ZooCam_Get_Trigger_Mode(TRIGGER_INFO *info);


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
TRIGGER_MODE ZooCam_Set_Trigger_Mode(TRIGGER_MODE mode, TRIGGER_INFO *info);


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
int ZooCam_Trigger(void);


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
--         (1) DCX_ACQUIRE_IMAGE   [captures the image]
--         (2) DCX_GET_IMAGE_INFO  [transmits information about image]
--         (3) DCX_GET_IMAGE_DATA  [transmits actual image bytes]
=========================================================================== */
int DCxZooCam_Acquire_Image(IMAGE_INFO *info, char **image);

/* ===========================================================================
--	Routine to set the camera acquisition time
--
--	Usage:  int DCxZooCam_Set_Exposure(double exposure, BOOL maximize_framerate);
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
int DCxZooCam_Acquire_Image(IMAGE_INFO *info, char **image);

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
int DCxZooCam_Burst_Arm(void);
int DCxZooCam_Burst_Abort(void);
int DCxZooCam_Burst_Status(void);
int DCxZooCam_Burst_Wait(int msTimeout);

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
int ZooCam_LED_Set_State(int state);

#endif		/* _ZOOCAM_CLIENT_INCLUDED */
