#ifndef dcx_loaded

#define dcx_loaded

#define	_PURE_C
	#include "uc480.h"
#undef	_PURE_C

#define USE_RINGS						/* Use ring buffers */
#define DFLT_RING_SIZE	(10)		

#define	DCX_MIN_FPS				(0.1)		/* Nominal minimum frame rate */
#define	DCX_MAX_FPS				(25.0)	/* Nominal maximum frame rate */

#define	DCX_MAX_RING_SIZE		(999)		/* Maximum number ring buffers */
#define	DCX_DFLT_RING_SIZE	(10)		/* Default number of frames in ring */

typedef enum _DCX_IMAGE_FORMAT { IMAGE_BMP=0, IMAGE_JPG=1, IMAGE_PNG=2 } DCX_IMAGE_FORMAT;
typedef enum _DCX_IMAGE_TYPE   { IMAGE_MONOCHROME=0, IMAGE_COLOR=1 } DCX_IMAGE_TYPE;

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

/* Structure used by DCX_CLIENT to allow changes to exposure */
#pragma pack(4)
/* Or'd bit-flags in option to control setting parameters */
/* Exposure always has priority over FPS, but FPS will be maximized if not modified with exposure */
#define	DCXF_MODIFY_EXPOSURE		(0x01)	/* Modify exposure (value in ms) */
#define	DCXF_MODIFY_FPS			(0x02)	/* Modify frames per second */
#define	DCXF_MODIFY_GAMMA			(0x04)	/* Modify gamma */
#define	DCXF_MODIFY_MASTER_GAIN	(0x08)	/* Modify master gain */
#define	DCXF_MODIFY_RED_GAIN		(0x10)	/* Red channel gain */
#define	DCXF_MODIFY_GREEN_GAIN	(0x20)	/* Green channel gain */
#define	DCXF_MODIFY_BLUE_GAIN	(0x40)	/* Blue channel gain */
typedef struct _DCX_EXPOSURE_PARMS {
	double exposure;									/* Exposure time in ms				*/
	double fps;											/* Frame rate (per second)			*/
	uint32_t gamma;									/* Gamma value (0 < gamma < 100)	*/
	uint32_t master_gain;							/* Master gain (0 < gain < 100)	*/
	uint32_t red_gain, green_gain, blue_gain;	/* Individual channel gains		*/
} DCX_EXPOSURE_PARMS;
#pragma pack()

/* DCX type camera information */
typedef struct _DCX_CAMERA {
	HCAM hCam;
	int CameraID;
	CAMINFO CameraInfo;						/* Details on the camera */
	SENSORINFO SensorInfo;					/* Details on the sensor */
	int width, height;						/* Size of the sensor in pixels */
	BOOL IsSensorColor;						/* Is the camera a color camera */
	int NumImageFormats;						/* Number of image formats available */
	IMAGE_FORMAT_LIST *ImageFormatList;	/* List of formats for the active camera */
	int ImageFormatID;						/* Currently selected Image Format */

	/* Associated with the selected resolution */
	IMAGE_FORMAT_INFO *ImageFormatInfo;

#ifdef USE_RINGS
	int  nSize;									/* Number of buffers in the ring */
	int  nValid;								/* Number of frames valid since last reset */
	int  iLast;									/* index of last buffer used (from events) */
	int  iShow;									/* index of currently displayed frame */
	int  *Image_PID;							/* Pointers to PIDs of each image in the ring */
	char **Image_Mem;							/* Pointers to the image memory */
#else
	int Image_PID;
	char *Image_Mem;
#endif
	BOOL Image_Mem_Allocated;				/* Have we allocated (vis IS_AllocImageMem) the bufers */

	TRIG_MODE Trig_Mode;						/* Triggering mode (DCX_TRIG_FREERUN, SOFTWARE, EXTERNAL */

} DCX_CAMERA;

/* Generic routines that can be called */
int    DCx_Initialize(void);
int	 DCx_SetDebug(BOOL debug);
int    DCx_Shutdown(void);
int    DCx_Status(DCX_CAMERA *dcx, DCX_STATUS *status);

int    DCx_Enum_Camera_List(int *pcount, UC480_CAMERA_INFO **pinfo);
int    DCx_Select_Camera(DCX_CAMERA *dcx, int CameraID, int *nBestFormat);
int    DCx_Initialize_Resolution(DCX_CAMERA *dcx, int ImageFormatID);

int    DCx_CloseCamera(DCX_CAMERA *dcx);

int    DCx_RenderImage(DCX_CAMERA *dcx, int frame, HWND hwnd);

int    DCx_GetExposureParms(DCX_CAMERA *dcx, double *ms_min, double *ms_max, double *ms_inc);
double DCx_SetExposure(DCX_CAMERA *dcx, double ms_expose);
double DCx_GetExposure(DCX_CAMERA *dcx, BOOL bForceQuery);

double DCx_SetFPSControl(DCX_CAMERA *dcx, double fps);
double DCx_GetFPSControl(DCX_CAMERA *dcx);
double DCx_GetFPSActual(DCX_CAMERA *dcx);

#define	DCX_IGNORE_GAIN		(-999)
int DCx_SetRGBGains    (DCX_CAMERA *dcx, int  master, int  red, int  green, int  blue);
int DCx_GetRGBGains    (DCX_CAMERA *dcx, int *master, int *red, int *green, int *blue);
int DCx_GetDfltRGBGains(DCX_CAMERA *dcx, int *master, int *red, int *green, int *blue);

double DCx_SetGamma(DCX_CAMERA *dcx, double gamma);
double DCx_GetGamma(DCX_CAMERA *dcx);

COLOR_CORRECT DCx_SetColorCorrection(DCX_CAMERA *dcx, COLOR_CORRECT mode, double rval);
COLOR_CORRECT DCx_GetColorCorrection(DCX_CAMERA *dcx, double *rval);

int DCx_LoadParameterFile(DCX_CAMERA *dcx, char *path);
int DCx_SaveParameterFile(DCX_CAMERA *dcx, char *path);

/* Triggering controls (freerun especially) */
int DCx_Trigger(DCX_CAMERA *dcx, int msWait);
TRIG_MODE DCx_SetTrigMode(DCX_CAMERA *dcx, TRIG_MODE mode, int msWait);
TRIG_MODE DCx_GetTrigMode(DCX_CAMERA *dcx);

/* Ring buffer control */
int DCx_GetRingInfo(DCX_CAMERA *dcx, int *nSize, int *nValid, int *iLast, int *iShow);
int DCx_SetRingBufferSize(DCX_CAMERA *dcx, int nBuf);
int DCx_ReleaseRingBuffers(DCX_CAMERA *dcx);

/* Linked to server */
int DCx_Set_Exposure_Parms(int options, DCX_EXPOSURE_PARMS *request, DCX_EXPOSURE_PARMS *actual);
int DCx_Acquire_Image(DCX_IMAGE_INFO *info, char **buffer);

int FindImageIndexFromPID(DCX_CAMERA *dcx, int PID);
int FindImageIndexFrompMem(DCX_CAMERA *dcx, char *pMem);
unsigned char *FindImagepMemFromPID(DCX_CAMERA *dcx, int PID, int *index);
int FindImagePIDFrompMem(DCX_CAMERA *dcx, unsigned char *pMem, int *index);

int DCx_SaveImage(DCX_CAMERA *dcx, char *fname, int format);
int DCx_CaptureImage(DCX_CAMERA *dcx, char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap);

#endif			/* dcx_loaded */
