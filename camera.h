#ifndef CAMERA_LOADED

#define	CAMERA_LOADED

#define	NUM_TRIG_MODES	(3)
typedef enum _TRIG_MODE     { TRIG_SOFTWARE=0, TRIG_FREERUN=1, TRIG_EXTERNAL=2 } TRIG_MODE;

#define	NUM_COLOR_CORRECT_MODES	(5)
typedef enum _COLOR_CORRECT { COLOR_DISABLE=0, COLOR_ENABLE=1, COLOR_BG40=2, COLOR_HQ=3, COLOR_AUTO_IR=4 } COLOR_CORRECT;

/* Structure use for communicating ring size information in client/server */
#pragma pack(4)
typedef struct _RING_INFO {
	int nSize;									/* Number of buffers in the ring */
	int nValid;									/* Number of frames valid since last reset */
	int iLast;									/* index of last buffer used (from events) */
	int iShow;									/* index of currently displayed frame */
} RING_INFO;
#pragma pack()

#pragma pack(4)
typedef struct _TRIG_INFO {
	int msWait;									/* ms to wait for previous trig to complete before switch */
	int BurstLength;							/* # of images to capture in burst mode */
	enum {TRIG_POS_POLARITY=0, TRIG_NEG_POLARITY=1} ExtTrigPolarity;
} TRIG_INFO;
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

int Camera_Trigger(HWND hdlg, WND_INFO *wnd, int msWait);
TRIG_MODE Camera_SetTrigMode(HWND hdlg, WND_INFO *wnd, TRIG_MODE mode, int msWait);
TRIG_MODE Camera_GetTrigMode(HWND hdlg, WND_INFO *wnd);

int Camera_GetRingInfo(HWND hdlg, WND_INFO *wnd, RING_INFO *info);
int Camera_SetRingBufferSize(HWND hdlg, WND_INFO *wnd, int nBuf);

int Camera_RenderImage(HWND hdlg, WND_INFO *wnd, int frame, HWND hwnd);
int Camera_SaveImage(HWND hdlg, WND_INFO *wnd);

#endif			/* CAMERA_LOADED */
