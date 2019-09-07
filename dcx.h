/* Global identifier of my window handle */
HWND DCx_main_hdlg;

typedef enum _DCX_IMAGE_FORMAT { IMAGE_BMP, IMAGE_JPG, IMAGE_PNG } DCX_IMAGE_FORMAT;
typedef enum _DCX_IMAGE_TYPE   { IMAGE_COLOR, IMAGE_MONOCHROME } DCX_IMAGE_TYPE;

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


#ifdef INCLUDE_DCX_DETAIL_INFO

typedef struct _DCX_WND_INFO {
	HWND main_hdlg;							/* Handle to primary dialog box */
	HANDLE FrameEvent;
	BOOL RenderImageThreadActive;
	BOOL LiveVideo;							/* Are we in free-run mode? */

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
	int Image_PID;
	char *Image_Mem;
	int Image_Memory_Pitch;
	double Image_Aspect;

	HWND thumbnail;
	double x_image_target, y_image_target;
	BOOL full_width_cursor;

	int red_saturate, green_saturate, blue_saturate;
	GRAPH_CURVE *red_hist, *green_hist, *blue_hist;
	GRAPH_CURVE *vert_w, *vert_r, *vert_g, *vert_b;
	GRAPH_CURVE *horz_w, *horz_r, *horz_g, *horz_b;
} DCX_WND_INFO;

/* ===========================================================================
-- Routine to set the exposure on the camera (if enabled)
--
-- Usage: int DCx_Set_Exposure(DCX_WND_INFO *dcx, double exposure, BOOL maximize_framerate, HWND hdlg);
--
-- Inputs: dcx - pointer to info about the camera
--         exposure - desired exposure in ms
--         maximize_framerate - if TRUE, maximize framerate for given exposure
--         hdlg - if a window, will receive WMP_SHOW_FRAMERATE and WMP_SHOW_EXPOSURE messages
--
-- Output: Sets the camera exposure to desired value, and optionally maximizes 
--         the framerate
--
-- Return: 0 if successful
=========================================================================== */
int DCx_Set_Exposure(DCX_WND_INFO *dcx, double exposure, BOOL maximize_framerate, HWND hdlg);

#endif			/* INCLUDE_DCX_DETAIL_INFO */
