/* Global identifier of my window handle */
HWND DCx_main_hdlg;

#define USE_RINGS	(10)				/* Default ring buffer size */

typedef enum _DCX_IMAGE_FORMAT { IMAGE_BMP, IMAGE_JPG, IMAGE_PNG } DCX_IMAGE_FORMAT;
typedef enum _DCX_IMAGE_TYPE   { IMAGE_COLOR, IMAGE_MONOCHROME } DCX_IMAGE_TYPE;

/* Camera/window information structure */
typedef struct _DCX_WND_INFO DCX_WND_INFO;

/* Structure associated with an image information message in client/server */
#pragma pack(4)
typedef struct _DCX_IMAGE_INFO {
	uint32_t width, height;			/* Image width and height */
	uint32_t memory_pitch;			/* Bytes between each row (allocate memory pitch*height) */
	double exposure;					/* Current exposure (ms) */
	double gamma;						/* Gamma value */
	uint32_t master_gain;			/* Gains in non-linear range [0,100] */
	uint32_t red_gain, green_gain, blue_gain;
	uint32_t color_correction;		/* 0,1,2,4,8 corresponding to disable, enable, BG40, HQ, IR Auto */
	double color_correction_factor;
	uint32_t red_saturate, green_saturate, blue_saturate;		/* Number saturated pixels */
} DCX_IMAGE_INFO;
#pragma pack()

/* Structure associated with an camera status message in client/server */
#pragma pack(4)
typedef struct _DCX_STATUS {
	char manufacturer[32];			/* Camera manufacturer */
	char model[32];					/* Camera model */
	char serial[32];					/* Camera serial number */
	char version[32];					/* Camera version */
	char date[32];						/* Firmware date */
	uint32_t CameraID;				/* Camera ID (as set in EEPROM) */
	DCX_IMAGE_TYPE color_mode;		/* Monochrome or color mode */
	uint32_t pixel_pitch;			/* Pixel pitch in um */
	double fps;							/* Frame rate (frames per second) */
	double exposure;					/* Current exposure (ms) */
	double gamma;						/* Gamma value */
	uint32_t master_gain;			/* Gains in non-linear range [0,100] */
	uint32_t red_gain, green_gain, blue_gain;
	uint32_t color_correction;		/* 0,1,2,4,8 corresponding to disable, enable, BG40, HQ, IR Auto */
	double color_correction_factor;
} DCX_STATUS;
#pragma pack()

/* ===========================================================================
-- Start a thread to run the dialog box for the camera
--
-- Usage: void DCx_Start_Dialog(void *arglist);
--
-- Inputs: arglist - unused
--
-- Output: simply launches the dialog box
--
-- Return: none
=========================================================================== */
void DCx_Start_Dialog(void *arglist);

/* ===========================================================================
-- Interface routine to accept a request to save an image as a file
--
-- Usage: int DCX_Capture_Image(char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND HwndRenderBitmap);
--
-- Inputs: fname   - if not NULL, pointer to name of file to be saved
--                   if NULL, brings up a Save As ... dialog box
--         format  - one of IMAGE_BMP, IMAGE_JPG, IMAGE_PNG
--         quality - quality of image (primary JPEG)
--         info    - pointer (if not NULL) to structure to be filled with image info
--			  HwndRenderBitmap - if not NULL, handle were we should try to render the bitmap
--
-- Output: Saves the file as specified.
--         if info != NULL, *info filled with details of capture and basic image stats
--
-- Return: 0 - all okay
--         1 - camera is not initialized and active
--         2 - file save failed for some other reason
=========================================================================== */
int DCx_Capture_Image(char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND HwndRenderBitmap);

/* ===========================================================================
-- Interface routine to accept a request to grab and store an image in memory 
--
-- Usage: int DCX_Acquire_Image(DCX_IMAGE_INFO *info, char **buffer);
--
-- Inputs: info    - pointer (if not NULL) to structure to be filled with image info
--         buffer  - pointer set to location of image in memory;
--                   calling routine responsible for freeing this memory after use
--
-- Output: Captures an image and copies the buffer to memory location
--         if info != NULL, *info filled with details of capture and basic image stats
--
-- Return: 0 - all okay
--         1 - camera is not initialized and active
--         2 - file save failed for some other reason
=========================================================================== */
int DCx_Acquire_Image(DCX_IMAGE_INFO *info, char **buffer);

/* ===========================================================================
-- Query of DCX camera status
--
-- Usage: int DCX_Status(DCX_STATUS *status);
--
-- Inputs: conditions - if not NULL, filled with details of the current
--                      imaging conditions
--
-- Output: Fills in *conditions if requested
--
-- Return: 0 - camera is ready for imaging
--         1 - camera is not initialized and/or not active
=========================================================================== */
int DCx_Status(DCX_STATUS *status);

