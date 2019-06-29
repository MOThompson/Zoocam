/* Global identifier of my window handle */
HWND DCx_main_hdlg;

typedef enum _DCX_IMAGE_FORMAT { IMAGE_BMP, IMAGE_JPG, IMAGE_PNG } DCX_IMAGE_FORMAT;
typedef enum _DCX_IMAGE_TYPE   { IMAGE_COLOR, IMAGE_MONOCHROME } DCX_IMAGE_TYPE;

typedef struct _DCX_IMAGE_INFO {
	int width, height;
	double exposure;			/* Current exposure (ms) */
	double gamma;				/* Gamma value */
	int master_gain;			/* Gains in non-linear range [0,100] */
	int red_gain, green_gain, blue_gain;
	int color_correction;	/* 0,1,2,4,8 corresponding to disable, enable, BG40, HQ, IR Auto */
	double color_correction_factor;
	int red_saturate, green_saturate, blue_saturate;
} DCX_IMAGE_INFO;

typedef struct _DCX_STATUS {
	char manufacturer[32];		/* Camera manufacturer */
	char model[32];				/* Camera model */
	char serial[32];				/* Camera serial number */
	char version[32];				/* Camera version */
	char date[32];					/* Firmware date */
	int CameraID;					/* Camera ID (as set in EEPROM */
	DCX_IMAGE_TYPE color_mode;	/* Monochrome or color mode */
	int pixel_pitch;				/* Pixel pitch in um */
	double fps;						/* Frame rate (frames per second) */
	double exposure;				/* Current exposure (ms) */
	double gamma;					/* Gamma value */
	int master_gain;				/* Gains in non-linear range [0,100] */
	int red_gain, green_gain, blue_gain;
	int color_correction;		/* 0,1,2,4,8 corresponding to disable, enable, BG40, HQ, IR Auto */
	double color_correction_factor;
} DCX_STATUS;

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
