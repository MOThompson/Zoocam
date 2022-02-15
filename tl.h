#ifndef tl_loaded

#define tl_loaded

// #include "tl_camera_sdk.h"					/* Somehow, this is already loaded */
#include "tl_camera_sdk_load.h"
#include "tl_mono_to_color_processing.h"
#include "tl_color_enum.h"
#include "tl_mono_to_color_processing_load.h"

/* Maximum number of threads that can request a signal when new frame available */
#define	TL_MAX_SIGNALS				(10)
#define	TL_MAX_RING_SIZE			(1000)
#define	TL_IMAGE_ACCESS_TIMEOUT	(200)		/* Never delay for more than 200 ms (ensure 5 fps) for access to image buffers */

#define	TL_CAMERA_MAGIC	0x8A46
typedef struct _TL_CAMERA {
	int magic;													/* Magic value to indicate valid */
	char ID[32];												/* String ID for the camera		*/
	char name[64];
	char model[64];											/* Model									*/
	char serial[64];											/* Serial number						*/
	char firmware[1024];										/* Firmware revisions				*/
	FILE *handle;												/* Handle to the camera itself	*/
	enum TL_CAMERA_SENSOR_TYPE sensor_type;			/* Sensor type (must be BAYER)	*/
	enum TL_COLOR_FILTER_ARRAY_PHASE color_filter;	/* Color filter array phase		*/
	float color_correction[9];								/* Color correction matrix			*/
	float white_balance[9];									/* Default white balance matrix	*/

	long long us_expose_min, us_expose_max;			/* Min and max exposure times		*/
	long long us_expose;										/* Exposure time in us				*/

	BOOL bFrameRateControl;									/* Is framerate control possible	*/
	double fps_min, fps_max;								/* Min and max frame rate			*/

	BOOL bGainControl;										/* Is gain control possible		*/
	double db_gain_min, db_gain_max;						/* Min/max gain in dB */

	int clock_Hz;												/* Camera clock frequency (or 0)	*/

	int bit_depth;												/* Bit depth							*/
	int width, height;										/* Image size							*/
	double pixel_height_um, pixel_width_um;			/* Pixel size in um					*/
	int pixel_bytes;											/* Number of bytes per pixel		*/
	int image_bytes;											/* Number of bytes in an image	*/
	void *color_processor;
	BOOL IsSensorColor;										/* TRUE color, FALSE monochrome	*/

	HANDLE image_mutex;										/* Access to modify/use data		*/

	int frame_count;											/* Total number of frames 			*/

	/* Ring information */
	int nBuffers,												/* Number of frames in the ring	*/
		 nValid,													/* # frames since last reset		*/
	    iLast,													/* index of last buffer used		*/
		 iShow;													/* index of frame in rgb/seps		*/
	int npixels;												/* Number of pixels in a frame	*/
	int nbytes_raw;											/* Number of bytes in each frame */
	unsigned short **raw;									/* nsize array of raw sensor data buffers */
	double *timestamp;										/* Array of timestamps on images	*/
	BOOL valid_raw;											/* Raw data is again valid since any reset */

	double rgb24_timestamp;									/* Timestamp of rgb24 data			*/
	int nbytes_rgb24;											/* Number of bytes in rgb24 data	*/
	unsigned char *rgb24;									/* rgb32 image RGBQUAD (4xh*2)	*/

	double separations_timestamp;							/* Timestamp of separation data	*/
	int nbytes_red, nbytes_green, nbytes_blue;		/* Number of bytes in sub-chans	*/
	unsigned short *red, *green, *blue;					/* Inidividual channels */

	HANDLE new_image_signals[TL_MAX_SIGNALS];			/* Handles to event semaphores	*/

} TL_CAMERA;


/* Global status variables */
BOOL TL_is_initialized;

/* Maximum number of cameras that can be simultaneously opened */
/* While this is public, access to list is not intended to be direct */
/* NULL entries in this list indicate no camera in that slot */
#define	TL_MAX_CAMERAS	(20)
TL_CAMERA *tl_camera_list[TL_MAX_CAMERAS];
int tl_camera_count;

/* Functions */
int TL_Initialize(void);
int TL_Shutdown(void);

/* Find  initializes structures with minimal resources  */
/* Open  opens the handle and initializes buffers */
/* Close releases buffers associated witht he camera */
/* Free  deletes the camera from the list */
TL_CAMERA *TL_FindCamera(char *ID, int *rc);
int TL_ForgetCamera(TL_CAMERA *camera);
int TL_OpenCamera(TL_CAMERA *camera, int nBuf);
int TL_CloseCamera(TL_CAMERA *camera);
int TL_SetBufferSize(TL_CAMERA *camera, int nBuf);

int TL_FindAllCameras(TL_CAMERA **list[]);
int TL_CloseAllCameras(void);
int TL_ForgetAllCameras(void);

TL_CAMERA *TL_FindCameraByIndex(int index);
TL_CAMERA *TL_FindCameraByID(char *ID);
TL_CAMERA *TL_FindCameraByHandle(void *handle);

BOOL TL_IsValidCamera(TL_CAMERA *camera);

int TL_AddImageSignal(TL_CAMERA *camera, HANDLE signal);
int TL_RemoveImageSignal(TL_CAMERA *camera, HANDLE signal);

int TL_ProcessRGB(TL_CAMERA *camera, int frame);				/* No ties to TL_ProcessRawSeparation */
int TL_ProcessRawSeparation(TL_CAMERA *camera, int frame);	/* No ties to TL_ProcessRGB */

BITMAPINFOHEADER *TL_CreateDIB(TL_CAMERA *camera, int frame, int *rc);
int TL_SaveBMPFile(TL_CAMERA *camera, char *path, int frame);

int TL_RenderImage(TL_CAMERA *camera, int frame, HWND hwnd);

double TL_SetExposure(TL_CAMERA *camera, double ms_expose);
double TL_GetExposure(TL_CAMERA *camera, BOOL bForceQuery);

int TL_SetFPSControl(TL_CAMERA *camera, double fps);
double TL_GetFPSControl(TL_CAMERA *camera);
double TL_GetFPSActual(TL_CAMERA *camera);

int TL_SetMasterGain(TL_CAMERA *camera, double dB_gain);
int TL_GetMasterGain(TL_CAMERA *camera, double *dB_gain);

#define	TL_IGNORE_GAIN		(-999)
int TL_GetRGBGains(TL_CAMERA *camera, double *red, double *green, double *blue);
int TL_SetRGBGains(TL_CAMERA *camera, double  red, double  green, double  blue);

#endif			/* tl_loaded */