/* ===========================================================================
-- Write DCx camera properties to a logfile
--
-- Usage: int DCx_WriteParmaeters(char *pre_text, FILE *funit);
--
-- Inputs: pre_text - if not NULL, text written each line before the information (possibly "/* ")
--         funit    - open file handle
--
-- Output: Outputs text based information on the camera to the log file
--
-- Return: 0 - successful (camera valid and funit valid)
--         1 - camera not initialized or funit invalid
=========================================================================== */
int DCx_WriteParameters(char *pre_text, FILE *funit);


/* ===========================================================================
-- Routine to set the exposure on the camera (if enabled)
--
-- Usage: int DCx_Set_Exposure(DCX_WND_INFO *dcx, double exposure, BOOL maximize_framerate, HWND hdlg);
--
-- Inputs: dcx - pointer to info about the camera or NULL to use default
--         exposure - desired exposure in ms
--         maximize_framerate - if TRUE, maximize framerate for given exposure
--         hdlg - if a window, will receive WMP_SHOW_FRAMERATE and WMP_SHOW_EXPOSURE messages
--
-- Output: Sets the camera exposure to desired value, and optionally maximizes 
--         the framerate
--
-- Return: 0 if successful
--
-- Notes: If dcx is unknown (and hdlg), can set dcx to NULL to use static
--        value.  This is used by the client/server code to simplify life.
=========================================================================== */
int DCx_Set_Exposure(DCX_WND_INFO *dcx, double exposure, BOOL maximize_framerate, HWND hdlg);

/* ===========================================================================
-- Routine to set the gains on the camera (if enabled)
--
-- Usage: int DCx_Set_Gains(DCX_WND_INFO *dcx, int master, int red, int green, int blue, HWND hdlg);
--
-- Inputs: dcx - pointer to info about the camera or NULL to use default
--         master - value in range [0,100] for hardware gain of the channel
--         red   - value in range [0,100] for hardware gain of the red channel
--         green - value in range [0,100] for hardware gain of the green channel
--         blue  - value in range [0,100] for hardware gain of the blue channel
--         hdlg - if a window, will receive WMP_SHOW_GAINS message
--
-- Output: Sets the hardware gain values to desired value
--
-- Return: 0 if successful
--
-- Notes: If dcx is unknown (and hdlg), can set dcx to NULL to use static
--        value.  This is used by the client/server code to simplify life.
=========================================================================== */
int DCx_Set_Gains(DCX_WND_INFO *dcx, int master, int red, int green, int blue, HWND hdlg);


/* ===========================================================================
-- Interface to the BURST functions
--
-- Usage: int DCx_Burst_Actions(DCX_BURST_ACTION request, int msTimeout, int *response);
--
-- Inputs: request - what to do
--           (0) BURST_STATUS ==> return status
--           (1) BURST_ARM    ==> arm the burst mode
--           (2) BURST_ABORT  ==> abort burst if enabled
--           (3) BURST_WAIT   ==> wait for burst to complete (timeout active)
--         msTimeout - timeout for some operations (wait)
--         response - pointer to for return code (beyond success)
--
-- Output: *response - action dependent return codes if ! NULL
--         Sets internal parameters for the burst mode capture
--
-- Return: 0 if successful, 1 if burst mode is unavailable, 2 other errors
--
-- *response codes
--     ACTION = 0 (STATUS)
=========================================================================== */
typedef enum _DCX_BURST_ACTION {BURST_STATUS=0, BURST_ARM=1, BURST_ABORT=2, BURST_WAIT=3} DCX_BURST_ACTION;

int DCx_Burst_Actions(DCX_BURST_ACTION request, int msTimeout, int *response);

/* ===========================================================================
-- Interface to the RING functions
--
-- Usage: int DCX_Ring_Actions(DCX_RING_ACTION request, int option, int *response);
--
-- Inputs: request - what to do
--           (0) RING_GET_SIZE        ==> return number of buffers in the ring
--           (1) RING_SET_SIZE        ==> set number of buffers in the ring
--           (2) RING_GET_ACTIVE_CNT  ==> returns number of buffers currently with data
--         option - various use
--         response - pointer to for return code (beyond success)
--
-- Output: *response - action dependent return codes if ! NULL
--         Sets internal parameters as necessary
--         Sends message to mainhdlg (if !NULL) to modify controls
--
-- Return: 0 if successful, 1 if rings are unavailable, 2 other errors
--
-- *response codes
--     RING_GET_SIZE:			configured number of rings
--		 RING_SET_SIZE:			new configured number of rings
--		 RING_GET_ACTIVE_CNT:	number of buffers with image data
=========================================================================== */
typedef enum _DCX_RING_ACTION {RING_GET_INFO=0, RING_GET_SIZE=1, RING_SET_SIZE=2, RING_GET_ACTIVE_CNT=3} DCX_RING_ACTION;

