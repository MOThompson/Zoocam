#ifndef ZOOCAM_LOADED

#define ZOOCAM_LOADED

/* Camera/window information structure */
typedef struct _WND_INFO WND_INFO;

/* Global identifier of my window handle */
HWND ZooCam_main_hdlg;
WND_INFO *main_wnd;

/* Special calls to the display thread */
#define	WMP_SHOW_FRAMERATE			(WM_APP+3)
#define	WMP_SHOW_EXPOSURE				(WM_APP+4)
#define	WMP_SHOW_GAMMA					(WM_APP+6)
#define	WMP_SHOW_COLOR_CORRECT		(WM_APP+7)
#define	WMP_SHOW_GAINS					(WM_APP+8)
#define	WMP_UPDATE_IMAGE_W_CURSOR	(WM_APP+9)
#define	WMP_SHOW_CURSOR_POSN			(WM_APP+9)
#define	WMP_BURST_ARM					(WM_APP+10)
#define	WMP_BURST_COMPLETE			(WM_APP+11)
#define	WMP_UPDATE_TRIGGER_BUTTONS	(WM_APP+12)

/* Support routines that are now safe and simple */
int GenerateCrosshair(WND_INFO *wnd, HWND hwnd);

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
-- Usage: int DCx_Set_Exposure(WND_INFO *wnd, double exposure, BOOL maximize_framerate, HWND hdlg);
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
int DCx_Set_Exposure(WND_INFO *wnd, double exposure, BOOL maximize_framerate, HWND hdlg);


/* ===========================================================================
-- Interface to the BURST functions
--
-- Usage: int Burst_Actions(BURST_ACTION request, int msTimeout, int *response);
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
typedef enum _BURST_ACTION {BURST_STATUS=0, BURST_ARM=1, BURST_ABORT=2, BURST_WAIT=3} BURST_ACTION;

int Burst_Actions(BURST_ACTION request, int msTimeout, int *response);

/* ===========================================================================
-- Interface to the RING functions
--
-- Usage: int Ring_Actions(RING_ACTION request, int option, RING_INFO *response);
--
-- Inputs: request - what to do
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
typedef enum _RING_ACTION {RING_GET_INFO=0, RING_GET_SIZE=1, RING_SET_SIZE=2, RING_GET_ACTIVE_CNT=3} RING_ACTION;

int Ring_Actions(RING_ACTION request, int option, struct _RING_INFO *response);

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


/* ===========================================================================
-- Routine to enable or disable Live Video (from client/server)
--
-- Usage: int DCx_Enable_Live_Video(int state);
--
-- Inputs: state - 0 => disable, 1 => enable, other => just return current
--
-- Output: Optionally enables or disables live video
--
-- Return: 0 or 1 for current state (or -1 on no camera error)
=========================================================================== */
int DCx_Enable_Live_Video(int state);

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */


#ifdef INCLUDE_WND_DETAIL_INFO

/* Upper level camera information encoded in the combobox dropdown and ActiveCamera pointer */
typedef struct _CAMERA_INFO {
	enum {UNKNOWN=0, DCX=1, TL=2} driver;		/* empty initialization points to none */
	char id[64];										/* Up to 32 char id for combobox */
	char description[256];							/* Optional description for help	*/
	void *details;										/* Camera driver specific detail */
} CAMERA_INFO;

typedef struct _WND_INFO {
	HWND main_hdlg;							/* Handle to primary dialog box */

	BOOL LiveVideo;							/* Are we in free-run mode? */
	BOOL PauseImageRendering;				/* Critical sections where buffers maybe changing ... disable access */
	BOOL BurstModeActive;					/* Are we in an active mode with burst (wait, collecting, etc.) */
													/* Set to FALSE after arm'd to abort */
	enum {BURST_STATUS_INIT=0,				/* Initial value on program start ... no request ever received */
			BURST_STATUS_ARM_REQUEST=1,	/* An arm request received ... but thread not yet running */
			BURST_STATUS_ARMED=2,			/* Armed and awaiting a stripe start message */
			BURST_STATUS_RUNNING=2,			/* In stripe run */
			BURST_STATUS_COMPLETE=3,		/* Stripe completed with images saved */
			BURST_STATUS_ABORT=4,			/* Capture was aborted */
			BURST_STATUS_FAIL=5				/* Capture failed for other reason (no semaphores, etc.) */
	} BurstModeStatus;						/* Internal status */

	/* Camera initialized */
	CAMERA_INFO Camera;						/* Pointer to primary info on the camera (DCX or TL) */

	/* Common camera information */
	int Image_Count;							/* Number of images processed - use to identify new data */
	int height, width;						/* Image height and width in pixels */
	BOOL bColor;								/* Sensor is color */

	/* Cross-hair focus of windows */	
	struct {		
		double x,y;								/* Fraction of [0,1) the way across image for the cursor (in screen coordinates) */
		BOOL fullwidth;						/* Cursor should extend over full range of image */
	} cursor_posn;

	/* Should we generate error reports */
	BOOL EnableErrorReports;				/* Do we want error reports as message boxes? */

	/* Associated with opening a DCX camera */
	DCX_CAMERA *dcx;

	HWND thumbnail;

	BOOL track_centroid;								/* Continuously update cursor at centroid */
	int red_saturate, green_saturate, blue_saturate;
	GRAPH_CURVE *red_hist, *green_hist, *blue_hist;
	GRAPH_CURVE *vert_w, *vert_r, *vert_g, *vert_b, *vert_sum;
	GRAPH_CURVE *horz_w, *horz_r, *horz_g, *horz_b, *horz_sum;

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

} WND_INFO;

#endif			/* INCLUDE_DCX_DETAIL_INFO */

int AllocRingBuffers(WND_INFO *wnd, int nRequest);

#endif			/* ZOOCAM_LOADED */

