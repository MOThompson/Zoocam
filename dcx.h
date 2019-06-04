/* Global identifier of my window handle */
HWND DCx_main_hdlg;

typedef enum _DCX_IMAGE_FORMAT { IMAGE_BMP, IMAGE_JPG, IMAGE_PNG } DCX_IMAGE_FORMAT;
typedef struct _DCX_STATUS {
	char manufacturer[32];	/* Camera manufacturer */
	char model[32];			/* Camera model */
	char serial[32];			/* Camera serial number */
	char version[32];			/* Camera version */
	char date[32];				/* Firmware date */
	int CameraID;				/* Camera ID (as set in EEPROM */
	char color_mode[16];		/* Bayer, Monochrome or unknown */
	int pixel_pitch;			/* Pixel pitch in um */
	double fps;					/* Frame rate (frames per second) */
	double exposure;			/* Current exposure (ms) */
	double gamma;				/* Gamma value */
	int master_gain;			/* Gains in non-linear range [0,100] */
	int red_gain, green_gain, blue_gain;
	int color_correction;	/* 0,1,2,4,8 corresponding to disable, enable, BG40, HQ, IR Auto */
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
-- Usage: int DCx_Capture_Image(char *fname, DCX_IMAGE_FORMAT format, int quality);
--
-- Inputs: fname   - if not NULL, pointer to name of file to be saved
--                   if NULL, brings up a Save As ... dialog box
--         format  - one of IMAGE_BMP, IMAGE_JPG, IMAGE_PNG
--         quality - quality of image (primary JPEG)
--
-- Output: Saves the file as specified.
--
-- Return: 0 - all okay
--         1 - camera is not initialized and active
--         2 - file save failed for some other reason
=========================================================================== */
int DCx_Capture_Image(char *fname, DCX_IMAGE_FORMAT format, int quality);

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
--         1 - camera is not initialized and active
=========================================================================== */
int DCx_Status(DCX_STATUS *status);
