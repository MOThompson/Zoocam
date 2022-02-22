#ifndef CAMERA_LOADED

#define	CAMERA_LOADED

/* Values for the enumeration must match order of radio buttons */
#define	NUM_TRIGGER_MODES		(4)
typedef enum _TRIGGER_MODE     { TRIG_FREERUN=0, TRIG_SOFTWARE=1, TRIG_EXTERNAL=2, TRIG_BURST=3 } TRIGGER_MODE;
typedef enum _TRIGGER_POLARIY  { TRIG_EXT_NOCHANGE=0, TRIG_EXT_POS=1, TRIG_EXT_NEG=2, TRIG_EXT_UNSUPPORTED=3} TRIGGER_POLARITY ;

/* Structure for details about trigging (mostly external trigger parms) */
#pragma pack(4)
typedef struct _TRIGGER_INFO {
	TRIGGER_MODE mode;						/* Current triggering mode (query only) */
	TRIGGER_POLARITY ext_slope;			/* External trigger polarity  */
	BOOL bArmed;								/* If true, trigger is armed (query only) */
	int frames_per_trigger;					/* Frames per trigger (in SOFTWARE / HARDWARE modes) */
	int msWait;									/* ms to wait for previous trig to complete before switch	*/
	int nBurst;									/* number of images to capture on software/external trigger */
} TRIGGER_INFO;
#pragma pack()

#define	NUM_COLOR_CORRECT_MODES	(5)
typedef enum _COLOR_CORRECT { COLOR_DISABLE=0, COLOR_ENABLE=1, COLOR_BG40=2, COLOR_HQ=3, COLOR_AUTO_IR=4 } COLOR_CORRECT;

/* Flags identifying allowed save formats for a given camera */
#define	FL_BMP			(0x01)
#define	FL_RAW			(0x02)
#define	FL_JPG			(0x04)
#define	FL_PNG			(0x08)
#define	FL_BURST			(0x10)

/* Structure use for communicating ring size information in client/server */
#pragma pack(4)
typedef struct _RING_INFO {
	int nBuffers;								/* Number of buffers in the ring */
	int nValid;									/* Number of frames valid since last reset */
	int iLast;									/* index of last buffer used (from events) */
	int iShow;									/* index of currently displayed frame */
} RING_INFO;
#pragma pack()

typedef struct _WND_INFO WND_INFO;

int    Camera_GetExposureParms(HWND hdlg, WND_INFO *wnd, double *ms_low, double *ms_high, double *ms_incr);
double Camera_SetExposure(HWND hdlg, WND_INFO *wnd, double ms_expose);
double Camera_GetExposure(HWND hdlg, WND_INFO *wnd);

double Camera_SetGamma(HWND hdlg, WND_INFO *wnd, double  gamma);
double Camera_GetGamma(HWND hdlg, WND_INFO *wnd);

typedef enum {M_CHAN=0, R_CHAN=1, G_CHAN=2, B_CHAN=3} GAIN_CHANNEL;
typedef enum {IS_SLIDER, IS_VALUE} ENTRY_TYPE;
int Camera_SetGains(HWND hdlg, WND_INFO *wnd, GAIN_CHANNEL channel, ENTRY_TYPE entry, double value);
int Camera_GetGains(HWND hdlg, WND_INFO *wnd, double values[4], double slider[4]);
int Camera_ResetGains(HWND hdlg, WND_INFO *wnd);

double Camera_GetFPSActual(HWND hdlg, WND_INFO *wnd);
double Camera_SetFPSControl(HWND hdlg, WND_INFO *wnd, double  rate);
double Camera_GetFPSControl(HWND hdlg, WND_INFO *wnd);

COLOR_CORRECT Camera_GetColorCorrection(HWND hdlg, WND_INFO *wnd, double *rval);
COLOR_CORRECT Camera_SetColorCorrection(HWND hdlg, WND_INFO *wnd, COLOR_CORRECT mode, double rval);

int Camera_Arm(HWND hdlg, WND_INFO *wnd);
int Camera_Disarm(HWND hdlg, WND_INFO *wnd);
int Camera_Trigger(HWND hdlg, WND_INFO *wnd);
int Camera_GetFramesPerTrigger(HWND hdlg, WND_INFO *wnd);
int Camera_SetFramesPerTrigger(HWND hdlg, WND_INFO *wnd, int frames);
TRIGGER_MODE Camera_SetTriggerMode(HWND hdlg, WND_INFO *wnd, TRIGGER_MODE mode, TRIGGER_INFO *info);
TRIGGER_MODE Camera_GetTriggerMode(HWND hdlg, WND_INFO *wnd, TRIGGER_INFO *info);

int Camera_GetRingInfo(HWND hdlg, WND_INFO *wnd, RING_INFO *info);
int Camera_SetRingBufferSize(HWND hdlg, WND_INFO *wnd, int nBuf);

int Camera_GetPreferredImageFormat(HWND hdlg, WND_INFO *wnd);
int Camera_GetSaveFormatFlag(HWND hdlg, WND_INFO *wnd);
int Camera_RenderFrame(HWND hdlg, WND_INFO *wnd, int frame, HWND hwnd);
int Camera_SaveImage(HWND hdlg, WND_INFO *wnd, int format);
int Camera_SaveBurstImages(HWND hdlg, WND_INFO *wnd, int format);

#endif			/* CAMERA_LOADED */