int DCx_Ring_Actions(DCX_RING_ACTION request, int option, int *response);

/* ===========================================================================
-- Interface to the BURST functions
--
-- Usage: int DCx_Query_Frame_Data(int frame, double *tstamp, int *width, int *height, int *pitch, char **pMem);
--
-- Inputs: frame         - which frame to transfer
--         tstamp        - variable pointer for timestamp of image
--         width, height - variable pointers for frame size
--         pitch         - variable pointer for pitch of row data
--         pMem          - variable pointer for array data
--
-- Output: *tstamp         - time image transferred (resolution of 1 ms)
--         *width, *height - if !NULL, filled with image size
--         *pitch          - if !NULL, bytes between start of rows in image
--         *pMem           - if !NULL, pointer to first element of data
--
-- Return: 0 ==> successful
--         1 ==> no camera initialized
--         2 ==> requested frame invalid
=========================================================================== */
int DCx_Query_Frame_Data(int frame, double *tstamp, int *width, int *height, int *pitch, char **pMem);


/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */


#ifdef INCLUDE_DCX_DETAIL_INFO

typedef struct _DCX_WND_INFO {
	HWND main_hdlg;							/* Handle to primary dialog box */

	HANDLE FrameEvent;						/* Event for new frame valid in memory buffer */
	BOOL ProcessNewImageThreadActive;

#ifdef USE_RINGS
	HANDLE SequenceEvent;					/* Event for completion of a sequence (last buffer element) */
	BOOL SequenceThreadActive;
#endif

	BOOL LiveVideo;							/* Are we in free-run mode? */
	BOOL BurstModeArmed;						/* Have we armed camera for a burst capture */
													/* Set to FALSE after arm'd to abort */
	enum {BURST_STATUS_INIT=0,				/* Initial value on program start ... no request ever received */
			BURST_STATUS_ARM_REQUEST=1,	/* An arm request received ... but thread not yet running */
			BURST_STATUS_ARMED=2,			/* Armed and awaiting a stripe start message */
			BURST_STATUS_RUNNING=2,			/* In stripe run */
			BURST_STATUS_COMPLETE=3,		/* Stripe completed with images saved */
			BURST_STATUS_ABORT=4,			/* Capture was aborted */
			BURST_STATUS_FAIL=5				/* Capture failed for other reason (no semaphores, etc.) */
	} BurstModeStatus;						/* Internal status */

	/* Associated with opening a camera */
	HCAM hCam;
	int CameraID;
	CAMINFO CameraInfo;						/* Details on the camera */
	SENSORINFO SensorInfo;					/* Details on the sensor */
	BOOL SensorIsColor;						/* Is the camera a color camera */
	int NumImageFormats;						/* Number of image formats available */
	IMAGE_FORMAT_LIST *ImageFormatList;	/* List of formats for the active camera */
	int ImageFormatID;						/* Currently selected Image Format */
	BOOL EnableErrorReports;				/* Do we want error reports as message boxes? */

	/* Associated with the selected resolution */
	int Image_Count;							/* Number of images processed - use to identify new data */
	IMAGE_FORMAT_INFO *ImageFormatInfo;
	int height, width;

#ifdef USE_RINGS
	int nRing,									/* Number of buffers in the ring */
		 nLast,									/* Last buffer index used (from events) */
		 nShow,									/* Currently display frame */
		 nValid;									/* Highest buffer index used since reset */
	int   *Image_PID;							/* Pointers to PIDs of each image in the ring */
	char  **Image_Mem;						/* Pointers to the image memory */
#else
	int Image_PID;
	char *Image_Mem;
#endif
	BOOL Image_Mem_Allocated;				/* Have we allocated (vis IS_AllocImageMem) the bufers */

	double Image_Aspect;

	HWND thumbnail;
	double x_image_target, y_image_target;		/* Fraction of [0,1) the way across image for the cursor (in screen coordinates) */
	BOOL full_width_cursor;							/* Draw cursor full width of the image */

	BOOL track_centroid;								/* Continuously update cursor at centroid */
	int red_saturate, green_saturate, blue_saturate;
	GRAPH_CURVE *red_hist, *green_hist, *blue_hist;
	GRAPH_CURVE *vert_w, *vert_r, *vert_g, *vert_b;
	GRAPH_CURVE *horz_w, *horz_r, *horz_g, *horz_b;

#ifdef USE_NUMATO
	/* Elements for Numator DIO */
	struct {
		BOOL initialized;
		int port;
		BOOL enabled;
		NUMATO *dio;
		BOOL bit_is_output;
		enum {DIO_OFF=0, DIO_ON=1, DIO_TOGGLE=2} mode;
		int on, off, total, phase;
	} numato;
#endif

} DCX_WND_INFO;

#endif			/* INCLUDE_DCX_DETAIL_INFO */
