/* Thorlabs Camera Utility */
/* === ideas ===
(1) Have float window copy the image data so can render again after a new live window created
================ */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE					/* Always require POSIX standard */

#define NEED_WINDOWS_LIBRARY			/* Define to include windows.h call functions */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>						/* for defining several useful types and macros */
#include <stdlib.h>						/* for performing a variety of operations */
#include <stdio.h>
#include <string.h>						/* for manipulating several kinds of strings */
#include <time.h>
#include <direct.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>						 /* C99 extension to get known width integers */

/* Extend from POSIX to get I/O and thread functions */
#undef _POSIX_
	#include <stdio.h>					/* for performing input and output */
	#include <io.h>						/* For _open_osfhandle and _fdopen */
	#include <fcntl.h>					/* For _O_RDONLY */
	#include <process.h>					/* for process control fuctions (e.g. threads, programs) */
#define _POSIX_

/* Standard Windows libraries */
#ifdef NEED_WINDOWS_LIBRARY
#define STRICT							/* define before including windows.h for stricter type checking */
	#include <windows.h>					/* master include file for Windows applications */
	#include <windowsx.h>				/* Extensions for GET_X_LPARAM */
	#include <commctrl.h>
	#include <wingdi.h>					/* Bitmap headers */
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "win32ex.h"
#include "graph.h"
#include "resource.h"
#include "timer.h"

// #define	USE_NUMATO
#define	USE_KEITHLEY
#define	USE_FOCUS

#ifdef USE_NUMATO
	#define	NUMATO_COM_PORT	4			/* Set to COM port to use */
	#include "Numato_DIO.h"					/* For toggling an LED between images */
#endif

#ifdef USE_KEITHLEY
	#include "ki224.h"
#endif

/* Load camera routines */
#include "camera.h"							/* Call either DCX or TL versions */
#include "dcx.h"								/* DCX API camera routines & info */
#define INCLUDE_TL_DETAIL_INFO
#include "tl.h"								/* TL  API camera routines & info */

/* Load the full WND detail */
#define	INCLUDE_WND_DETAIL_INFO			/* Get all of the typedefs and internal details */
#include "ZooCam.h"							/* Access to the ZooCam info */

#include "ZooCam_server.h"

#ifdef USE_FOCUS
	#include "focus_client.h"
#endif


/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#ifndef PATH_MAX
	#define	PATH_MAX	(260)
#endif

#define	nint(x)	(((x)>0) ? ( (int) (x+0.5)) : ( (int) (x-0.5)) )

#define	DFLT_FPS_MAX				(25)					/* Default range for FPS scale */
#define	MAX_FRAME_DISPLAY_HZ		(8)					/* Limit CPU consumed rendering images */

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
int InitializeScrollBars(HWND hdlg, WND_INFO *wnd);
int InitializeHistogramCurves(HWND hdlg, WND_INFO *wnd);
void FreeCurve(GRAPH_CURVE *cv);

static void AutoExposureThread(void *arglist);

static void show_sharpness_dialog_thread(void *arglist);
BOOL CALLBACK DCX_CameraInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK TL_CameraInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK NUMATODlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AutoSaveInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ROIDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

/* Camera functionsn ... just split to handle the multiple optional drivers */
static int Camera_Open(HWND hdlg, WND_INFO *wnd, CAMERA *camera);
static int DCx_CameraOpen(HWND hdlg, WND_INFO *wnd, DCX_CAMERA *dcx, UC480_CAMERA_INFO *info);
static int TL_CameraOpen(HWND hdlg, WND_INFO *wnd, TL_CAMERA *tl);

static int Camera_Close(HWND hdlg, WND_INFO *wnd);
static int DCx_CameraClose(HWND hdlg, WND_INFO *wnd, DCX_CAMERA *dcx, UC480_CAMERA_INFO *info);
static int TL_CameraClose(HWND hdlg, WND_INFO *wnd, TL_CAMERA *tl);

static int Camera_ShowImage(WND_INFO *wnd, int frame, int *pSharp);
static int DCx_ShowImage(WND_INFO *wnd, int index, int *pSharp);
static int TL_ShowImage(WND_INFO *wnd, int index, int *pSharp);

static void Camera_Info_Thread(void *arglist);

int CalcStatistics(WND_INFO *wnd, int index, unsigned char *rgb, int *pSharp);
static FILE_FORMAT GetDfltImageFormat(HWND hdlg);

/* DCx thread for monitor sequence events */
static void DCx_SequenceThread(void *arglist);
static void DCx_ImageThread(void *arglist);

static int  Set_DCx_Resolution(HWND hdlg, WND_INFO *wnd, int ImageFormatID);
static int DCx_AllocRingBuffers(WND_INFO *wnd, int nRequest);
int Init_Known_Resolution(HWND hdlg, WND_INFO *wnd, HCAM hCam);


/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of  global vars		  */
/* ------------------------------- */
HWND ZooCam_main_hdlg = NULL;											/* Global identifier of my window handle */
WND_INFO *main_wnd = NULL;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
BOOL abort_all_threads = FALSE;										/* Global signal on shutdown to abort everything */

static sig_atomic_t CalcStatistics_Active = FALSE;				/* Are we already processing statistics ... just skip call */

static sig_atomic_t TL_Process_Image_Thread_Active = FALSE;	/* For monitoring when done */
static sig_atomic_t TL_Process_Image_Thread_Abort  = FALSE;	/* Abort when no longer needed */

static HANDLE TL_Process_Image_Thread_Trigger = NULL;
static void TL_ImageThread(void *arglist);

static HINSTANCE hInstance=NULL;
static HWND float_image_hwnd;										/* Handle to free-floating image window */

/* This list must match order of radio buttons in resources.h (get index of stack) */
static int ColorCorrectionModes[] = { COLOR_DISABLE, COLOR_ENABLE, COLOR_BG40, COLOR_HQ, COLOR_AUTO_IR };
#define	N_COLOR_MODES	sizeof(ColorCorrectionModes)/sizeof(*ColorCorrectionModes)

#define	ID_NULL			(-1)

/* List of information about the exposure radio buttons */
struct {
	int wID;
	double exp_min, exp_max;
	char *str_min, *str_mid, *str_max;
} ExposureList[] = {
	{IDR_EXPOSURE_100US,   0.1,    10.0, "100 us", "1 ms",   "10 ms"},
	{IDR_EXPOSURE_1MS,     1.0,   100.0, "1 ms",   "10 ms",  "100 ms"},
	{IDR_EXPOSURE_10MS,   10.0,  1000.0, "10 ms",  "100 ms", "1000 ms"},
	{IDR_EXPOSURE_100MS, 100.0, 10000.0, "100 ms",  "1 s",   "10 s"}
};
#define N_EXPOSURE_LIST	(sizeof(ExposureList)/sizeof(ExposureList[0]))

/* List of all camera controls ... disabled when no camera exists */
int AllCameraControls[] = { 
	IDB_CAMERA_DISCONNECT, IDC_CAMERA_MODES, IDB_CAMERA_DETAILS,
	IDB_SAVE_PARAMETERS, IDB_LOAD_PARAMETERS,
	IDC_LIVE, IDB_TRIGGER, IDC_ARM,
	IDR_TRIG_FREERUN, IDR_TRIG_SOFTWARE, IDR_TRIG_EXTERNAL, IDR_TRIG_SS, IDR_TRIG_BURST, IDT_TRIG_COUNT, IDV_TRIG_COUNT,
	IDR_TRIG_POS, IDR_TRIG_NEG, IDB_BURST_ARM,
	IDB_SAVE, IDB_SAVE_BURST,
	IDV_RING_SIZE, IDB_RESET_RING, IDS_FRAME_RATE, IDV_FRAME_RATE, IDT_ACTUALFRAMERATE, IDS_EXPOSURE_TIME, IDV_EXPOSURE_TIME,
	IDB_SHARPNESS_DIALOG, IDS_GAMMA, IDV_GAMMA, IDB_GAMMA_NEUTRAL,
	IDR_COLOR_DISABLE, IDR_COLOR_ENABLE, IDR_COLOR_BG40, IDR_COLOR_HQ, IDR_COLOR_AUTO_IR, 
	IDG_COLOR_CORRECTION, IDV_COLOR_CORRECT_FACTOR, IDS_TEXT_0,
	IDT_RED_SATURATE, IDT_GREEN_SATURATE, IDT_BLUE_SATURATE, 
	IDS_MASTER_GAIN, IDV_MASTER_GAIN, IDS_RED_GAIN, IDV_RED_GAIN, IDS_GREEN_GAIN, IDV_GREEN_GAIN, IDS_BLUE_GAIN, IDV_BLUE_GAIN, IDB_RESET_GAINS,
	IDR_EXPOSURE_100US, IDR_EXPOSURE_1MS, IDR_EXPOSURE_10MS, IDR_EXPOSURE_100MS,
	IDC_SHOW_INTENSITY, IDC_SHOW_RGB, IDC_SHOW_SUM, IDC_TRACK_CENTROID,
	IDT_FRAME_COUNT, IDV_CURRENT_FRAME, IDT_FRAME_VALID, IDB_NEXT_FRAME, IDB_PREV_FRAME,
	IDR_IMAGE_BMP, IDR_IMAGE_RAW, IDR_IMAGE_JPG, IDR_IMAGE_PNG,
	IDB_ROI, IDT_ROI_INFO,
	ID_NULL
};

/* List of camera controls that get turned off when starting resolution change */
int CameraOffControls[] = { 
	IDB_SAVE_PARAMETERS, IDB_LOAD_PARAMETERS,
	IDC_LIVE, IDB_TRIGGER, IDC_ARM,
	IDR_TRIG_POS, IDR_TRIG_NEG, IDB_BURST_ARM,
	IDR_TRIG_FREERUN, IDR_TRIG_SOFTWARE, IDR_TRIG_EXTERNAL, IDR_TRIG_SS, IDR_TRIG_BURST, IDT_TRIG_COUNT, IDV_TRIG_COUNT,
	IDB_SAVE, IDB_SAVE_BURST,
	IDV_RING_SIZE, IDB_RESET_RING, IDS_FRAME_RATE, IDV_FRAME_RATE, IDT_ACTUALFRAMERATE, IDS_EXPOSURE_TIME, IDV_EXPOSURE_TIME,
	IDB_SHARPNESS_DIALOG, IDS_GAMMA, IDV_GAMMA, IDB_GAMMA_NEUTRAL,
	IDR_COLOR_DISABLE, IDR_COLOR_ENABLE, IDR_COLOR_BG40, IDR_COLOR_HQ, IDR_COLOR_AUTO_IR, 
	IDG_COLOR_CORRECTION, IDV_COLOR_CORRECT_FACTOR, IDS_TEXT_0,
	IDT_RED_SATURATE, IDT_GREEN_SATURATE, IDT_BLUE_SATURATE, 
	IDS_MASTER_GAIN, IDV_MASTER_GAIN, IDS_RED_GAIN, IDV_RED_GAIN, IDS_GREEN_GAIN, IDV_GREEN_GAIN, IDS_BLUE_GAIN, IDV_BLUE_GAIN, IDB_RESET_GAINS,
	IDR_EXPOSURE_100US, IDR_EXPOSURE_1MS, IDR_EXPOSURE_10MS, IDR_EXPOSURE_100MS,
	IDC_SHOW_INTENSITY, IDC_SHOW_RGB, IDC_SHOW_SUM, IDC_TRACK_CENTROID,
	IDT_FRAME_COUNT, IDV_CURRENT_FRAME, IDT_FRAME_VALID, IDB_NEXT_FRAME, IDB_PREV_FRAME,
	IDR_IMAGE_BMP, IDR_IMAGE_RAW, IDR_IMAGE_JPG, IDR_IMAGE_PNG,
	IDB_ROI, IDT_ROI_INFO,
	ID_NULL
};

/* List of camera controls that get turned on when resolution is set */
int CameraOnControls[] = { 
	IDC_CAMERA_LIST, 
	IDT_RED_SATURATE, IDT_GREEN_SATURATE, IDT_BLUE_SATURATE, 
	IDR_EXPOSURE_100US, IDR_EXPOSURE_1MS, IDR_EXPOSURE_10MS, IDR_EXPOSURE_100MS,
	IDC_SHOW_INTENSITY, IDC_SHOW_RGB, IDC_SHOW_SUM, IDC_TRACK_CENTROID,
	IDB_SHARPNESS_DIALOG, IDV_RING_SIZE, IDB_RESET_RING,
	IDT_ACTUALFRAMERATE, IDS_EXPOSURE_TIME, IDV_EXPOSURE_TIME,
#ifdef USE_RINGS
	IDT_FRAME_COUNT, IDV_CURRENT_FRAME, IDT_FRAME_VALID, IDB_NEXT_FRAME, IDB_PREV_FRAME,
#endif
	ID_NULL
};

/* List of camera controls that get disabled while in "ARM" mode */
int BurstArmControlsDisable[] = { 
	IDC_CAMERA_LIST, IDC_CAMERA_MODES,
	IDB_SAVE_PARAMETERS, IDB_LOAD_PARAMETERS,
	IDC_LIVE, IDB_TRIGGER,
	IDR_TRIG_FREERUN, IDR_TRIG_SOFTWARE, IDR_TRIG_EXTERNAL, IDR_TRIG_SS, IDR_TRIG_BURST, IDT_TRIG_COUNT, IDV_TRIG_COUNT,
	IDB_SAVE, IDB_SAVE_BURST,
	IDC_FLOAT, IDB_SHARPNESS_DIALOG, IDV_RING_SIZE, IDB_RESET_RING,
	IDB_NEXT_FRAME, IDB_PREV_FRAME,
	ID_NULL
};

int BurstArmControlsReenable[] = { 
	IDC_CAMERA_LIST, IDC_CAMERA_MODES,
	IDB_SAVE_PARAMETERS, IDB_LOAD_PARAMETERS,
	IDB_SAVE, IDB_SAVE_BURST,
	IDC_FLOAT, IDB_SHARPNESS_DIALOG, IDV_RING_SIZE, IDB_RESET_RING,
	IDB_NEXT_FRAME, IDB_PREV_FRAME,
	ID_NULL
};

/* Elements required for working with client/server to focus process */
#ifdef USE_FOCUS
	BOOL CALLBACK SharpnessDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

	static struct {
		HWND hdlg;								/* Handle to the dialog box */
		enum {TIME_SEQUENCE=0, FOCUS_SWEEP=1} mode;
		BOOL paused;
		GRAPH_CURVE *cv;
		GRAPH_CURVE *focus;					/* Focus position */
	} SharpnessDlg = { NULL, TIME_SEQUENCE, FALSE, NULL};
#endif

HANDLE hwndFloat = NULL;
	
/* ===========================================================================
-- Callback routine for floating image ... resizable image
=========================================================================== */
#define	CX_OFFSET	(20)					/* Extra width of x-direction		*/
#define	CY_OFFSET	(43)					/* Size of bar at top of window	*/

LRESULT CALLBACK FloatImageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	int cxClient, cyClient;
	RECT *pWindow, Client;
	POINT point;
	BOOL rc;
	double aspect;								/* needed Width/Height ratio (640/480) */
	HDC	hdc;									/* Device context for drawing */
	PAINTSTRUCT paintstruct;

	WND_INFO *wnd;
	HWND hdlg;

	/* Recover the main window process for use in this routine */
	wnd = main_wnd;
	hdlg = wnd->hdlg;							/* And main window dialog box */

	/* Default return and process messages */
	rc = FALSE;
	switch (msg) {

		case WM_CREATE:
			SetWindowText(hwnd, "Zoo Camera Imaging");
			wnd->hdlg_float = hwndFloat = hwnd;
			Camera_RenderFrame(wnd, -1, hwnd);								/* Render the current image */
			rc = TRUE; break;
			
		case WM_PAINT:
			hdc = BeginPaint(hwnd, &paintstruct);							/* Get DC */
			EndPaint(hwnd, &paintstruct);										/* Release DC */
			rc = TRUE; break;
			
		case WM_RBUTTONDOWN:
			GetCursorPos((LPPOINT) &point);
			ScreenToClient(hwnd, &point);
			GetClientRect(hwnd, &Client);
			wnd->cursor_posn.x = (1.0*point.x) / Client.right;
			wnd->cursor_posn.y = (1.0*point.y) / Client.bottom;
			SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
			rc = TRUE; break;

		case WM_LBUTTONDBLCLK:								/* Magnify at this location */
		case WM_MBUTTONDOWN:
			break;

		case WM_SIZING:
			aspect = (wnd->height != 0) ? 1.0 * wnd->width / wnd->height : 16.0/9.0 ;
			pWindow = (RECT *) lParam;												/* left, right, left, bottom */
			cxClient = (pWindow->right - pWindow->left) - CX_OFFSET;
			cyClient = (pWindow->bottom - pWindow->top) - CY_OFFSET;

			/* Determine how the size of the system changes ... lock aspect ratio */
			switch (wParam) {
				case WMSZ_RIGHT:
				case WMSZ_LEFT:
					cyClient = (int) (cxClient / aspect + 0.5);
					break;
				case WMSZ_TOP:
				case WMSZ_BOTTOM:
					cxClient = (int) (cyClient * aspect + 0.5);
					break;
				case WMSZ_BOTTOMLEFT:												/* Choose to keep the larger and expand to fit aspect ratio */
				case WMSZ_BOTTOMRIGHT:
				case WMSZ_TOPLEFT:
				case WMSZ_TOPRIGHT:
					if (1.0*cxClient/cyClient > aspect) {						/* Change cyClient */
						cyClient = (int) (cxClient / aspect + 0.5);
					} else {																/* Change cxClient */
						cxClient = (int) (cyClient * aspect + 0.5);
					}
					break;
			}
			/* Now modify the appropriate corner of the new rectangle */
			switch (wParam) {
				case WMSZ_RIGHT:
				case WMSZ_BOTTOM:
				case WMSZ_BOTTOMRIGHT:
					pWindow->right = pWindow->left + cxClient + CX_OFFSET;
					pWindow->bottom = pWindow->top + cyClient + CY_OFFSET;
					break;
				case WMSZ_LEFT:
				case WMSZ_BOTTOMLEFT:
					pWindow->left = pWindow->right - cxClient - CX_OFFSET;
					pWindow->bottom = pWindow->top + cyClient + CY_OFFSET;
					break;
				case WMSZ_TOP:
				case WMSZ_TOPRIGHT:
					pWindow->right = pWindow->left + cxClient + CX_OFFSET;
					pWindow->top = pWindow->bottom - cyClient - CY_OFFSET;
					break;
				case WMSZ_TOPLEFT:
					pWindow->left = pWindow->right - cxClient - CX_OFFSET;
					pWindow->top = pWindow->bottom - cyClient - CY_OFFSET;
					break;
			}
			/* Message was handled - important here to acknowledge */
			rc = TRUE; break;

		case WM_SIZE:
			cxClient = LOWORD(lParam);
			cyClient = HIWORD(lParam);
			InvalidateRect(hwnd, NULL, FALSE);
			break;

		case WM_DESTROY:
			SetDlgItemCheck(hdlg, IDC_FLOAT, FALSE);							/* Pass message to main dialog box */
			PostQuitMessage(0);
			wnd->hdlg_float = hwndFloat = NULL;
			break;

/* All other messages (a lot of them) are processed using default procedures */
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return rc;
}


/* ===========================================================================
-- Routine to calculate the sharpness of an image in memory
--
-- Usage: int CalcSharpness(WND_INFO *wnd, unsigned char *pMem);
--
-- Inputs: wnd     - structure with all the info
--         width   - width of the image
--         height  - height of the image
--         pitch   - bytes bewteen successful rows of the image
--         iscolor - TRUE if image is in RGB format (3 bytes/pixel), FALSE if greyscale (1 byte/pixel)
--         pMem    - pointer to the image buffer in memory
--
-- Output: none
--
-- Return: estimate of sharpness (in some units) or 0 on error
=========================================================================== */
int CalcSharpness(WND_INFO *wnd, int width, int height, int pitch, BOOL iscolor, unsigned char *pMem) {
	int ix0, iy0, col, line, delta, delta_max, xspan, yspan;
	unsigned char *aptr;

	if (wnd == NULL || pMem == NULL) return 0;

#define	SCAN_BLOCK	(128)																	/* edge of box measured */

	/* Set up a rectangular region to scan for derivative */
	xspan = (width  >= SCAN_BLOCK) ? SCAN_BLOCK : width ;							/* How wide is the regions explored? */
	yspan = (height >= SCAN_BLOCK) ? SCAN_BLOCK : height ;

	ix0 = ((int) (width *wnd->cursor_posn.x+0.5)) - xspan/2;						/* First column of test */
	if (ix0 < 0) ix0 = 0;
	if ((ix0 + xspan) > width)  ix0 = width-xspan;

	iy0 = ((int) (height*wnd->cursor_posn.y+0.5)) - yspan/2;						/* First row of test */
	if (iy0 < 0) iy0 = 0;
	if ((iy0 + yspan) > height) iy0 = height-yspan;

	delta_max = 0;
	for (col=ix0; col<ix0+xspan; col++) {
		for (line=iy0; line<iy0+yspan; line++) {
			aptr = pMem + line*pitch;
			/* Consider horizontal changes */
			if (iscolor) {
				delta = aptr[3*col+3]+aptr[3*col+4]+aptr[3*col+5] - (aptr[3*col+0]+aptr[3*col+1]+aptr[3*col+2]);
			} else {
				delta = aptr[col+1]-aptr[col];
			}
			if (abs(delta) > delta_max) delta_max = abs(delta);
			/* And now vertical changes */
			if (iscolor) {
				delta = aptr[3*col+pitch]+aptr[3*col+pitch+1]+aptr[3*col+pitch+2] - (aptr[3*col+0]+aptr[3*col+1]+aptr[3*col+2]);
			} else {
				delta = aptr[col+pitch]-aptr[col];
			}
			if (abs(delta) > delta_max) delta_max = abs(delta);
		}
	}
	return delta_max;
}


/* ===========================================================================
-- Thread to autoset the intentity so max is between 95 and 100% with
-- no saturation of any of the channels
--
-- Usage: _beginthread(AutoExposureThread, 0, NULL);
=========================================================================== */
static void AutoExposureThread(void *arglist) {
	static char *rname="AutoExposureThread";

	static sig_atomic_t active=FALSE;

	int i, try;
	char msg[20];
	WND_INFO *wnd;
	HWND hdlg;
	double exposure, last_exposure;					/* Exposure time in ms */
	double upper_bound, lower_bound, min_increment;
	int LastImage, max_saturate, red_peak, green_peak, blue_peak, peak;

	/* Get a pointer to the data structure */
	wnd = (WND_INFO*) arglist;
	hdlg = wnd->hdlg;
	if (hdlg != NULL && ! IsWindow(hdlg)) hdlg = NULL;		/* Mark hdlg if not window */

	/* Avoid multiple by just monitoring myself */
	if (active || ! wnd->LiveVideo) {
		fprintf(stderr, "[%s] Either already active (%d) or no live video (%d)\n", rname, active, wnd->LiveVideo); fflush(stderr);
		Beep(300,200);
		return;
	}
	active = TRUE;

	if (hdlg != NULL) {
		EnableDlgItem(hdlg, IDB_AUTO_EXPOSURE, FALSE);
		SetDlgItemText(hdlg, IDB_AUTO_EXPOSURE, "iter 0");
	}

	/* Query exposure time limits (in ms) */
	Camera_GetExposureParms(wnd, &lower_bound, &upper_bound, &min_increment);

	/* Set maximum number of saturated pixels to tolerate */
	max_saturate = wnd->width*wnd->height / 1000;			/* Max tolerated as saturated */

	/* Do binary search ... Starting from 1000 ms, will get to within 0.03 ms */
	fprintf(stderr, "iter\texposure \tlower bound\tupper bound\tnew expose\tlast expose\n"); fflush(stderr);
	for (try=0; try<15; try++) {

		/* Get current exposure time (ms) and image info */
		exposure = Camera_GetExposure(wnd);
		last_exposure = exposure;									/* Hold so know change at end */
		LastImage = wnd->Image_Count;

		/* Print debug information */
		fprintf(stderr, "%d\t%9.3f\t%9.3f\t%9.3f", try, exposure, lower_bound, upper_bound); fflush(stderr);

		/* If saturated, new upper_bound and use mid-point of lower/upper next time */
		if (wnd->red_saturate > max_saturate || wnd->green_saturate > max_saturate || wnd->blue_saturate > max_saturate) {
			upper_bound = exposure;
			exposure = (upper_bound + lower_bound) / 2.0;
			if (exposure > 500 && exposure > lower_bound) exposure = 500;	/* Don't sit at long exposures unless necessary */

		/* Okay, intensity is low, not high ... ignore top max_saturate pixels to get highest intensity */
		} else {
			lower_bound = exposure;

			peak = max_saturate - wnd->red_saturate;
			for (i=255; i>=0,peak>0; i--) peak -= (int) wnd->red_hist->y[i];
			red_peak = i+1;

			peak = max_saturate - wnd->green_saturate;
			for (i=255; i>=0,peak>0; i--) peak -= (int) wnd->green_hist->y[i];
			green_peak = i+1;

			peak = max_saturate - wnd->blue_saturate;
			for (i=255; i>=0,peak>0; i--) peak -= (int) wnd->blue_hist->y[i];
			blue_peak = i+1;

			peak = max(red_peak, green_peak); peak = max(peak, blue_peak);

			fprintf(stderr, "\t%3d/%d\t%3d/%d\t%3d/%d\t%3d", red_peak, wnd->red_saturate, green_peak, wnd->green_saturate, blue_peak, wnd->blue_saturate, peak);

			if (peak >= 245) {
				fprintf(stderr, "\n  -- peak is >245 ... close enough to done\n"); fflush(stderr);
				break;															/* We are done */
			}
			/* Estimate exposure where peak=250, but be careful extrapolating too far */
			/* If extrapolate more than 2x, don't jump more than 2 "binary" levels	  */
			exposure *= 250.0/max(1,peak);					/* Avoid zero divide */
			if (peak > 125) {										/* Less than factor of 2... just use */
				exposure = min(exposure, upper_bound);
			} else {
				exposure = min(exposure, 0.25*last_exposure+0.75*upper_bound);
			}
		}
		fprintf(stderr, "\t%9.3f\t%9.3f\n", exposure, last_exposure); fflush(stderr);

		/* minimal change ... declare life done */
		if (fabs(exposure-last_exposure) < min_increment) {
			fprintf(stderr, "-- exposure within minimum increment (%f)\n", min_increment); fflush(stderr);
			break;
		/* Don't both going up by less than 3% */
		} else if ( (exposure > last_exposure) && ((exposure-last_exposure) < 0.03*last_exposure)) {
			fprintf(stderr, "-- exposure change less than 3%%\n"); fflush(stderr);
			break;
		}

		/* Reset the gain now and collect a few images to stabilize */
		Camera_SetExposure(wnd, exposure);

		/* Wait for at least the 3th new exposure to stabilize image */
		if (hdlg != NULL) {
			sprintf_s(msg, sizeof(msg), "iter %d", try+1);
			SetDlgItemText(hdlg, IDB_AUTO_EXPOSURE, msg);
		}
		/* Need to wait at least for two frame periods (maximum update rate) */
		/* Values for saturation only updated with images that are displayed */
		Sleep(2100/MAX_FRAME_DISPLAY_HZ);

		/* And then keep sleeping until we know we have had two new images */
		for (i=0; i<10; i++) {
			if (wnd->Image_Count > LastImage+2) break;
			Sleep(max(nint(exposure),50));
		}
		if (wnd->Image_Count <= LastImage+2) {
			fprintf(stderr, "Failed to get at least 2 new images (Image_Count: %d   LastImage: %d)\n", wnd->Image_Count, LastImage); fflush(stderr);
			break;									/* No new image - abort */
		}
	}
	fprintf(stderr, "final\t%9.3f\t%9.3f\t%9.3f\n", exposure, lower_bound, upper_bound); fflush(stderr);

	active = FALSE;						/* We are done with what we will try */
	if (hdlg != NULL) {
		SetDlgItemText(hdlg, IDB_AUTO_EXPOSURE, "Auto");
		EnableDlgItem(hdlg, IDB_AUTO_EXPOSURE, TRUE);
	}

	return;
}

/* ===========================================================================
-- Thread from IDB_BURST_ARM ... waits for semaphore of a stripe starting, then
-- turns on video.  Then waits for semaphore indicating the end of the
-- stripe and turns off the video.  Call WMP_BURST_TRIG_COMPLETE when
-- done to reset window controls.
--
-- Usage: _beginthread(trigger_burst_mode, 0, (void *) wnd);
--
-- Inputs: wnd - pointer to the WND_INFO structure
--
-- Output:
--
-- Return: nothing ... just exit
--
-- Notes: 1) Expecting that the camera is in TRIG_BURST mode
--        2) Opens semaphores to LasGo for stripe start/end signals
--        3) Waits for start semaphore, or flag to abort
=========================================================================== */
static void trigger_burst_mode(void *arglist) {

	WND_INFO *wnd;
	HWND hdlg;
	HANDLE start, end;
	char szTmp[256];
	int rc;

	static sig_atomic_t active=FALSE;

	/* Get a pointer to the data structure */
	wnd = (WND_INFO*) arglist;
	hdlg = wnd->hdlg;
	if (hdlg == NULL || ! IsWindow(hdlg)) hdlg = NULL;		/* Mark hdlg if not window */

	/* Avoid multiple by just monitoring myself */
	if (active) { Beep(300,200); return; }
	active = TRUE;

	/* Get the two global semaphores that LasGo uses to signal stripes */
	start = OpenEvent(SYNCHRONIZE, FALSE, "LasGoStripeStart");
	end   = OpenEvent(SYNCHRONIZE, FALSE, "LasGoStripeEnd");
	if (start == NULL || end == NULL) {
		sprintf_s(szTmp, sizeof(szTmp), "Unable to open the semaphores to the LasGo stripe triggers\n  start: %p  end: %p", start, end);
		MessageBox(NULL, szTmp, "Image Burst Capture Failed", MB_ICONERROR | MB_OK);
		goto BurstThreadFail;
	}

	/* Wait for the start semaphore from a stripe */
	/* Use 500 ms wait to be able to watch for aborts */
	Camera_Arm(wnd, TRIG_ARM);											/* Arm the camera ... */
	if (hdlg != NULL) SetDlgItemText(hdlg, IDC_ARM, "Burst: armed");
	wnd->BurstModeStatus = BURST_STATUS_ARMED;					/* Mark that we are really armed now */
	while (TRUE) {
		if (! wnd->BurstModeActive) goto BurstThreadAbort;
		if ( (rc = WaitForSingleObject(start, 500)) == WAIT_OBJECT_0) break;
		if (rc == WAIT_TIMEOUT) continue;
			sprintf_s(szTmp, sizeof(szTmp), "Wait for LasGoStripeStart semaphore returned error\n  rc=%d", rc);
			MessageBox(NULL, szTmp, "Image Burst Capture Failed", MB_ICONERROR | MB_OK);
			goto BurstThreadFail;
	}

	/* Scan has started.  Turn on video and let it collect frames */
	if (! wnd->BurstModeActive) goto BurstThreadAbort;
	wnd->BurstModeStatus = BURST_STATUS_RUNNING;
	Camera_Trigger(wnd);											/* Send software trigger to camera driver */
	if (hdlg != NULL) SetDlgItemText(hdlg, IDC_ARM, "Burst: running");
	if (hdlg != NULL) SetDlgItemCheck(hdlg, IDC_LIVE, TRUE);	/* Show triggered state in the button */

	/* Wait for the end signal ... but never more than 10 seconds */
	rc = WaitForSingleObject(end, 10000);
	Camera_Arm(wnd, TRIG_DISARM);
	if (hdlg != NULL) SetDlgItemCheck(hdlg, IDC_LIVE, FALSE);
	wnd->BurstModeStatus = BURST_STATUS_COMPLETE;
	goto BurstThreadExit;

BurstThreadFail:
	wnd->BurstModeStatus = BURST_STATUS_FAIL;
	goto BurstThreadExit;
	
BurstThreadAbort:
	wnd->BurstModeStatus = BURST_STATUS_ABORT;
	goto BurstThreadExit;

BurstThreadExit:
	if (hdlg != NULL)  SetDlgItemText(hdlg, IDC_ARM, "Burst: standby");
	if (start != NULL) CloseHandle(start);
	if (end   != NULL) CloseHandle(end);
	if (hdlg  != NULL) SendMessage(hdlg, WMP_BURST_COMPLETE, 0, 0);

	/* No longer active and BurstMode is no longer armed */
	active = FALSE;
	wnd->BurstModeActive = FALSE;
	
	return;
}

/* ===========================================================================
-- The 'main' function of Win32 GUI programs ... now for a new imaging thread 
=========================================================================== */
int WINAPI ImageWindow(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	WNDCLASSEX wc;			/* A properties struct of our window */
	MSG msg;					/* A temporary location for all messages */
	static BOOL first = TRUE;
	WND_INFO *wnd;
	int cxSize, cySize;
	double aspect;

/* zero out the struct and set the stuff we want to modify */
	if (first) {
		memset(&wc,0,sizeof(wc));
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_DBLCLKS;						/* Allow double clicks to be registered */
		wc.lpfnWndProc = FloatImageWndProc;		/* This is where we will send messages to */
		wc.hInstance = hThisInstance;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);

/*		White, COLOR_WINDOW is just a #define for a system color, try Ctrl+Clicking it */
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
		wc.lpszClassName = "FloatGraphClass";
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);			/* Load a standard icon */
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);		/* use the name "A" to use the project icon */

		if (! RegisterClassEx(&wc)) {
			MessageBox(NULL, "Window Registration Failed!","Error!",MB_ICONEXCLAMATION|MB_OK);
			return 0;
		}
		first = FALSE;
	}

	/* Figure out the appropriate size */
	wnd = main_wnd;
	aspect = (wnd->height != 0) ? 1.0 * wnd->width / wnd->height : 16.0/9.0 ;
	cxSize = 640;
	cySize = (int) (cxSize/aspect + 0.5);
	cxSize += CX_OFFSET;						/* Offsets up in sizing code for window */
	cySize += CY_OFFSET;

	float_image_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, "FloatGraphClass", "Caption", WS_VISIBLE | WS_OVERLAPPEDWINDOW,
								 CW_USEDEFAULT, /* x */
								 CW_USEDEFAULT, /* y */
								 cxSize, /* width */
								 cySize, /* height */
								 NULL, NULL, hThisInstance, NULL);
	if (float_image_hwnd == NULL) {
		MessageBox(NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	/* Get and process the message loop */
	while(GetMessage(&msg, NULL, 0, 0) > 0) { /* If no error is received... */
		TranslateMessage(&msg);						 /* Translate key codes to chars if present */
		DispatchMessage(&msg);						 /* Send it to FloatImageWndProc */
	}

	float_image_hwnd = NULL;
//	return msg.wParam;
	return 0;
}

static void start_image_window(void *arglist) {
	ImageWindow(NULL, NULL, NULL, 1);
	return;
}

static void start_Keith224_LED_window(void *arglist) {
	return;
}
static void start_NUMATO_LED_window(void *arglist) {
	return;
}

/* ===========================================================================
-- Routine to clean up on exit.  Use via atexit() routine.
-- 
-- Usage: void Final_Closeout(void);
--
-- Inputs: none
--
-- Output: releases camera if initialized, frees memory
--
-- Return: none
=========================================================================== */
void Final_Closeout(void) {
	WND_INFO *wnd;
	DCX_CAMERA *dcx;

	fprintf(stderr, "Final closeout\n"); fflush(stderr);
	
	if ( (wnd = main_wnd) != NULL) {
		printf("Performing final shutdown of cameras\n"); fflush(stdout);

		dcx = wnd->dcx;
		/* Release resources */
		if (dcx != NULL && dcx->FrameEvent != NULL) {
			CloseHandle(dcx->FrameEvent);
			dcx->FrameEvent = NULL;
		}
#ifdef USE_RINGS
		if (dcx != NULL && dcx->SequenceEvent != NULL) {
			CloseHandle(dcx->SequenceEvent);
			dcx->SequenceEvent = NULL;
		}
#endif

		/* Close the individual camera APIs */
		DCx_Shutdown();
		TL_Shutdown();

		/* Free memory and disable dealing with multipe calls */
		main_wnd = NULL;
		free(wnd);
	}

	return;
}

/* ===========================================================================
-- Routine consolidating all statistics run on the images
--
-- Usage: int CalcStatistics(WND_INFO *wnd, int index, unsigned char *rgb, int *pSharp);
--
-- Inputs: wnd     - structure with all the info
--         width   - width of the image
--         height  - height of the image
--         iscolor - TRUE if image is in RGB format (3 bytes/pixel), FALSE if greyscale (1 byte/pixel)
--         index   - for TL camera, index for the image
--         rgb     - pointer to the RGB image buffer in memory (byte only)
--         pSharp  - pointer to variable to get sharpness estimate (if ! NULL)
--
-- Output: *pSharp - estimate of the sharpness (in some units)
--
-- Return: 0 if successful, otherwise error
--           1 ==> nothing to do
--           2 ==> already processing statistics for another image
--
-- Notes: Only sets parameters ih the stats structure due to threading
--        problems.  TIMER_STATS_UPDATE in main window procedure actually 
--        display the results on a reasonable frequency.
--
--        Root problem was that this routine blocked while processing
--        main dialog messages.  Could not shut down a camera by processing
--        messages in the main dialog routine.
=========================================================================== */
struct {
	BOOL updated;							/* Set true when modified ... set to FALSE when handled in main */
	double R_sat, G_sat, B_sat;		/* Saturation percent of each channel (255) */
	int x_centroid, y_centroid;		/* Centroid tracking results */
	GRAPH_SCALES horz_scales,			/* Scales for the horizontal line scans */
				    vert_scales;			/* Scales for the vertical line scans */
	int sharpness;							/* Sharpness estimate */
} stats;

int CalcStatistics(WND_INFO *wnd, int index, unsigned char *rgb, int *pSharp) {
	static char *rname = "CalcStatistics";

	int i, j, col, line, w_max;
	int sharpness;
	unsigned char *aptr;
	double total_max;

	/* Image information (look up from driver) */
	int height, width, pitch;
	BOOL is_color;

	GRAPH_CURVE *red, *green, *blue;
	GRAPH_CURVE *vert, *vert_r, *vert_g, *vert_b, *vert_sum;
	GRAPH_CURVE *horz, *horz_r, *horz_g, *horz_b, *horz_sum;
	GRAPH_SCALES *scales;

	/* If the main window isn't a window, don't bother with the histogram calculations */
	if (wnd == NULL || ! IsWindow(wnd->hdlg)) return 1;
	CalcStatistics_Active = TRUE;

	/* Split based on RGB or only monochrome */
	red    = wnd->red_hist;
	green  = wnd->green_hist;
	blue   = wnd->blue_hist;
	memset(red->y,   0, red->npt  *sizeof(*red->y));
	memset(green->y, 0, green->npt*sizeof(*green->y));
	memset(blue->y,  0, blue->npt *sizeof(*blue->y));

	/* At this point, split based on the camera ... DCx versus TL */
	/* The TL will use the raw structure rather than bmp conversion for statistics */
	if (wnd->Camera.driver == TL) {
		int b,g,r,w;									/* Values of R,G,B and W (grey) intensities */
		TL_CAMERA *tl;
		SHORT *data;

		/* Get camera information */
		tl = (TL_CAMERA *) wnd->Camera.details;

		/* Look up the image size info */
		height = tl->height; width = tl->width;
		is_color = tl->IsSensorColor;
		pitch = is_color ? 3*width : width;

		/* validate the image index and get pointer to raw data */
		if (index < 0) index = tl->iLast;
		data = tl->images[index].raw;			/* Image data */

		if (! is_color) {
			w_max = 0;
			for (i=0; i<height*width; i++) {
				w = max(0, min(data[i]/16+8, 255)); red->y[w]++;
				if (w > w_max) w_max = w;
			}
		} else {
			w_max = 0;
			for (i=0; i<height; i++) {
				for (j=0; j<width; j+=2) {
					g = (i%2 == 0) ? data[i*width+j] : data[i*width+j+1];
					g = max(0, min(g/16+8, 255)); green->y[g] += 2;
					if (i%2 == 0) {							/* Line with red */
						r = data[i*width+j+1];
						r = max(0, min(r/16+8, 255)); red->y[r] += 4;
					} else {
						b = data[i*width+j];
						b = max(0, min(b/16+8, 255)); blue->y[b] += 4;
					}
					w = (r+g+b)/3;
					if (w > w_max) w_max = w;
				}
			}
		}

	/* DCX looks at the RGB */
	} else if (wnd->Camera.driver == DCX) {
		DCX_CAMERA *dcx;

		/* Get camera information */
		dcx = wnd->Camera.details;

		/* Look up the image size info */
		height = wnd->height; width = wnd->width;
		is_GetImageMemPitch(dcx->hCam, &pitch);
		is_color = wnd->dcx->IsSensorColor;

		for (line=0; line<height; line++) {
			int b,g,r,w;									/* Values of R,G,B and W (grey) intensities */
			aptr = rgb + line*pitch;					/* Pointer to this line */
			w_max = 0;
			for (col=0; col<width; col++) {
				if (is_color) {
					b = aptr[3*col+0]; if (b < 0) b = 0; if (b > 255) b = 255; blue->y[b]++;
					g = aptr[3*col+1]; if (g < 0) g = 0; if (g > 255) g = 255; green->y[g]++;
					r = aptr[3*col+2]; if (r < 0) r = 0; if (r > 255) r = 255; red->y[r]++;
					w = (b+g+r)/3;
				} else {
					w = aptr[col]; if (w < 0) w = 0; if (w > 255) w = 255; red->y[w]++;
				}
				if (w > w_max) w_max = w;				/* Track maximum for centroid calculations */
			}
		}
	} else {
		CalcStatistics_Active = FALSE;
		return 2;
	}

	/* Now process the information */
	if (is_color) {
		stats.R_sat = (100.0*red->y[255]  )/(1.0*height*width);
		stats.G_sat = (100.0*green->y[255])/(1.0*height*width);
		stats.B_sat = (100.0*blue->y[255] )/(1.0*height*width);

		wnd->red_saturate   = (int) red->y[255];		red->y[255]   = 0;
		wnd->green_saturate = (int) green->y[255];	green->y[255] = 0;
		wnd->blue_saturate  = (int) blue->y[255];		blue->y[255]  = 0;
		red->modified = green->modified = blue->modified = TRUE;
		red->visible  = green->visible  = blue->visible  = TRUE;
		red->rgb = RGB(255,0,0);
	} else {
		stats.R_sat = stats.G_sat = stats.B_sat = (100.0*red->y[255]  )/(1.0*height*width);

		wnd->red_saturate = wnd->green_saturate = wnd->blue_saturate = (int) red->y[255];
		red->modified = TRUE;
		red->visible = TRUE; green->visible = FALSE; blue->visible = FALSE;
		red->rgb = RGB(225,225,255);
	}

	/* Should we move the cursor based on the centroid of intensity */
	/* Algorithm is to only consider intensities 50% of maximum and above */
	if (wnd->track_centroid) {
		double z0,xz,yz,zi;
		xz = yz = z0 = 0;
		for (line=0; line<height; line++) {
			aptr = rgb + line*pitch;					/* Pointer to this line */
			for (col=0; col<width; col++) {
				if (is_color) {
					zi = aptr[3*col+0]+aptr[3*col+1]+aptr[3*col+2];
				} else {
					zi = aptr[col];
				}
				if (zi > w_max/2) {
					xz += col*zi;
					yz += line*zi;
					z0 += zi;
				}
			}
		}
		if (z0 > 0 && width  > 0) {
			wnd->cursor_posn.x = xz/z0/width;
			stats.x_centroid = nint(xz/z0);
		}
		if (z0 > 0 && height > 0) {
			wnd->cursor_posn.y = yz/z0/height;
			stats.y_centroid = nint(yz/z0);
		}
//		fprintf(stderr, "New cursor at: %f %f\n", xz/z0, yz/z0); fflush(stderr);
	}

	/* Do the horizontal profile at centerline */
	horz     = wnd->horz_w;
	horz_r   = wnd->horz_r;
	horz_g   = wnd->horz_g;
	horz_b   = wnd->horz_b;
	horz_sum = wnd->horz_sum;
	if (horz->nptmax < width) {
		horz->x     = realloc(horz->x, width*sizeof(*horz->x));
		horz->y     = realloc(horz->y, width*sizeof(*horz->y));
		horz_r->x   = realloc(horz_r->x, width*sizeof(*horz_r->x));
		horz_r->y   = realloc(horz_r->y, width*sizeof(*horz_r->y));
		horz_g->x   = realloc(horz_g->x, width*sizeof(*horz_g->x));
		horz_g->y   = realloc(horz_g->y, width*sizeof(*horz_g->y));
		horz_b->x   = realloc(horz_b->x, width*sizeof(*horz_b->x));
		horz_b->y   = realloc(horz_b->y, width*sizeof(*horz_b->y));
		horz_sum->x = realloc(horz_sum->x, width*sizeof(*horz_sum->x));
		horz_sum->y = realloc(horz_sum->y, width*sizeof(*horz_sum->y));
		horz->nptmax = horz_r->nptmax = horz_g->nptmax = horz_b->nptmax = horz_sum->nptmax = width;
	}
	horz->npt = horz_r->npt = horz_g->npt = horz_b->npt = horz_sum->npt = width;

	/* Copy the profile at the cross-hair */
	aptr = rgb + pitch*((int) (height*wnd->cursor_posn.y+0.5));			/* Pointer to target line */
	for (i=0; i<width; i++) {
		horz->x[i] = horz_r->x[i] = horz_g->x[i] = horz_b->x[i] = i;
		if (is_color) {
			horz->y[i] = (aptr[3*i+0] + aptr[3*i+1] + aptr[3*i+2])/3.0 ;		/* Average intensity */
			horz_r->y[i] = aptr[3*i+2];
			horz_g->y[i] = aptr[3*i+1];
			horz_b->y[i] = aptr[3*i+0];
		} else {
			horz->y[i] = horz_r->y[i] = horz_g->y[i] = horz_b->y[i] = aptr[i];
		}
	}
	/* And do the sum of all pixels for profiling */
	total_max = 0;
	for (i=0; i<width; i++) {horz_sum->x[i] = i; horz_sum->y[i] = 0; }
	for (i=0; i<width; i++) {					/* Sum all pixels in this column */
		aptr = rgb + (is_color ? 3*i : i);
		for (j=0; j<height; j++) {
			if (is_color) {
				horz_sum->y[i] += aptr[j*pitch+0] + aptr[j*pitch+1] + aptr[j*pitch+2];
			} else {
				horz_sum->y[i] += aptr[j*pitch];
			}
			if (total_max < horz_sum->y[i]) total_max = horz_sum->y[i];
		}
	}
	for (i=0; i<width; i++) horz_sum->y[i] = nint(255.0*horz_sum->y[i]/total_max);

	/* Redraw them now */
	scales = &stats.horz_scales;
	memset(scales, 0, sizeof(*scales));
	scales->xmin = 0;	scales->xmax = width-1;
	scales->ymin = 0;  scales->ymax = 256;
	scales->autoscale_x = FALSE; scales->force_scale_x = TRUE;
	scales->autoscale_y = FALSE; scales->force_scale_y = TRUE;
	horz->modified = TRUE;
	horz->modified = horz_sum->modified = horz_r->modified = horz_g->modified = horz_b->modified = TRUE;
	/* TIMER_STATS_UPDATE sends WMP_SET_SCALES and WMP_REDRAW messages */

	/* Do the vertical profiles at centerline and total */
	vert     = wnd->vert_w;
	vert_r   = wnd->vert_r;
	vert_g   = wnd->vert_g;
	vert_b   = wnd->vert_b;
	vert_sum = wnd->vert_sum;
	if (vert->nptmax < height) {
		vert->x = realloc(vert->x, height*sizeof(*vert->x));
		vert->y = realloc(vert->y, height*sizeof(*vert->y));
		vert_r->x = realloc(vert_r->x, height*sizeof(*vert_r->x));
		vert_r->y = realloc(vert_r->y, height*sizeof(*vert_r->y));
		vert_g->x = realloc(vert_g->x, height*sizeof(*vert_g->x));
		vert_g->y = realloc(vert_g->y, height*sizeof(*vert_g->y));
		vert_b->x = realloc(vert_b->x, height*sizeof(*vert_b->x));
		vert_b->y = realloc(vert_b->y, height*sizeof(*vert_b->y));
		vert_sum->x = realloc(horz_sum->x, height*sizeof(*horz_sum->x));
		vert_sum->y = realloc(horz_sum->y, height*sizeof(*horz_sum->y));
		vert->nptmax = vert_r->nptmax = vert_g->nptmax = vert_b->nptmax = vert_sum->nptmax = height;
	}
	vert->npt = vert_r->npt = vert_g->npt = vert_b->npt = vert_sum->npt = height;

	/* Copy the profile at the cross-hair */
	for (i=0; i<height; i++) {
		vert->y[i] = vert_r->y[i] = vert_g->y[i] = vert_b->y[i] = height-1-i;
		if (is_color) {
			aptr = rgb + i*pitch + 3*((int) (width*wnd->cursor_posn.x+0.5));	/* Access first of the column */
			vert->x[i] = (3*256 - (aptr[0] + aptr[1] + aptr[2])) / 3.0;
			vert_r->x[i] = 256 - aptr[2];
			vert_g->x[i] = 256 - aptr[1];
			vert_b->x[i] = 256 - aptr[0];
		} else {
			aptr = rgb + i*pitch + ((int) (width*wnd->cursor_posn.x+0.5));		/* Access first of the column */
			vert->x[i] = vert_r->x[i] = vert_g->x[i] = vert_b->x[i] = 256 - aptr[0];
		}
	}

	/* And do the sum of all pixels for profiling */
	total_max = 0;
	for (i=0; i<height; i++) {vert_sum->y[i] = height-1-i; vert_sum->x[i] = 0; }
	for (i=0; i<height; i++) {					/* Sum all pixels in this row */
		aptr = rgb + i*pitch;
		for (j=0; j<width; j++) {
			if (is_color) {
				vert_sum->x[i] += aptr[3*j+0] + aptr[3*j+1] + aptr[3*j+2];
			} else {
				vert_sum->x[i] += aptr[j];
			}
			if (total_max < vert_sum->x[i]) total_max = vert_sum->x[i];
		}
	}
	for (i=0; i<height; i++) vert_sum->x[i] = 256 - nint(255.0*vert_sum->x[i]/total_max);

	/* Redraw them now */
	scales = &stats.vert_scales;
	memset(scales, 0, sizeof(*scales));
	scales->xmin = 0; scales->xmax = 256;
	scales->ymin = 0; scales->ymax = height-1;
	scales->autoscale_x = FALSE;  scales->force_scale_x = TRUE;
	scales->autoscale_y = FALSE;  scales->force_scale_y = TRUE;
	vert->modified = vert_sum->modified = vert_r->modified = vert_g->modified = vert_b->modified = TRUE;
	/* TIMER_STATS_UPDATE sends WMP_SET_SCALES and WMP_REDRAW messages */

	/* Calculate the sharpness - largest delta between pixels */
	stats.sharpness = sharpness = CalcSharpness(wnd, width, height, pitch, is_color, rgb);
	if (pSharp != NULL) *pSharp = sharpness;
	
	stats.updated = TRUE;

	CalcStatistics_Active = FALSE;
	return 0;
}

/* ===========================================================================
-- CameraList_Add:   Add a camera to the list of known cameras
-- CameraList_Reset: Forget all the known cameras in the list (to be regenerated)
--
-- Usage: int  CameraList_Add(CAMERA *info, int *error);
--        void CameraList_Reset(void)
--
-- Inputs: info  - a filled in structure describing a camera
--         error - pointer (possibly NULL) to get error code on problems
--
-- Output: *error - filled in with error code if error != NULL
--                  0 ==> added without issue
--                  1 ==> info was NULL
--                  2 ==> unable to allocate memory
--
-- Return: Number of cameras now in the list (always)
=========================================================================== */
static CAMERA *camera_list = NULL;					/* Allocate space for camera list */
static int camera_list_size = 0;						/* Combobox points to elements in this list */
static int camera_count = 0;

void CameraList_Reset(void) {
	if (camera_list != NULL) memset(camera_list, 0, sizeof(*camera_list)*camera_list_size);
	camera_count = 0;
	return;
}

int CameraList_Add(CAMERA *camera, int *errcode) {
	int rc;

	/* Increase memory size if needed (almost always first time) */
	if (camera_list_size <= camera_count && camera != NULL) {
		camera_list_size += 10;						/* Just add 10 ... unlikely ever to go to 20 */
		camera_list = realloc(camera_list, camera_list_size*sizeof(*camera_list));
	}

	/* Make sure the call is valid ... if not, just return camera count */
	if (camera == NULL) {
		rc = 1;
	} else if (camera_list == NULL) {
		rc = 2;
	} else {
		memcpy(camera_list+camera_count, camera, sizeof(*camera));
		camera_count++;
		rc = 0;
	}

	if (errcode != NULL) *errcode = rc;
	return camera_count;
}

/* ===========================================================================
-- Is the specified camera available for use?
--
-- Usage: BOOL Camera_Available(CAMERA *camera);
--
-- Inputs: camera - camera structure describing one of many camera types
--
-- Output: none
--
-- Return: TRUE if the camera appears to be available for use
=========================================================================== */
BOOL Camera_Available(CAMERA *camera) {
	
	BOOL rc;

	if (camera == NULL) {
		rc = FALSE;
	} else switch (camera->driver) {
		case DCX:
			rc = ! ((UC480_CAMERA_INFO *) camera->details)->dwInUse;
			break;
		case TL:
			rc = TRUE;
			break;
		default:
			rc = FALSE; break;
	}

	return rc;
}

/* ===========================================================================
-- Open a camera (either DCx or TL) and start images flowing
--
-- Usage: int Camera_Open(HWND hdlg, WND_INFO *wnd, CAMERA *camera);
--			 int DCx_CameraOpen(HWND hdlg, WND_INFO *wnd, DCX_CAMERA *dcx, UC480_CAMERA_INFO *info);
--        int TL_CameraOpen (HWND hdlg, WND_INFO *wnd, TL_CAMERA *tl);
--
-- Inputs: hdlg   - pointer to current window
--         wnd    - pointer to high level camera information
--         camera - pointer to structure identifying desired camera
--         dcx    - pointer to DCX_CAMERA structure for camera (open and fill)
--         tl     - pointer to TL_CAMERA structure for camera (open and fill)
--         
-- Output: Initializes the requested camera and updates dialog box
--
-- Return: 0 if successful
--           1 ==> camera structure invalid (NULL)
--           2 ==> camera->driver not recognized
=========================================================================== */
static int Camera_Open(HWND hdlg, WND_INFO *wnd, CAMERA *request) {
	static char *rname = "Camera_Open";

	int rc, formats;
	TL_CAMERA  *tl;
	DCX_CAMERA *dcx;
	int i;

	if (request == NULL) return 1;

	switch (request->driver) {
		case DCX:
			dcx  = wnd->dcx;
			rc = DCx_CameraOpen(hdlg, wnd, dcx, (UC480_CAMERA_INFO *) request->details);
			wnd->bColor  = dcx->IsSensorColor;							/* Copy parameters to wnd structure */
			wnd->height  = dcx->height;
			wnd->width   = dcx->width;
			wnd->fps_min = 0;
			wnd->fps_max = DFLT_FPS_MAX;
			wnd->has_fps_control = TRUE;
			break;
		case TL:
			tl = (TL_CAMERA *) request->details;
			rc = TL_CameraOpen(hdlg, wnd, tl);
			wnd->bColor  = tl->IsSensorColor;					/* Copy parameters to wnd structure */
			wnd->height  = tl->height;
			wnd->width   = tl->width;
			wnd->fps_min = tl->fps_min;
			wnd->fps_max = tl->fps_max;
			wnd->has_fps_control = tl->bFrameRateControl;
			break;
		default:
			fprintf(stderr, "[%s] Driver from camera structure invalid (%d)\n", rname, request->driver); fflush(stderr);
			rc = 2;
	}

	/* If successful, copy to the main structure and update window */
	if (rc == 0) {

		/* Enable all the common controls */
		for (i=0; CameraOnControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOnControls[i], TRUE);

		/* Enable the buttons to disconnect and to show camera information details */
		EnableDlgItem(hdlg, IDB_CAMERA_DETAILS, TRUE);
		EnableDlgItem(hdlg, IDB_CAMERA_DISCONNECT, TRUE);

		/* Enable/disable RGB profile options based on sensor type */
		EnableDlgItem  (hdlg, IDC_SHOW_RGB, wnd->bColor);
		SetDlgItemCheck(hdlg, IDC_SHOW_RGB, wnd->bColor);
		wnd->vert_r->visible = wnd->vert_g->visible = wnd->vert_b->visible = wnd->bColor;
		wnd->horz_r->visible = wnd->horz_g->visible = wnd->horz_b->visible = wnd->bColor;

		SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
		SendMessage(hdlg, WMP_SHOW_COLOR_CORRECT, 0, 0);
		SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
		SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);
		SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);

		formats = Camera_GetSaveFormatFlag(wnd);
		if ( (formats & (FL_BMP | FL_RAW | FL_JPG | FL_PNG)) == 0) formats = FL_BMP;
		EnableDlgItem(hdlg, IDR_IMAGE_BMP, formats & FL_BMP);
		EnableDlgItem(hdlg, IDR_IMAGE_RAW, formats & FL_RAW);
		EnableDlgItem(hdlg, IDR_IMAGE_JPG, formats & FL_JPG);
		EnableDlgItem(hdlg, IDR_IMAGE_PNG, formats & FL_PNG);
		SetRadioButton(hdlg, IDR_IMAGE_BMP, IDR_IMAGE_PNG,
							(formats & FL_BMP) ? IDR_IMAGE_BMP :
							(formats & FL_JPG) ? IDR_IMAGE_JPG :
							(formats & FL_BMP) ? IDR_IMAGE_PNG : IDR_IMAGE_RAW);
	} else {
		wnd->Camera.driver = UNKNOWN;
	}

	return rc;
}

/* ===========================================================================
-- Close/disconnect a camera (either DCx or TL) and stop image rendering
--
-- Usage: int Camera_Close(HWND hdlg, WND_INFO *wnd);
--			 int DCx_CameraClose(HWND hdlg, WND_INFO *wnd, UC480_CAMERA_INFO *info);
--        int TL_CameraClose (HWND hdlg, WND_INFO *wnd, TL_CAMERA *info);
=========================================================================== */
static int Camera_Close(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_Close";

	TL_CAMERA *tl;
	DCX_CAMERA *dcx;
	int i, rc;

	if (wnd == NULL) return 1;											/* Invalid call */
	if (wnd->Camera.driver == UNKNOWN) return 0;				/* Already closed */

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			if (dcx != NULL) rc = DCx_CloseCamera(dcx);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			if (tl != NULL && tl->handle != NULL) {				/* Is it possibly open */
				/* Abort the process thread cleanly ... waiting only 0.5 second ... either ends immediately or doesn't end */
				for (i=0; i<5 && TL_Process_Image_Thread_Active; i++) {
					fprintf(stderr, "[%s] [%d] Aborting TL_Image_Thread.  Abort=%d  Active=%d\n", rname, i, TL_Process_Image_Thread_Abort, TL_Process_Image_Thread_Active); fflush(stderr);
					TL_Process_Image_Thread_Abort = TRUE;	
					SetEvent(TL_Process_Image_Thread_Trigger);	/* Re-trigger */
					Sleep(100);
				}
				if (i >= 5) { fprintf(stderr, "[%s] Failed to see processing thread terminate\n", rname); fflush(stderr); }
				rc = TL_CloseCamera(tl);
			}
			EnableDlgItem(hdlg, IDB_ROI, FALSE);		ShowDlgItem(hdlg, IDB_ROI, FALSE);
			EnableDlgItem(hdlg, IDT_ROI_INFO, FALSE);	ShowDlgItem(hdlg, IDT_ROI_INFO, FALSE);
			break;
		default:
			fprintf(stderr, "[%s] Driver from camera structure invalid (%d)\n", rname, wnd->Camera.driver); fflush(stderr);
			rc = 2;
	}

	/* No camera now active */
	wnd->Camera.driver = UNKNOWN;
	wnd->LiveVideo = FALSE;
	SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);

	return rc;
}

/* ===========================================================================
-- Queries all available DCX and TL cameras and builds the combobox with options
--
-- Usage: int Fill_Camera_List_Control(HWND hdlg, WND_INFO *wnd, int *pnvalid, CAMERA **pFirst);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--         pnvalid - NULL or pointer to int to receive number of valid (available) cameras
--         pFirst  - NULL or pointer to receive first available camera in the list
--
-- Output: *pnvalid - number of valid (not in use) cameras in the list
--         *pFirst  - pointer to CAMERA structure of first available camera
--
-- Return: Number of cameras (entries in the combo box)
=========================================================================== */
int Fill_Camera_List_Control(HWND hdlg, WND_INFO *wnd, int *pnvalid, CAMERA **pFirst) {
	static char *rname = "Fill_Camera_List_Control";

	int i, count, nfree;
	CB_PTR_LIST *combolist;

	CAMERA camera, *first;
	UC480_CAMERA_INFO *dcx_info, *dcx_details;
	TL_CAMERA *tl_camera, **tl_list;
//	TL_CAMERA_INFO *tl_info, tl_details;

	CameraList_Reset();

	/* Get the DCX cameras and add to the list */
	fprintf(stderr, "[%s] Enumerating DCX list\n", rname); fflush(stderr);
	DCx_EnumCameraList(&count, &dcx_details);
	for (i=0; i<count; i++) {
		dcx_info = dcx_details + i;
		camera.driver = DCX;
		sprintf_s(camera.id,          sizeof(camera.id),          "[DCX %d]:%s (%s)", dcx_details[i].dwCameraID, dcx_details[i].Model, dcx_details[i].SerNo);
		sprintf_s(camera.description, sizeof(camera.description), "Some DCX camera");
		camera.details = (void *) (dcx_details+i);
		CameraList_Add(&camera, NULL);
		printf("DCX_Camera %d:  CameraID: %d  DeviceID: %d  SensorID: %d  InUse: %d S/N: %s  Model: %s  Status: %d\n", i, dcx_details[i].dwCameraID, dcx_details[i].dwDeviceID, dcx_details[i].dwSensorID, dcx_details[i].dwInUse, dcx_details[i].SerNo, dcx_details[i].Model, dcx_details[i].dwStatus); fflush(stdout);
	}

	/* Get the TL cameras and add to the list */
	fprintf(stderr, "[%s] Enumerating TL list\n", rname); fflush(stderr);
	TL_EnumCameraList(&count, &tl_list);
	for (i=0; i<count; i++) {
		tl_camera = tl_list[i];
		camera.driver = TL;
		sprintf_s(camera.id,          sizeof(camera.id),          "[TL %s]:%s (%s)", tl_camera->ID, tl_camera->model, tl_camera->serial);
		sprintf_s(camera.description, sizeof(camera.description), "Some TL camera");
		camera.details = (void *) tl_camera;
		CameraList_Add(&camera, NULL);
		printf("TL_Camera %d:  CameraID: %s  S/N: %s  Model: %s\n", i, tl_camera->ID, tl_camera->serial, tl_camera->model); fflush(stdout);
	}

	/* Clear combo selection boxes and mark camera invalid now */
	combolist = calloc(camera_count, sizeof(*combolist));
	for (i=0; i<camera_count; i++) {
		combolist[i].id = camera_list[i].id;
		combolist[i].value = camera_list+i;
	}
	ComboBoxClearList(hdlg, IDC_CAMERA_LIST);
	if (camera_count > 0) ComboBoxFillPtrList(hdlg, IDC_CAMERA_LIST, combolist, camera_count);
	EnableDlgItem(hdlg, IDC_CAMERA_LIST, camera_count > 0);
	free(combolist);

	/* Scan through the list to determine how many cameras are available, and the first one available */
	for (i=0,nfree=0,first=NULL; i<camera_count; i++) {
		if (! Camera_Available(camera_list+i)) continue;
		nfree++;
		if (first == NULL) first = camera_list + i;
	}

	/* Potentially return values of number free and first available */
	if (pnvalid != NULL) *pnvalid = nfree;
	if (pFirst  != NULL) *pFirst  = first;

	return camera_count;
}

/* ===========================================================================
=========================================================================== */
#define	TIMER_FRAME_RATE_UPDATE				(1)
#define	TIMER_STATS_UPDATE					(2)
#define	TIMER_FRAMEINFO_UPDATE				(3)

BOOL CALLBACK CameraDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "CameraDlgProc";

	BOOL rcode, bFlag;
	int i, ival, ineed, nfree, ichan, rc, mode;
	CAMERA *nfirst;
	int wID, wNotifyCode;
	char szBuf[256];

	RING_INFO rings;
	TRIGGER_INFO trigger_info;

	double fps, rval;
	POINT point;
	RECT rect;

	/* List of controls which will respond to <ENTER> with a WM_NEXTDLGCTL message */
	/* Make <ENTER> equivalent to losing focus */
	static int DfltEnterList[] = {					
		IDV_EXPOSURE_TIME, IDV_FRAME_RATE, IDV_GAMMA, IDV_CURSOR_X_PIXEL, IDV_CURSOR_Y_PIXEL,
		IDV_RED_GAIN, IDV_GREEN_GAIN, IDV_BLUE_GAIN, IDV_MASTER_GAIN,
		IDV_RING_SIZE, IDV_CURRENT_FRAME, IDV_TRIG_COUNT, IDV_COLOR_CORRECT_FACTOR,
		ID_NULL };

	WND_INFO *wnd;
	HWND hwndTest;
	int *hptr;

	/* List of IP addresses */
	static CB_PTR_LIST ip_list[] = {
		{"Manual",		NULL},
		{"Chess-host",	"128.253.129.71"},
		{"LSA-host",	"128.253.129.74"}
	};

/* Recover the information data associated with this window */
	if (msg != WM_INITDIALOG) {
		wnd = (WND_INFO *) GetWindowLongPtr(hdlg, GWLP_USERDATA);
	}

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			fprintf(stderr, "Initializing ZooCam interface\n"); fflush(stderr);
			DlgCenterWindow(hdlg);

			/* Since may not actually be the call, look up this applications instance */
			hInstance = (HINSTANCE) GetWindowLongPtr(hdlg, GWLP_HINSTANCE);

			sprintf_s(szBuf, sizeof(szBuf), "Version 2.0 [ %s ]", LinkDate);
			SetDlgItemText(hdlg, IDT_COMPILE_VERSION, szBuf);

			/* Immediately register a closeout procedure (since this dialog box may open/close many times */
			atexit(Final_Closeout);

			/* Disable all controls for now */
			EnableDlgItem(hdlg, IDC_CAMERA_LIST, FALSE);				/* Will be enabled when scanning cameras */
			for (i=0; AllCameraControls[i]!=ID_NULL; i++) EnableDlgItem(hdlg, AllCameraControls[i], FALSE);

			/* Create the information block and save it within this dialog.  Initialize critical parameters */
			if ( (wnd = main_wnd) == NULL) {
				wnd = main_wnd = (WND_INFO *) calloc(1, sizeof(WND_INFO));
				wnd->cursor_posn.x = wnd->cursor_posn.y = 0.5;

				/* Immediately mark that no camera is active (may or may not be zero) */
				wnd->Camera.driver = UNKNOWN;

				/* Create the structure for DCX cameras */
				wnd->dcx = (DCX_CAMERA *) calloc(1, sizeof(DCX_CAMERA));

				/* Initialize NUMATO information */
				#ifdef USE_NUMATO
					wnd->numato.port        = NUMATO_COM_PORT;
					wnd->numato.enabled     = FALSE;
					wnd->numato.initialized = TRUE;
				#endif
			}

			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) wnd);
			wnd->hdlg   = hdlg;									/* Have this available for other use */
			ZooCam_main_hdlg = hdlg;							/* Let the outside world know also */
			wnd->thumbnail = GetDlgItem(hdlg, IDC_DISPLAY);

			/* Initialize ring buffer entries */
			SetDlgItemInt(hdlg, IDV_RING_SIZE,     1, FALSE);
			SetDlgItemInt(hdlg, IDT_FRAME_COUNT,   0, FALSE);
			SetDlgItemInt(hdlg, IDT_FRAME_VALID,   0, FALSE);
			SetDlgItemInt(hdlg, IDV_CURRENT_FRAME, 0, FALSE);
#ifndef USE_RINGS													/* Value is default number to use */
			EnableDlgItem(hdlg, IDV_RING_SIZE,  FALSE);
			EnableDlgItem(hdlg, IDB_SAVE_BURST, FALSE);
			EnableDlgItem(hdlg, IDB_NEXT_FRAME, FALSE);
			EnableDlgItem(hdlg, IDB_PREV_FRAME, FALSE);
			EnableDlgItem(hdlg, IDB_RESET_RING, FALSE);
#endif

			/* Now, initialize the rest of the windows (will fill in parts of dcx) */
			InitializeScrollBars(hdlg, wnd);
			InitializeHistogramCurves(hdlg, wnd);
			SetDlgItemCheck(hdlg, IDC_SHOW_INTENSITY, TRUE);
			SetDlgItemCheck(hdlg, IDC_SHOW_RGB, TRUE);
			SetDlgItemCheck(hdlg, IDC_FULL_WIDTH_CURSOR, wnd->cursor_posn.fullwidth);
			SetRadioButton(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS, ExposureList[0].wID);
			SetDlgItemText(hdlg, IDT_MIN_EXPOSURE, ExposureList[0].str_min);
			SetDlgItemText(hdlg, IDT_MID_EXPOSURE, ExposureList[0].str_mid);
			SetDlgItemText(hdlg, IDT_MAX_EXPOSURE, ExposureList[0].str_max);

			/* Autosave parameters */
			SetDlgItemCheck(hdlg, IDC_AUTOSAVE, wnd->autosave.enable);
			EnableDlgItem(hdlg, IDT_AUTOSAVE_FNAME, wnd->autosave.enable);
			if (*wnd->autosave.template  == '\0') strcpy_m(wnd->autosave.template,  sizeof(wnd->autosave.template),  "image");
			if (*wnd->autosave.directory == '\0') strcpy_m(wnd->autosave.directory, sizeof(wnd->autosave.directory), ".");
			if (wnd->autosave.next_index < 0) wnd->autosave.next_index = 0;

			/* Initialize DCx driver and set error reporting mode */
			DCx_Initialize();									/* Safe to call multiple times */
			TL_Initialize();

			/* Set debug mode in each driver */
			DCx_SetDebug(wnd->EnableErrorReports);		/* Internal request for debugging */
			TL_SetDebug(wnd->EnableErrorReports);
			SetDlgItemCheck(hdlg, IDC_ENABLE_DCX_ERRORS, wnd->EnableErrorReports);

			/* Fill in the list of cameras, possibly return one to initialize */
			Fill_Camera_List_Control(hdlg, wnd, &nfree, &nfirst);

			/* If only one possible camera, go ahead and initialize it */
			if (nfree == 1 && nfirst != NULL) {		/* If only one free, then open it */
				ComboBoxSetByPtrValue(hdlg, IDC_CAMERA_LIST, nfirst);
				if (Camera_Open(hdlg, wnd, nfirst) != 0) {
					ComboBoxClearSelection(hdlg, IDC_CAMERA_LIST);		/* Should be "unselect" */
					fprintf(stderr, "Failed to open first camera\n"); fflush(stderr);
				}
			}

#ifdef USE_NUMATO
			if (wnd->numato.on == 0) wnd->numato.on = 1;
			wnd->numato.total = wnd->numato.on + wnd->numato.off;
			wnd->numato.phase = 0;
			ShowDlgItem(hdlg, IDG_LED, TRUE);
			ShowDlgItem(hdlg, IDB_LED_CONFIGURE, TRUE);
#endif

#ifdef USE_KEITHLEY
			ShowDlgItem(hdlg, IDG_LED, TRUE);
			ShowDlgItem(hdlg, IDB_LED_CONFIGURE, TRUE);
#endif

#ifndef USE_FOCUS
			ShowWindow(GetDlgItem(hdlg, IDB_SHARPNESS_DIALOG, FALSE);
#endif

			/* Update the cursor position to initial value (probably 0) */
			SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);

			SetTimer(hdlg, TIMER_FRAME_RATE_UPDATE, 1000, NULL);				/* Redraw at roughtly 1 Hz rate */
			SetTimer(hdlg, TIMER_STATS_UPDATE, 200, NULL);						/* Update stats no more than 5 Hz */
			SetTimer(hdlg, TIMER_FRAMEINFO_UPDATE, 100, NULL);					/* Make them go fast */
			
			/* Initialize the TCP server for remote image requests */
			Init_ZooCam_Server();

			/* Enable a floating window */
			EnableDlgItem(hdlg, IDC_FLOAT, TRUE);

			/* Have a random button on screen for debug ... bottom left of dialog box */
			ShowDlgItem(hdlg, IDB_DEBUG, TRUE);		/* Uncomment for debug work ... can be anything */
			rcode = TRUE; break;

		case WM_CLOSE:
			printf("WM_CLOSE received ..."); fflush(stdout);
			Camera_Close(hdlg, wnd);					/* Close down active camera if any */
			abort_all_threads = TRUE;					/* Tell everyone to disappear */

			/* Give long enought for all WaitFor... objects to timeout and acknowledge shutdown */
			Sleep(1100);

			/* Shutdown the TL system */
			TL_Shutdown();

			wnd->hdlg		  = NULL;					/* Mark this window as invalid so can restart */
			wnd->thumbnail   = NULL;					/* Eliminate the thumbnail options */
			ZooCam_main_hdlg = NULL;					/* And is gone for the outside world */

			FreeCurve(wnd->red_hist);   wnd->red_hist = NULL;
			FreeCurve(wnd->green_hist); wnd->green_hist = NULL;
			FreeCurve(wnd->blue_hist);  wnd->blue_hist = NULL;
			FreeCurve(wnd->vert_w);		 wnd->vert_w = NULL;
			FreeCurve(wnd->vert_r);		 wnd->vert_r = NULL;
			FreeCurve(wnd->vert_g);		 wnd->vert_g = NULL;
			FreeCurve(wnd->vert_b);		 wnd->vert_b = NULL;
			FreeCurve(wnd->vert_sum);	 wnd->vert_sum = NULL;
			FreeCurve(wnd->horz_w);		 wnd->horz_w = NULL;
			FreeCurve(wnd->horz_r);		 wnd->horz_r = NULL;
			FreeCurve(wnd->horz_b);		 wnd->horz_b = NULL;
			FreeCurve(wnd->horz_g);		 wnd->horz_g = NULL;
			FreeCurve(wnd->horz_sum);	 wnd->horz_sum = NULL;

		/* Release resources */
			if (wnd->dcx != NULL) {
				if (wnd->dcx->FrameEvent != NULL) { CloseHandle(wnd->dcx->FrameEvent); wnd->dcx->FrameEvent = NULL; }
#ifdef USE_RINGS
				if (wnd->dcx->SequenceEvent != NULL) { CloseHandle(wnd->dcx->SequenceEvent); wnd->dcx->SequenceEvent = NULL; }
#endif		
			}

			/* Need to release memory associated with the curves */
			printf(" calling EndDialog ..."); fflush(stdout);
			EndDialog(hdlg,0);
			printf(" returning\n"); fflush(stdout);
			rcode = TRUE; break;

		case WM_TIMER:
			switch (wParam) {
				case TIMER_FRAME_RATE_UPDATE:
					fps = Camera_GetFPSActual(wnd);
					SetDlgItemDouble(hdlg, IDT_ACTUALFRAMERATE, "%.2f", fps);
					if (! wnd->has_fps_control) {
						ival = nint(200.0*(fps-wnd->fps_min)/(wnd->fps_max-wnd->fps_min));
						ival = max(0,min(200,ival));
						SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETPOS, TRUE, ival);
					}
					break;

				case TIMER_STATS_UPDATE:
					if (stats.updated) {
						/* Update the color saturation and histogram graphs */
						SetDlgItemDouble(hdlg, IDT_RED_SATURATE,   "%.2f%%", stats.R_sat);
						SetDlgItemDouble(hdlg, IDT_GREEN_SATURATE, "%.2f%%", stats.G_sat);
						SetDlgItemDouble(hdlg, IDT_BLUE_SATURATE,  "%.2f%%", stats.B_sat);
						SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_REDRAW, 0, 0);
						/* Update the calculated centroid position */
						if (wnd->track_centroid) {
							SetDlgItemInt(hdlg, IDV_CURSOR_X_PIXEL, stats.x_centroid, FALSE);
							SetDlgItemInt(hdlg, IDV_CURSOR_Y_PIXEL, stats.y_centroid, FALSE);
						}
						/* Update the horizontal and vertical line scans */
						SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_SCALES, (WPARAM) &stats.horz_scales, (LPARAM) 0);
						SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_REDRAW, 0, 0);
						SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_SCALES, (WPARAM) &stats.vert_scales, (LPARAM) 0);
						SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_REDRAW, 0, 0);
						/* Update the sharpness */
						SetDlgItemInt(hdlg, IDT_SHARPNESS, stats.sharpness, FALSE);
						stats.updated = FALSE;
					}
					break;

					/* Update the ring information */
				case TIMER_FRAMEINFO_UPDATE:
					{
						static RING_INFO ringhold = {-1,-1,-1,-1};
						Camera_GetRingInfo(wnd, &rings);
						if (rings.nBuffers != ringhold.nBuffers) SetDlgItemInt(hdlg, IDV_RING_SIZE,     rings.nBuffers, FALSE);
						if (rings.iLast    != ringhold.iLast)    SetDlgItemInt(hdlg, IDT_FRAME_COUNT,   rings.iLast,    FALSE);
						if (rings.nValid   != ringhold.nValid)   SetDlgItemInt(hdlg, IDT_FRAME_VALID,   rings.nValid,   FALSE);
						if (rings.iShow    != ringhold.iShow)    SetDlgItemInt(hdlg, IDV_CURRENT_FRAME, rings.iShow,    FALSE);
						ringhold = rings;
					}
					break;

				default:
					break;
			}
			rcode = TRUE; break;

		case WM_RBUTTONDOWN:
			GetCursorPos((LPPOINT) &point);
			GetWindowRect(GetDlgItem(hdlg, IDC_DISPLAY), &rect);
			if (point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom) {
				wnd->cursor_posn.x = (1.0*point.x-rect.left) / (rect.right-rect.left);
				wnd->cursor_posn.y = (1.0*point.y-rect.top)  / (rect.bottom-rect.top);
			}
			SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
			break;

		case WM_LBUTTONDBLCLK:								/* Magnify at this location */
		case WM_MBUTTONDOWN:
			break;

/* See dirsync.c for way to change the text color instead of just the background color */
/* See lasgo queue.c for way to change background */
		case WM_CTLCOLORMSGBOX:
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSCROLLBAR:
		case WM_CTLCOLORBTN:						/* Real buttons like OK or CANCEL */
		case WM_CTLCOLORSTATIC:					/* Includes check boxes as well as static */
			break;

		case WM_VSCROLL:
			wID = ID_NULL;							/* Determine unerlying wID and set ichan for set below */
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_MASTER_GAIN)) { wID = IDS_MASTER_GAIN; ichan = M_CHAN; }
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_RED_GAIN))    { wID = IDS_RED_GAIN;    ichan = R_CHAN; }
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_GREEN_GAIN))  { wID = IDS_GREEN_GAIN;	ichan = G_CHAN; }
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_BLUE_GAIN))   { wID = IDS_BLUE_GAIN;	ichan = B_CHAN; }
			if (wID != ID_NULL) {
				static HIRES_TIMER *timer = NULL;
				static double t_last = 0.0;
				int ipos;

				if (timer == NULL) timer = HiResTimerReset(NULL, 0.0);

				ipos = -99999;
				switch (LOWORD(wParam)) {
					case SB_THUMBPOSITION:									/* Moved manually */
						ipos = HIWORD(wParam); break;
					case SB_THUMBTRACK:										/* Limit slider updates to 5 Hz */
						if (HiResTimerDelta(timer)-t_last > 0.2) {	/* Has it been 200 ms? */
							ipos = HIWORD(wParam); 
							t_last = HiResTimerDelta(timer);
						}
						break;
					case SB_LINEDOWN:
					case SB_LINEUP:
					case SB_PAGEDOWN:
					case SB_PAGEUP:
					case SB_BOTTOM:
					case SB_TOP:
					default:
						ipos = (int) SendDlgItemMessage(hdlg, wID, TBM_GETPOS, 0, 0); break;
				}
				if (ipos != -99999 && wnd->Camera.driver != UNKNOWN) {
					Camera_SetGains(wnd, ichan, IS_SLIDER, 0.01*(100-ipos));
					SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
				}
			}
			rcode = TRUE; break;

		case WM_HSCROLL:
			wID = ID_NULL;
			/* Which scroll bar was moved */
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_FRAME_RATE))    wID = IDS_FRAME_RATE;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_EXPOSURE_TIME)) wID = IDS_EXPOSURE_TIME;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_GAMMA))			 wID = IDS_GAMMA;
			if (wID != ID_NULL) {
				static HIRES_TIMER *timer = NULL;
				static double t_last = 0.0;
				int ipos;

				if (timer == NULL) timer = HiResTimerReset(NULL, 0.0);

				ipos = -99999;
				switch (LOWORD(wParam)) {
					case SB_THUMBPOSITION:									/* Moved manually */
						ipos = HIWORD(wParam); break;
					case SB_THUMBTRACK:
						if (HiResTimerDelta(timer)-t_last > 0.2) {	/* Has it been 200 ms? */
							ipos = HIWORD(wParam); 
							t_last = HiResTimerDelta(timer);
						}
						break;
					case SB_LINEDOWN:
					case SB_LINEUP:
					case SB_PAGEDOWN:
					case SB_PAGEUP:
					case SB_BOTTOM:
					case SB_TOP:
					default:
						ipos = (int) SendDlgItemMessage(hdlg, wID, TBM_GETPOS, 0, 0); break;
				}
				if (ipos != -99999) {
					switch (wID) {
						case IDS_FRAME_RATE:
							Camera_SetFPSControl(wnd, wnd->fps_min+ipos/200.0*(wnd->fps_max-wnd->fps_min));			/* Range of the slider */
							SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);		/* May have changed */
							SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);		/* Show actual values */
							break;
						case IDS_EXPOSURE_TIME:										/* Just send the value of the scroll bar */
							i = GetRadioButtonIndex(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS);
							rval = ExposureList[i].exp_min * pow(10.0,ipos/100.0);			/* Scale */
							if (rval > ExposureList[i].exp_max) rval = ExposureList[i].exp_max;
							Camera_SetExposure(wnd, rval);
							break;
						case IDS_GAMMA:
							Camera_SetGamma(wnd, 0.01*ipos);
							SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
							break;
					}
				}
			}
			rcode = TRUE; break;

		case WMP_UPDATE_IMAGE_WITH_CURSOR:
			SetDlgItemInt(hdlg, IDV_CURSOR_X_PIXEL, nint((wnd->cursor_posn.x-0.5)*wnd->width),  TRUE);
			SetDlgItemInt(hdlg, IDV_CURSOR_Y_PIXEL, nint((0.5-wnd->cursor_posn.y)*wnd->height), TRUE);	/* Remember Y is top down */
			Camera_GetRingInfo(wnd, &rings);													/* Which frame is currently dispalyed .. need to re-render */
			Camera_ShowImage(wnd, rings.iShow, NULL);
			rc = TRUE; break;

		case WMP_SHOW_GAMMA:
			rval = Camera_GetGamma(wnd);
			SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETPOS, TRUE, (int) (rval*100.0+0.5));
			SetDlgItemDouble(hdlg, IDV_GAMMA, "%.2f", rval);
			rcode = TRUE; break;

		case WMP_SHOW_COLOR_CORRECT:
			rval = 0;
			if ( (rc = Camera_GetColorCorrection(wnd, &rval)) >= 0) {
				for (i=0; i<N_COLOR_MODES; i++) { if (rc == ColorCorrectionModes[i]) break; }
				if (i >= N_COLOR_MODES) i = 0;
				SetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR, i);
				SetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR, "%.2f", rval);
			}
			rcode = TRUE; break;
				
		case WMP_SHOW_GAINS:
			if (wnd->Camera.driver != UNKNOWN) {
				double values[4], slider[4];
				
				Camera_GetGains(wnd, values, slider);
				if (wnd->Camera.driver == DCX) {									/* Values are integers */
					SetDlgItemInt(hdlg, IDV_MASTER_GAIN, (int) (values[0]+0.5), FALSE);
					SetDlgItemInt(hdlg, IDV_RED_GAIN,    (int) (values[1]+0.5), FALSE);
					SetDlgItemInt(hdlg, IDV_GREEN_GAIN,  (int) (values[2]+0.5), FALSE);
					SetDlgItemInt(hdlg, IDV_BLUE_GAIN,   (int) (values[3]+0.5), FALSE);
				} else if (wnd->Camera.driver == TL) {
					SetDlgItemDouble(hdlg, IDV_MASTER_GAIN, "%.1f", values[0]);
					SetDlgItemDouble(hdlg, IDV_RED_GAIN,    "%.1f", values[1]);
					SetDlgItemDouble(hdlg, IDV_GREEN_GAIN,  "%.1f", values[2]);
					SetDlgItemDouble(hdlg, IDV_BLUE_GAIN,   "%.1f", values[3]);
				}
				SendDlgItemMessage(hdlg, IDS_MASTER_GAIN, TBM_SETPOS, TRUE, 100 - (int) (100.0*slider[0]+0.5));
				SendDlgItemMessage(hdlg, IDS_RED_GAIN,    TBM_SETPOS, TRUE, 100 - (int) (100.0*slider[1]+0.5));
				SendDlgItemMessage(hdlg, IDS_GREEN_GAIN,  TBM_SETPOS, TRUE, 100 - (int) (100.0*slider[2]+0.5));
				SendDlgItemMessage(hdlg, IDS_BLUE_GAIN,   TBM_SETPOS, TRUE, 100 - (int) (100.0*slider[3]+0.5));
			}
			rcode = TRUE; break;

		case WMP_SHOW_FRAMERATE:
			fps = Camera_GetFPSControl(wnd);
			SetDlgItemDouble(hdlg, IDT_FPS_MIN, "%.0f fps", wnd->fps_min);
			SetDlgItemDouble(hdlg, IDT_FPS_MAX, "%.0f fps", wnd->fps_max);
			if (fps > 0.0) {
				ival = nint(200.0*(fps-wnd->fps_min)/(wnd->fps_max-wnd->fps_min));
				ival = max(0,min(200,ival));
				SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETPOS, TRUE, ival);
				SetDlgItemDouble(hdlg, IDV_FRAME_RATE, "%.2f", fps);
			} else {
				/* The IDS_FRAME_RATE control is used to display actual if there is no control */
				if (wnd->has_fps_control) SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETPOS, TRUE, 0);
				SetDlgItemText(hdlg, IDV_FRAME_RATE, "N/A");
			}
			rcode = TRUE; break;

		case WMP_SHOW_EXPOSURE:
			rval = Camera_GetExposure(wnd);
			SetDlgItemDouble(hdlg, IDV_EXPOSURE_TIME, "%.2f", rval);
			i = GetRadioButtonIndex(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS);
			ineed = (i >= 0 && i < N_EXPOSURE_LIST) ? i : 0;
			while (ineed > 0                 && rval < ExposureList[ineed].exp_min) ineed--;
			while (ineed < N_EXPOSURE_LIST-1 && rval > ExposureList[ineed].exp_max) ineed++;
			if (i != ineed) {
				SetRadioButtonIndex(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS, ineed);
				SetDlgItemText(hdlg, IDT_MIN_EXPOSURE, ExposureList[ineed].str_min);
				SetDlgItemText(hdlg, IDT_MID_EXPOSURE, ExposureList[ineed].str_mid);
				SetDlgItemText(hdlg, IDT_MAX_EXPOSURE, ExposureList[ineed].str_max);
			}
			rval = rval/ExposureList[ineed].exp_min;
			i = (int) (100.0*log(rval)/log(10.0)+0.5);		/* 2 decades range in general */
			SendDlgItemMessage(hdlg, IDS_EXPOSURE_TIME, TBM_SETPOS, TRUE, i);
			rcode = TRUE; break;

			/* Enable / disable appropriate radio buttons and control buttons for trigger capabilities and current mode */
		case WMP_UPDATE_TRIGGER_BUTTONS:
			mode = Camera_GetTriggerMode(wnd, &trigger_info);
			if (mode < 0) {
				static int wIDs[] = {IDB_TRIGGER, IDC_ARM,
											IDR_TRIG_FREERUN, IDR_TRIG_SOFTWARE, IDR_TRIG_EXTERNAL, IDR_TRIG_SS, IDR_TRIG_BURST,
											IDR_TRIG_POS, IDR_TRIG_NEG};
				for (i=0; i<sizeof(wIDs)/sizeof(wIDs[0]); i++) EnableDlgItem(hdlg, wIDs[i], FALSE);
				wnd->LiveVideo = FALSE;
				SetDlgItemText(hdlg, IDC_LIVE, "Live");
				SetDlgItemCheck(hdlg, IDC_LIVE, FALSE);
			} else {
				TRIGGER_CAPABILITIES *caps;
				caps = &trigger_info.capabilities;
				EnableDlgItem(hdlg, IDC_LIVE,				caps->bFreerun && mode != TRIG_BURST);
				EnableDlgItem(hdlg, IDR_TRIG_FREERUN,  caps->bFreerun);
				EnableDlgItem(hdlg, IDR_TRIG_SOFTWARE, caps->bSoftware);
				EnableDlgItem(hdlg, IDR_TRIG_EXTERNAL, caps->bExternal);
				EnableDlgItem(hdlg, IDR_TRIG_SS,			caps->bSingleShot);
				EnableDlgItem(hdlg, IDR_TRIG_BURST,		caps->bBurst);
				EnableDlgItem(hdlg, IDB_TRIGGER,       trigger_info.bArmed && ((mode == TRIG_SOFTWARE) || (mode == TRIG_BURST) || ((mode == TRIG_EXTERNAL || mode == TRIG_SS) && caps->bForceExtTrigger)));
				EnableDlgItem(hdlg, IDC_ARM, caps->bArmDisarm && (mode != TRIG_BURST));

				bFlag = (mode == TRIG_EXTERNAL || mode == TRIG_SS) && caps->bExtTrigSlope;
				ShowDlgItem(hdlg, IDR_TRIG_POS, bFlag); EnableDlgItem(hdlg, IDR_TRIG_POS, bFlag);
				ShowDlgItem(hdlg, IDR_TRIG_NEG, bFlag); EnableDlgItem(hdlg, IDR_TRIG_NEG, bFlag);
				SetRadioButton(hdlg, IDR_TRIG_POS, IDR_TRIG_NEG, (trigger_info.ext_slope == TRIG_EXT_NEG) ? IDR_TRIG_NEG : IDR_TRIG_POS);

				bFlag = (mode == TRIG_SOFTWARE || mode == TRIG_EXTERNAL || mode == TRIG_SS) && caps->bMultipleFramesPerTrigger;
				ShowDlgItem(hdlg, IDT_TRIG_COUNT, bFlag); EnableDlgItem(hdlg, IDT_TRIG_COUNT, bFlag);
				ShowDlgItem(hdlg, IDV_TRIG_COUNT, bFlag); EnableDlgItem(hdlg, IDV_TRIG_COUNT, bFlag);
				SetDlgItemInt(hdlg, IDV_TRIG_COUNT, Camera_GetFramesPerTrigger(wnd), FALSE);
				
				bFlag = mode == TRIG_BURST || mode == TRIG_SS;
				ShowDlgItem(hdlg, IDB_BURST_ARM, bFlag );						/* Different meanings, but both real */
				EnableDlgItem(hdlg, IDB_BURST_ARM, bFlag);

				if (mode != TRIG_BURST) {
					SetDlgItemCheck(hdlg, IDC_ARM, trigger_info.bArmed);
					SetDlgItemText(hdlg, IDC_ARM, trigger_info.bArmed ? "Camera armed" : "Arm camera");
				} else {
					SetDlgItemCheck(hdlg, IDC_ARM, FALSE);
					SetDlgItemText(hdlg, IDC_ARM, "Burst: standby");
				}

				/* Set the appropriate radio button corresponding to the mode */
				SetRadioButtonIndex(hdlg, IDR_TRIG_FREERUN, IDR_TRIG_BURST, mode);

				/* Deal with the Live/Paused button */
				if (mode == TRIG_FREERUN) {
					wnd->LiveVideo = trigger_info.bArmed;
					SetDlgItemCheck(hdlg, IDC_LIVE, TRUE);
					SetDlgItemText(hdlg, IDC_LIVE, wnd->LiveVideo ? "Live" : "Paused");
				} else {
					wnd->LiveVideo = FALSE;
					SetDlgItemText(hdlg, IDC_LIVE, "Live");
					SetDlgItemCheck(hdlg, IDC_LIVE, FALSE);
				}
				SetRadioButtonIndex(hdlg, IDR_TRIG_FREERUN, IDR_TRIG_BURST, mode);
				wnd->LiveVideo = (mode == TRIG_FREERUN) && trigger_info.bArmed;
			}
			rcode = 0; break;


		/* Reset dialog controls to reflect that burst mode has been armed (from Burst_Actions()) */
		case WMP_BURST_ARM:
			for (i=0; BurstArmControlsDisable[i] != ID_NULL; i++) EnableDlgItem(hdlg, BurstArmControlsDisable[i], FALSE);
			Camera_GetRingInfo(wnd, &rings);
			SetDlgItemInt(hdlg, IDT_FRAME_COUNT, rings.iLast, FALSE);		/* These all should be zero */
			SetDlgItemInt(hdlg, IDT_FRAME_VALID, rings.nValid, FALSE);
			SetDlgItemInt(hdlg, IDV_CURRENT_FRAME, rings.iShow, FALSE);
			SetDlgItemText(hdlg, IDB_BURST_ARM, "Abort");
			SetDlgItemText(hdlg, IDC_ARM, "Burst: standby");
			rcode = TRUE; break;

		/* Called after a trigger is complete to return to normal */
		case WMP_BURST_COMPLETE:
			for (i=0; BurstArmControlsReenable[i] != ID_NULL; i++) EnableDlgItem(hdlg, BurstArmControlsReenable[i], TRUE);
			SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);
			SetDlgItemText(hdlg, IDB_BURST_ARM, "Arm");
			SetDlgItemText(hdlg, IDC_ARM, "Burst: standby");
			wnd->BurstModeActive = FALSE;
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			rcode = FALSE;												/* Assume we don't process */
			switch (wID) {
				case IDB_DEBUG:										/* Special purpose testing button (normally invisible) */
					rcode = TRUE; break;

				case IDOK:												/* Default response for pressing <ENTER> */
					hwndTest = GetFocus();							/* Just see if we use to change focus */
					for (hptr=DfltEnterList; *hptr!=ID_NULL; hptr++) {
						if (GetDlgItem(hdlg, *hptr) == hwndTest) {
							if (*hptr == IDV_FRAME_RATE) {
								Camera_SetFPSControl(wnd, GetDlgItemDouble(hdlg, IDV_FRAME_RATE));
								SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);		/* May have changed */
								SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);		/* Show actual values */
							} else if (*hptr == IDV_EXPOSURE_TIME) {
								Camera_SetExposure(wnd, GetDlgItemDouble(hdlg, IDV_EXPOSURE_TIME));
							} else if (*hptr == IDV_GAMMA) {
								Camera_SetGamma(wnd, GetDlgItemDouble(hdlg, IDV_GAMMA));
								SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
							} else if (*hptr == IDV_CURRENT_FRAME) {
								SendMessage(hdlg, WM_COMMAND, MAKEWPARAM(IDV_CURRENT_FRAME, EN_KILLFOCUS), 0L);
								SendDlgItemMessage(hdlg, IDV_CURRENT_FRAME, EM_SETSEL, 0, -1);
							} else {
								PostMessage(hdlg, WM_NEXTDLGCTL, 0, 0L);
							}
							break;
						}
					}
					rcode = TRUE; break;

				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				case IDC_ENABLE_DCX_ERRORS:
					wnd->EnableErrorReports = GetDlgItemCheck(hdlg, wID);
					DCx_SetDebug(wnd->EnableErrorReports);		/* Internal request for debugging */
					TL_SetDebug(wnd->EnableErrorReports);
					rcode = TRUE; break;

#ifdef USE_RINGS
				/* Display index is 0 based but display number is 1 based */
				case IDB_NEXT_FRAME:
				case IDB_PREV_FRAME:
				case IDV_CURRENT_FRAME:													/* Can be modified */
					if ( (BN_CLICKED == wNotifyCode && (wID == IDB_NEXT_FRAME || wID == IDB_PREV_FRAME)) ||
						  (EN_KILLFOCUS == wNotifyCode && wID == IDV_CURRENT_FRAME) ) {
						int frame;
						if (wnd->LiveVideo) {
							Beep(300,200);
						} else {
							frame = GetDlgItemIntEx(hdlg, IDV_CURRENT_FRAME);
							if (wID == IDB_NEXT_FRAME) frame++;
							if (wID == IDB_PREV_FRAME) frame--;
							Camera_GetRingInfo(wnd, &rings);
							if (frame < 0) frame = rings.nValid-1;
							if (frame >= rings.nValid) frame = 0;
							SetDlgItemInt(hdlg, IDV_CURRENT_FRAME, frame, FALSE);
							Camera_ShowImage(wnd, frame, NULL);
						}
					}
					rcode = TRUE; break;
#endif
					
				
#ifdef USE_FOCUS
				case IDB_SHARPNESS_DIALOG:
					if (BN_CLICKED == wNotifyCode) _beginthread(show_sharpness_dialog_thread, 0, NULL);
					rcode = TRUE; break;
#endif

				case IDB_AUTO_EXPOSURE:
					if (BN_CLICKED == wNotifyCode) _beginthread(AutoExposureThread, 0, wnd);
					rcode = TRUE; break;
					
				case IDB_RESET_CURSOR:
					if (BN_CLICKED == wNotifyCode) {
						wnd->cursor_posn.x = wnd->cursor_posn.y = 0.5;
						SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
					}
					rcode = TRUE; break;

				case IDC_SHOW_INTENSITY:
					wnd->vert_w->visible = wnd->horz_w->visible = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;

				case IDC_SHOW_RGB:
					wnd->vert_r->visible = wnd->vert_g->visible = wnd->vert_b->visible = GetDlgItemCheck(hdlg, wID);
					wnd->horz_r->visible = wnd->horz_g->visible = wnd->horz_b->visible = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;

				case IDC_SHOW_SUM:
					wnd->vert_sum->visible = GetDlgItemCheck(hdlg, wID);
					wnd->horz_sum->visible = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;

				case IDC_TRACK_CENTROID:
					wnd->track_centroid = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;

				case IDC_FULL_WIDTH_CURSOR:
					wnd->cursor_posn.fullwidth = GetDlgItemCheck(hdlg, wID);
					SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
					rcode = TRUE; break;

				case IDB_CAMERA_DISCONNECT:
					if (BN_CLICKED == wNotifyCode) {

						/* If there is an active camera, close it now */
						Camera_Close(hdlg, wnd);

						/* Disable controls associated with cameras active */
						for (i=0; CameraOffControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOffControls[i], FALSE);
						ComboBoxClearSelection(hdlg, IDC_CAMERA_LIST);		/* Should be "unselect" */
						ComboBoxClearSelection(hdlg, IDC_CAMERA_MODES);		/* Should be "unselect" */
						EnableDlgItem(hdlg, IDB_CAMERA_DETAILS, FALSE);
						EnableDlgItem(hdlg, IDB_CAMERA_DISCONNECT, FALSE);
					}
					rcode = TRUE; break;
					
				case IDB_ROI:
					if ( wnd != NULL && wnd->Camera.driver == TL && wnd->Camera.details != NULL) {
						if (DialogBoxParam(hInstance, "IDD_ROI", HWND_DESKTOP, (DLGPROC) ROIDlgProc, (LPARAM) wnd) == IDOK) {
							TL_CAMERA *tl;
							tl = (TL_CAMERA *) wnd->Camera.details;		
							/* Update the information display */
							sprintf_s(szBuf, sizeof(szBuf), "%dx%d @ (%d,%d)", tl->width, tl->height, tl->roi.dx, tl->roi.dy);
							SetDlgItemText(hdlg, IDT_ROI_INFO, szBuf);

						}
					} else {
						Beep(300,200);
					}
					rcode = TRUE; break;
					
				case IDB_CAMERA_DETAILS:
					if (BN_CLICKED == wNotifyCode) _beginthread(Camera_Info_Thread, 0, (void *) wnd);
					rcode = TRUE; break;

				case IDC_CAMERA_LIST:
					if (CBN_SELENDOK == wNotifyCode) {
						CAMERA *camera;
						camera = (CAMERA *) ComboBoxGetPtrValue(hdlg, wID);

						/* Close any currently active camera and disable controls */
						Camera_Close(hdlg, wnd);
						for (i=0; CameraOffControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOffControls[i], FALSE);

						/* Open the new camera */
						if (Camera_Open(hdlg, wnd, camera) == 0) { 
							for (i=0; CameraOnControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOnControls[i], TRUE);
						} else {
							fprintf(stderr, "Unable to open requested camera\n"); fflush(stderr);
							ComboBoxClearSelection(hdlg, wID);
						}
					}
					rcode = TRUE; break;

				case IDC_CAMERA_MODES:
					if (CBN_SELENDOK == wNotifyCode && wnd->Camera.driver == DCX) {
						if (wnd->dcx->hCam > 0) {
							DCx_SetTriggerMode(wnd->dcx, TRIG_SOFTWARE, NULL);
							wnd->LiveVideo = FALSE;
							SetDlgItemCheck(hdlg, IDC_LIVE, FALSE);
							EnableDlgItem(hdlg, IDB_TRIGGER, FALSE);
							is_StopLiveVideo(wnd->dcx->hCam, IS_WAIT);

							if (Set_DCx_Resolution(hdlg, wnd, ComboBoxGetIntValue(hdlg, wID)) == 0) {
								DCx_SetTriggerMode(wnd->dcx, TRIG_FREERUN, NULL);
								SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);
								wnd->LiveVideo = TRUE;
								SetDlgItemCheck(hdlg, IDC_LIVE, wnd->LiveVideo);
								SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
							} else {
								ComboBoxClearSelection(hdlg, IDC_CAMERA_MODES);
							}
						}
					}
					rcode = TRUE; break;

				case IDC_FLOAT:
					if (BN_CLICKED == wNotifyCode) {
						if (GetDlgItemCheck(hdlg, wID)) {
							if (! IsWindow(float_image_hwnd)) _beginthread(start_image_window, 0, NULL);
						} else {
							if (IsWindow(float_image_hwnd)) SendMessage(float_image_hwnd, WM_CLOSE, 0, 0);
						}
					}
					rcode = TRUE; break;

				case IDB_LOAD_PARAMETERS:
					if (BN_CLICKED == wNotifyCode) {
						if (wnd->Camera.driver == DCX) {
							DCx_LoadParameterFile(wnd->dcx, NULL);
							SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);
							SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
							SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
							SendMessage(hdlg, WMP_SHOW_COLOR_CORRECT, 0, 0);
							SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
						}
					}
					rcode = TRUE; break;

				case IDB_SAVE_PARAMETERS:
					if (BN_CLICKED == wNotifyCode) {
						if (wnd->Camera.driver == DCX) {
							DCx_SaveParameterFile(wnd->dcx, NULL);
						}
					}
					rcode = TRUE; break;

				case IDV_FRAME_RATE:
					if (EN_KILLFOCUS == wNotifyCode) {
						Camera_SetFPSControl(wnd, GetDlgItemDouble(hdlg, wID));
						SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);		/* May have changed */
						SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);		/* Show actual values */
					}
					rcode = TRUE; break;
					
				case IDV_EXPOSURE_TIME:
					if (EN_KILLFOCUS == wNotifyCode) Camera_SetExposure(wnd, GetDlgItemDouble(hdlg, wID));
					rcode = TRUE; break;

				case IDV_GAMMA:
					if (EN_KILLFOCUS == wNotifyCode) {
						Camera_SetGamma(wnd, (double) GetDlgItemDouble(hdlg, wID));
						SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
					}
					rcode = TRUE; break;

				case IDB_GAMMA_NEUTRAL:
					if (BN_CLICKED == wNotifyCode) {
						Camera_SetGamma(wnd, 1.0);
						SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
					}
					rcode = TRUE; break;

				case IDR_EXPOSURE_100US:
				case IDR_EXPOSURE_1MS:
				case IDR_EXPOSURE_10MS:
				case IDR_EXPOSURE_100MS:
					i = GetRadioButtonIndex(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS);
					SetDlgItemText(hdlg, IDT_MIN_EXPOSURE, ExposureList[i].str_min);
					SetDlgItemText(hdlg, IDT_MID_EXPOSURE, ExposureList[i].str_mid);
					SetDlgItemText(hdlg, IDT_MAX_EXPOSURE, ExposureList[i].str_max);
					rval = Camera_GetExposure(wnd);
					if (rval < ExposureList[i].exp_min) {
						rval = Camera_SetExposure(wnd, 1.1*ExposureList[i].exp_min);	/* Try to make sure will be in range */
					} else if (rval > ExposureList[i].exp_max) {
						rval = Camera_SetExposure(wnd, 0.9*ExposureList[i].exp_max);	/* Try to make sure will be in range */
					} else {
						SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
					}
					fflush(stderr);
					rcode = TRUE; break;

				/* Only time I get a message is when it is clicked, so just take for granted */
				case IDR_COLOR_DISABLE:
				case IDR_COLOR_ENABLE:
				case IDR_COLOR_BG40:
				case IDR_COLOR_HQ:
				case IDR_COLOR_AUTO_IR:
					i = GetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR);
					i = max(0, min(i, NUM_COLOR_CORRECT_MODES));
					rval = GetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR);
					if ( (rc = Camera_SetColorCorrection(wnd, ColorCorrectionModes[i], rval)) >= 0) {
						/* Mark the one that actually is returned */
						for (i=0; i<N_COLOR_MODES; i++) { if (rc == ColorCorrectionModes[i]) break; }
						if (i >= N_COLOR_MODES) i = 0;
						SetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR, i);
					}
					rcode = TRUE; break;

				case IDV_COLOR_CORRECT_FACTOR:
					if (EN_KILLFOCUS == wNotifyCode) {
						static double last_value = -10.0;
						rval = GetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR);
						if (rval < 0.0) rval = 0.0;
						if (rval > 1.0) rval = 1.0;
						SetDlgItemDouble(hdlg, wID, "%.2f", rval);
						if (rval != last_value) {
							last_value = rval;
							i = GetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR);
							i = max(0, min(i, NUM_COLOR_CORRECT_MODES));
							rval = GetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR);
							if ( (rc = Camera_SetColorCorrection(wnd, ColorCorrectionModes[i], rval)) >= 0) {
								/* Mark the one that actually is returned */
								for (i=0; i<N_COLOR_MODES; i++) { if (rc == ColorCorrectionModes[i]) break; }
								if (i >= N_COLOR_MODES) i = 0;
								SetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR, i);
							}
						}
					}
					rcode = TRUE; break;
					
				case IDV_MASTER_GAIN:
				case IDV_RED_GAIN:
				case IDV_GREEN_GAIN:
				case IDV_BLUE_GAIN:
					if (EN_KILLFOCUS == wNotifyCode) {
						rval = GetDlgItemDouble(hdlg, wID);
						if (wID == IDV_MASTER_GAIN) ichan = M_CHAN;
						if (wID == IDV_RED_GAIN)    ichan = R_CHAN;
						if (wID == IDV_GREEN_GAIN)  ichan = G_CHAN;
						if (wID == IDV_BLUE_GAIN)   ichan = B_CHAN;
						Camera_SetGains(wnd, ichan, IS_VALUE, rval);			/* Set by value */
						SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
					}
					rcode = TRUE; break;

				case IDB_RESET_GAINS:
					Camera_ResetGains(wnd);
					SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
					rcode = TRUE; break;
					
				case IDV_CURSOR_X_PIXEL:
					if (EN_KILLFOCUS == wNotifyCode) {
						rval = ((double) GetDlgItemIntEx(hdlg, wID)) / wnd->width + 0.5;
						wnd->cursor_posn.x = (rval < 0.0) ? 0.0 : (rval > 1.0) ? 1.0 : rval ;
						SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
					}
					rcode = TRUE; break;
				case IDV_CURSOR_Y_PIXEL:
					if (EN_KILLFOCUS == wNotifyCode) {
						rval = 0.5 - ((double) GetDlgItemIntEx(hdlg, wID)) / wnd->height;
						wnd->cursor_posn.y = (rval < 0.0) ? 0.0 : (rval > 1.0) ? 1.0 : rval ;
						SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);
					}
					rcode = TRUE; break;

				case IDV_RING_SIZE:
#ifdef USE_RINGS
					if (EN_KILLFOCUS == wNotifyCode && wnd->Camera.driver != UNKNOWN) {

						/* Pause rendering and give 100 ms for anything going to complete */
						wnd->PauseImageRendering = TRUE; Sleep(100);

						/* Try to change # of buffers, and update with real */
						ineed = Camera_SetRingBufferSize(wnd, GetDlgItemIntEx(hdlg, wID));
						SetDlgItemInt(hdlg, wID, ineed, FALSE);

						/* Sleep a moment and then restart image rendering */
						Sleep(200); wnd->PauseImageRendering = FALSE;
					}
#endif
					rcode = TRUE; break;

				/* Reset the ring counters so next frame is zero */
				case IDB_RESET_RING:
					Camera_ResetRingCounters(wnd);
					rcode = TRUE; break;

				/** Complex operations 
					-- If LiveVideo (freerun and armed), go to TRIG_SOFTWARE and uncheck
					-- If freerun but not armed, just arm and go back to LiveVideo
					-- If not in TRIG_FREERUN mode, set new trigger mode
				**/
				case IDC_LIVE:
					if (BN_CLICKED == wNotifyCode) {
						if (wnd->LiveVideo) {
							Camera_SetTriggerMode(wnd, TRIG_SOFTWARE, NULL);	/* Go to software mode */
						} else if (Camera_GetTriggerMode(wnd, NULL) == TRIG_FREERUN) {
							Camera_Arm(wnd, TRIG_ARM);
						} else {
							Camera_SetTriggerMode(wnd, TRIG_FREERUN, NULL);		/* Go to freerun mode */
						}
						SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);	/* Will set click mode of button */
					}
					rcode = TRUE; break;

				/* Enable or abort based on current status ... */
				/* WMP_BURST_ABORT or WMP_BURST_ARM message will be sent by Burst_Actions() */
				case IDB_BURST_ARM:
					if (BN_CLICKED == wNotifyCode) {
						mode = Camera_GetTriggerMode(wnd, &trigger_info);
						if (TRIG_BURST == mode) {
							Burst_Actions(wnd->BurstModeActive ? BURST_ABORT : BURST_ARM, 0, NULL);
						} else if (TRIG_SS == mode && ! trigger_info.bArmed) {
							Camera_Arm(wnd, TRIG_ARM);
							SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);	/* Will set click mode of button */
						}
					}
					rcode = TRUE; break;

					/* Button like ... down ==> armed, up ==> disarmed ... change state on click */
				case IDC_ARM:
					bFlag = GetDlgItemCheck(hdlg, wID);
					Camera_Arm(wnd, bFlag ? TRIG_ARM : TRIG_DISARM);    
					SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);
					rcode = TRUE; break;
					
				case IDB_TRIGGER:
					if (BN_CLICKED == wNotifyCode) Camera_Trigger(wnd);
					rcode = TRUE; break;

				case IDR_TRIG_FREERUN:
				case IDR_TRIG_SOFTWARE:
				case IDR_TRIG_EXTERNAL:
				case IDR_TRIG_SS:
				case IDR_TRIG_BURST:
					rc = GetRadioButtonIndex(hdlg, IDR_TRIG_FREERUN, IDR_TRIG_BURST);
					Camera_SetTriggerMode(wnd, rc, NULL);
					SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);
					rcode = TRUE; break;

				case IDR_TRIG_POS:
				case IDR_TRIG_NEG:
					Camera_GetTriggerMode(wnd, &trigger_info);
					trigger_info.ext_slope = GetDlgItemCheck(hdlg, IDR_TRIG_POS) ? TRIG_EXT_POS : TRIG_EXT_NEG ;
					rcode = TRUE; break;
					
				case IDV_TRIG_COUNT:
					if (EN_KILLFOCUS == wNotifyCode) 
						rc = Camera_SetFramesPerTrigger(wnd, GetDlgItemIntEx(hdlg, IDV_TRIG_COUNT));
					rcode = TRUE; break;
					
				case IDB_SAVE:
//					if (BN_CLICKED == wNotifyCode) Camera_QuerySaveImage(wnd, GetDfltImageFormat(hdlg));
					if (BN_CLICKED == wNotifyCode) Camera_SaveImage(wnd, -1, NULL, GetDfltImageFormat(hdlg));
					rcode = TRUE; break;

				case IDB_SAVE_BURST:
					if (BN_CLICKED == wNotifyCode) Camera_SaveAll(wnd, NULL, GetDfltImageFormat(hdlg));
					rcode = TRUE; break;

				case IDC_AUTOSAVE:
					if (BN_CLICKED == wNotifyCode) {
						if (! GetDlgItemCheck(hdlg, wID)) {
							wnd->autosave.enable = FALSE;
						} else {
							rc = DialogBoxParam(hInstance, "IDD_AUTOSAVE", HWND_DESKTOP, (DLGPROC) AutoSaveInfoDlgProc, (LPARAM) wnd);
							if ( ! (wnd->autosave.enable = (rc == IDOK)) ) SetDlgItemCheck(hdlg, wID, FALSE);
						}
						EnableDlgItem(hdlg, IDT_AUTOSAVE_FNAME, wnd->autosave.enable);
					}
					rcode = TRUE; break;
								
				/* Process the LED control values */
				case IDB_LED_CONFIGURE:
					if (BN_CLICKED == wNotifyCode) {
#ifdef USE_KEITHLEY
						DialogBox(hInstance, "IDD_KEITHLEY_224", hdlg, (DLGPROC) Keith224DlgProc);
#elif USE_NUMATO
						DialogBox(hInstance, "IDD_LED_CONTROL", (HWND) arglist, (DLGPROC) NUMATODlgProc);
#endif
					}
					ShowDlgItem(hdlg, IDB_LED_ON,  TRUE);
					ShowDlgItem(hdlg, IDB_LED_OFF, TRUE);
					rcode = TRUE; break;

				case IDB_LED_ON:
				case IDB_LED_OFF:
					if (BN_CLICKED == wNotifyCode) {
#ifdef USE_KEITHLEY
						Keith224_Output(wID == IDB_LED_ON ? 1 : 0);
#elif USE_NUMATO
						Beep(300,200);
#endif
					}
					rcode = TRUE; break;

				/* Intentionally unused IDs */
				case IDR_IMAGE_BMP:
				case IDR_IMAGE_RAW:
				case IDR_IMAGE_JPG:
				case IDR_IMAGE_PNG:
				case IDT_RED_SATURATE:
				case IDT_GREEN_SATURATE:
				case IDT_BLUE_SATURATE:
				case IDT_ACTUALFRAMERATE:
				case IDT_FRAME_COUNT:
				case IDT_FRAME_VALID:
				case IDT_SHARPNESS:
				case IDT_ROI_INFO:
					break;

				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return rcode;
}

/* ===========================================================================
BOOL CALLBACK AutoSaveInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
=========================================================================== */
BOOL CALLBACK AutoSaveInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "AutoSaveInfoDlgProc";

	WND_INFO *wnd;
	char szTmp[PATH_MAX];
	int wID, wNotifyCode, rcode;
	OPENFILENAME ofn;
	char pathname[PATH_MAX];

	/* Save the directory identified for future calls */
	static char directory[PATH_MAX] = "";

/* Recover the information data associated with this window */
	if (msg != WM_INITDIALOG) {
		wnd = (WND_INFO *) GetWindowLongPtr(hdlg, GWLP_USERDATA);
	}

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			/* Recover the window pointer and save for this control */
			wnd = (WND_INFO *) lParam;											/* Recover pointer */
			if (wnd == NULL) wnd = main_wnd;
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) wnd);

			/* Center the window for working */
			DlgCenterWindowEx(hdlg, wnd->hdlg);
			
			/* Fill in initial values */
			SetDlgItemText(hdlg, IDV_TEMPLATE, wnd->autosave.template);
			SetDlgItemText(hdlg, IDV_DIRECTORY, wnd->autosave.directory);
			SetDlgItemInt(hdlg, IDV_NEXT_INDEX, wnd->autosave.next_index, FALSE);
			rcode = TRUE; break;

		case WM_CLOSE:
			GetDlgItemText(hdlg, IDV_TEMPLATE,  wnd->autosave.template,  sizeof(wnd->autosave.template));
			GetDlgItemText(hdlg, IDV_DIRECTORY, wnd->autosave.directory, sizeof(wnd->autosave.directory));
			wnd->autosave.next_index = GetDlgItemIntEx(hdlg, IDV_NEXT_INDEX);
			EndDialog(hdlg, IDOK);
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			switch (wID) {
				case IDCANCEL:
					EndDialog(hdlg, wID);
					rcode = TRUE; break;

				case IDOK:												/* Default response for pressing <ENTER> */
					SendMessage(hdlg, WM_CLOSE, 0, 0);			/* Handle above with X out */
					rcode = TRUE; break;

				case IDV_TEMPLATE:
					if (EN_KILLFOCUS == wNotifyCode) {
						GetDlgItemText(hdlg, wID, szTmp, sizeof(szTmp));
						if (_stricmp(szTmp, wnd->autosave.template) != 0) SetDlgItemInt(hdlg, IDV_NEXT_INDEX, 0, FALSE);
					}
					rcode = TRUE; break;

				case IDB_BROWSE:
					if (BN_CLICKED == wNotifyCode) {

						strcpy_m(pathname, sizeof(pathname), "dummy");	/* Pathname must be initialized with a value */
						ofn.lStructSize       = sizeof(OPENFILENAME);
						ofn.hwndOwner         = hdlg;
						ofn.lpstrTitle        = "Choose directory via dummy file";
						ofn.lpstrFilter       = "raw (*.raw)\0*.raw\0All files (*.*)\0*.*\0\0";
						ofn.lpstrCustomFilter = NULL;
						ofn.nMaxCustFilter    = 0;
						ofn.nFilterIndex      = 1;
						ofn.lpstrFile         = pathname;				/* Full path */
						ofn.nMaxFile          = sizeof(pathname);
						ofn.lpstrFileTitle    = NULL;						/* Partial path */
						ofn.nMaxFileTitle     = 0;
						ofn.lpstrDefExt       = "raw";
						ofn.lpstrInitialDir   = (*directory == '\0') ? NULL : directory;
						ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

						/* Query a filename ... if abandoned, just return now with no complaints */
						if (GetSaveFileName(&ofn)) {
							pathname[ofn.nFileOffset-1] = '\0';
							SetDlgItemText(hdlg, IDV_DIRECTORY, pathname);
							strcpy_m(directory, sizeof(directory), pathname);
						}
					}
					rcode = TRUE; break;

				case IDV_DIRECTORY:
				case IDV_NEXT_INDEX:
				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return 0;
}


/* ===========================================================================
BOOL CALLBACK ROIDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
=========================================================================== */
#define	WMP_SET_KNOWN			(WM_APP+1)
#define	WMP_UPDATE_LIMITS		(WM_APP+2)

BOOL CALLBACK ROIDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "ROIDlgProc";

	WND_INFO *wnd;
	int i, width, height, ixoff, iyoff, wID, wNotifyCode, rcode;
	int ulx, uly, lrx, lry, min_width, min_height;
	char szBuf[256];
	TL_CAMERA *tl;

	static int DfltEnterList[] = {					
		IDV_WIDTH, IDV_HEIGHT, IDV_X_OFFSET, IDV_Y_OFFSET,
		ID_NULL
	};
	HWND hwndTest;
	int *hptr;

	/* Recover the information data associated with this window */
	if (msg != WM_INITDIALOG) {
		wnd = (WND_INFO *) GetWindowLongPtr(hdlg, GWLP_USERDATA);
		tl = (wnd != NULL) ? (TL_CAMERA *) wnd->Camera.details : NULL ;
	}

	/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			/* Recover the window pointer and save for this control */
			wnd = (WND_INFO *) lParam;											/* Recover pointer */
			if (wnd == NULL) wnd = main_wnd;
			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) wnd);

			/* Recover the camera information */
			tl = (wnd != NULL) ? (TL_CAMERA *) wnd->Camera.details : NULL ;

			/* Center the window for working */
			DlgCenterWindowEx(hdlg, wnd->hdlg);

			/* Fill in initial values */
			if (tl != NULL) {
				SetDlgItemInt(hdlg, IDV_WIDTH,    tl->width,  FALSE);
				SetDlgItemInt(hdlg, IDV_HEIGHT,   tl->height, FALSE);
				SetDlgItemInt(hdlg, IDV_X_OFFSET, tl->roi.dx, TRUE);
				SetDlgItemInt(hdlg, IDV_Y_OFFSET, tl->roi.dy, TRUE);
				SendMessage(hdlg, WMP_UPDATE_LIMITS, 0, 0);
			}
			rcode = TRUE; break;

		case WM_CLOSE:
			EndDialog(hdlg, IDCANCEL);
			rcode = TRUE; break;

		/* Set one of the default value sets */
		case WMP_SET_KNOWN:
			static struct { 
				int width, height;				/* Pixel start and width/height (will be centered) */
			} roi[] = {
				{ 1920, 1200 },		/* Full frame */
				{ 1600, 1200 },		/* UXGA   */
				{ 1280, 1024 },		/* SXGA   */
				{ 1024,  768 },		/* XGA    */
				{  800,  600 },		/* SVGA   */
				{  640,  480 },		/* VGA    */
				{  400,  300 },		/* QSVGA  */
				{  320,  240 },		/* QVGA   */
				{  160,  120 },		/* QQVGA  */
				{   92,   60 }			/* Tiny   */
			};
#define	N_ROI	(sizeof(roi)/sizeof(*roi))
				i = max(0, min(N_ROI-1, (int) lParam));
				SetDlgItemInt(hdlg, IDV_WIDTH, roi[i].width, FALSE);
				SetDlgItemInt(hdlg, IDV_HEIGHT, roi[i].height, FALSE);
				SetDlgItemInt(hdlg, IDV_X_OFFSET, 0, TRUE);
				SetDlgItemInt(hdlg, IDV_Y_OFFSET, 0, TRUE);
				SendMessage(hdlg, WMP_UPDATE_LIMITS, 0, 0);
				rcode = TRUE; break;

		case WMP_UPDATE_LIMITS:
			min_width  = tl->roi.lr_min.x-tl->roi.ul_min.x;		/* minimums set smallest size */
			min_height = tl->roi.lr_min.y-tl->roi.ul_min.y;

			/* Get and validate the width */
			width = GetDlgItemIntEx(hdlg, IDV_WIDTH);
			if (width < min_width || width > tl->sensor_width) {
				width  = max(min_width,  min(tl->sensor_width,  width));
				SetDlgItemInt(hdlg, IDV_WIDTH, width, FALSE);
			}

			/* Get and validate the height */
			height = GetDlgItemIntEx(hdlg, IDV_HEIGHT);
			if (height < min_height || height > tl->sensor_height) {
				height = max(min_height, min(tl->sensor_height, GetDlgItemIntEx(hdlg, IDV_HEIGHT)));
				SetDlgItemInt(hdlg, IDV_HEIGHT, height, FALSE);
			}

			/* Fill in text limits for offset */
			sprintf_s(szBuf, sizeof(szBuf), "minimum %d", min_width);
			SetDlgItemText(hdlg, IDS_XMIN, szBuf);
			sprintf_s(szBuf, sizeof(szBuf), "minimum %d", min_height);
			SetDlgItemText(hdlg, IDS_YMIN, szBuf);
			sprintf_s(szBuf, sizeof(szBuf), "%d < x < %d", -(tl->sensor_width-width)/2, (tl->sensor_width-width)/2);
			SetDlgItemText(hdlg, IDS_XRANGE, szBuf);
			sprintf_s(szBuf, sizeof(szBuf), "%d < y < %d", -(tl->sensor_height-height)/2, (tl->sensor_height-height)/2);
			SetDlgItemText(hdlg, IDS_YRANGE, szBuf);

			/* Validate the offsets */
			ixoff = GetDlgItemIntEx(hdlg, IDV_X_OFFSET);
			if (abs(ixoff) > (tl->sensor_width-width)/2) {
				ixoff = (ixoff < 0) ? -(tl->sensor_width-width)/2 : (tl->sensor_width-width)/2 ;
				SetDlgItemInt(hdlg, IDV_X_OFFSET, ixoff, TRUE);
			}
			iyoff = GetDlgItemIntEx(hdlg, IDV_Y_OFFSET);
			if (abs(iyoff) > (tl->sensor_height-height)/2) {
				iyoff = (iyoff < 0) ? -(tl->sensor_height-height)/2 : (tl->sensor_height-height)/2 ;
				SetDlgItemInt(hdlg, IDV_Y_OFFSET, iyoff, TRUE);
			}
			rcode = TRUE; break;
				
		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			switch (wID) {
				case IDOK:
					hwndTest = GetFocus();							/* Just see if we use to change focus */
					for (hptr=DfltEnterList; *hptr!=ID_NULL; hptr++) {
						if (GetDlgItem(hdlg, *hptr) == hwndTest) {
							PostMessage(hdlg, WM_NEXTDLGCTL, 0, 0L);
							break;
						}
					}
					rcode = 0; break;

				case IDCANCEL:
					EndDialog(hdlg, wID);
					rcode = TRUE; break;

				case ID_SETROI:										/* OK button, but not ENTER */
					width  = GetDlgItemIntEx(hdlg, IDV_WIDTH);
					height = GetDlgItemIntEx(hdlg, IDV_HEIGHT);
					ixoff  = GetDlgItemIntEx(hdlg, IDV_X_OFFSET);
					iyoff  = GetDlgItemIntEx(hdlg, IDV_Y_OFFSET);
					ulx = (tl->sensor_width  - width)  / 2 + ixoff;
					uly = (tl->sensor_height - height) / 2 + iyoff;
					lrx = ulx + width  - 1;
					lry = uly + height - 1;
					TL_SetROI(tl, ulx, uly, lrx, lry);
					EndDialog(hdlg, IDOK);
					rcode = TRUE; break;

				/* The UPDATE_LIMITS will verify all values */
				case IDV_WIDTH:
				case IDV_HEIGHT:
				case IDV_X_OFFSET:
				case IDV_Y_OFFSET:
					if (EN_KILLFOCUS == wNotifyCode) SendMessage(hdlg, WMP_UPDATE_LIMITS, 0, 0);
					rcode = TRUE; break;
					
				/* Set the offset so it will center the current crosshair position */
				case IDB_CENTER_CROSSHAIR:
					ixoff = (int) (tl->roi.dx + (wnd->cursor_posn.x-0.5)*tl->width  + 0.5);
					iyoff = (int) (tl->roi.dy + (wnd->cursor_posn.y-0.5)*tl->height + 0.5);
					SetDlgItemInt(hdlg, IDV_X_OFFSET, ixoff, TRUE);
					SetDlgItemInt(hdlg, IDV_Y_OFFSET, iyoff, TRUE);
					SendMessage(hdlg, WMP_UPDATE_LIMITS, 0, 0);
					rcode = TRUE; break;

				case IDB_FULL_FRAME:
				case IDB_RES_1:
				case IDB_RES_2:
				case IDB_RES_3:
				case IDB_RES_4:
				case IDB_RES_5:
				case IDB_RES_6:
				case IDB_RES_7:
				case IDB_RES_8:
				case IDB_RES_9:
					SendMessage(hdlg, WMP_SET_KNOWN, 0, wID-IDB_FULL_FRAME); rcode = TRUE; break;
					rcode = TRUE; break;
					
				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return 0;
}


#ifdef USE_NUMATO

/* ===========================================================================
=========================================================================== */
BOOL CALLBACK NUMATODlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "NUMATODlgProc";

	WND_INFO *wnd;
	int wID, wNotifyCode, rcode;

	/* Copy the source of all information */
	wnd = main_wnd;

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			SetDlgItemInt(hdlg, IDV_COM_PORT, wnd->numato.port, FALSE);
			SetDlgItemCheck(hdlg, IDC_ENABLE, wnd->numato.enabled);
			SetDlgItemInt(hdlg, IDV_LED_ON, wnd->numato.on, FALSE);
			SetDlgItemInt(hdlg, IDV_LED_OFF, wnd->numato.off, FALSE);
			SetRadioButtonIndex(hdlg, IDR_LED_OFF, IDR_LED_TOGGLE, wnd->numato.mode);
			rcode = TRUE; break;

		case WM_CLOSE:
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				case IDV_COM_PORT:
					if (EN_KILLFOCUS == wNotifyCode) {
						if (GetDlgItemIntEx(hdlg, wID) != wnd->numato.port) {
							if (wnd->numato.dio != NULL) {
								NumatoQueryBit(wnd->numato.dio, 0, NULL);
								NumatoCloseDIO(wnd->numato.dio);
							}
							wnd->numato.dio = NULL;
							wnd->numato.port = GetDlgItemIntEx(hdlg, wID);
							SetDlgItemCheck(hdlg, IDC_ENABLE, FALSE);
							wnd->numato.enabled = FALSE;
						}
					}
					rcode = TRUE; break;

				case IDC_ENABLE:
					if (wnd->numato.enabled = GetDlgItemCheck(hdlg, wID)) {
						if ( (wnd->numato.dio = NumatoOpenDIO(wnd->numato.port, FALSE)) == NULL ) {
							SetDlgItemCheck(hdlg, IDC_ENABLE, FALSE);
							wnd->numato.enabled = FALSE;
						} else if (wnd->numato.mode != DIO_TOGGLE) {
							NumatoSetBit(wnd->numato.dio, 0, wnd->numato.mode == DIO_ON);
						}
					} else if (wnd->numato.dio != NULL) {
						NumatoQueryBit(wnd->numato.dio, 0, NULL);
						NumatoCloseDIO(wnd->numato.dio);
						wnd->numato.dio = NULL;
					}
					rcode = TRUE; break;
					
				case IDV_LED_ON:
				case IDV_LED_OFF:
					if (EN_KILLFOCUS == wNotifyCode) {
						wnd->numato.on  = GetDlgItemIntEx(hdlg, IDV_LED_ON);
						wnd->numato.off = GetDlgItemIntEx(hdlg, IDV_LED_OFF);
						if (wnd->numato.on <= 0) {
							wnd->numato.on = 1;
							SetDlgItemInt(hdlg, IDV_LED_ON, 1, FALSE);
						}
						wnd->numato.total = wnd->numato.on + wnd->numato.off;
						wnd->numato.phase = 0;
					}
					rcode = TRUE; break;

				case IDR_LED_OFF:
				case IDR_LED_ON:
				case IDR_LED_TOGGLE:
					wnd->numato.mode = GetRadioButtonIndex(hdlg, IDR_LED_OFF, IDR_LED_TOGGLE);
					if (wnd->numato.enabled && wnd->numato.mode != DIO_TOGGLE) {
						NumatoSetBit(wnd->numato.dio, 0, wnd->numato.mode == DIO_ON);
					}
					rcode = TRUE; break;

				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return 0;
}

#endif

/* ===========================================================================
-- Dialog box procedure for working with the focus client/server
--
-- Usage: BOOL CALLBACK SharpnessDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
--
-- Standard windows dialog
=========================================================================== */
#ifdef USE_FOCUS
static void show_sharpness_dialog_thread(void *arglist) {
	static BOOL running = FALSE;
	if (! running) {
		running = TRUE;
		DialogBox(hInstance, "IDD_FOCUS_MONITOR", HWND_DESKTOP, (DLGPROC) SharpnessDlgProc);
		running = FALSE;
	}
	return;
}

#define	TIMER_FOCUS_GRAPH_REDRAW	1

BOOL CALLBACK SharpnessDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "SharpnessDlgProc";

	WND_INFO *wnd;
	int rc, wID, wNotifyCode, rcode;
	GRAPH_CURVE *cv;
	double zposn;

	/* Copy the source of all information */
	wnd = main_wnd;

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			DlgCenterWindowEx(hdlg, main_wnd->hdlg);
			SharpnessDlg.hdlg = hdlg;											/* Okay, we are live now */
			SharpnessDlg.mode = FOCUS_SWEEP;
			SetRadioButtonIndex(hdlg, IDR_TIME_SEQUENCE, IDR_FOCUS_SWEEP, SharpnessDlg.mode);
			SetDlgItemCheck(hdlg, IDC_PAUSE, SharpnessDlg.paused);
			EnableDlgItem(hdlg, IDB_SET_EST_FOCUS, FALSE);

			if (SharpnessDlg.cv == NULL) {
				cv = calloc(sizeof(GRAPH_CURVE), 1);
				cv->ID = 1;											/* reference curve */
				strcpy_m(cv->legend, sizeof(cv->legend), "focus");
				cv->master        = TRUE;
				cv->visible       = TRUE;
				cv->free_on_clear = FALSE;
				cv->draw_x_axis   = cv->draw_y_axis   = TRUE;
				cv->force_scale_x = cv->force_scale_y = FALSE;
				cv->autoscale_x   = TRUE;	cv->autoscale_y = FALSE;
				cv->npt = 0;
				cv->nptmax = 1280;									/* Normally large enough, but will expand when set */
				cv->isize = 2;
				cv->x = calloc(sizeof(*cv->x), cv->nptmax);
				cv->y = calloc(sizeof(*cv->y), cv->nptmax);
				cv->rgb = RGB(150,255,150);
				SharpnessDlg.cv = cv;
			}
			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_CLEAR, (WPARAM) 0, (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_X_TITLE, (WPARAM) (SharpnessDlg.mode == TIME_SEQUENCE ? "frame" : "Z position [mm]"), (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_Y_TITLE, (WPARAM) "sharpness [counts/pixel]", (LPARAM) 0);
			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_ADD_CURVE, (WPARAM) SharpnessDlg.cv,  (LPARAM) 0);
//			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_LABEL_VISIBILITY, 0, 0);
//			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_TITLE_VISIBILITY, 0, 0);
//			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_NO_MARGINS, 1, 0);
//			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_BACKGROUND_COLOR, RGB(0,0,64), 0);
//			memset(&parms, 0, sizeof(parms));
//			parms.suppress_grid = parms.suppress_ticks = TRUE;
//			SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_AXIS_PARMS, (WPARAM) &parms, (LPARAM) 0);

			if (SharpnessDlg.focus == NULL) {
				cv = calloc(sizeof(GRAPH_CURVE), 1);
				cv->ID = 2;											/* reference curve */
				strcpy_m(cv->legend, sizeof(cv->legend), "best focus");
				cv->master        = FALSE;
				cv->visible       = TRUE;
				cv->free_on_clear = FALSE;
				cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
				cv->force_scale_x = cv->force_scale_y = FALSE;
				cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
				cv->npt = 0;
				cv->nptmax = 100;									/* Normally large enough, but will expand when set */
				cv->x = calloc(sizeof(*cv->x), cv->nptmax);
				cv->y = calloc(sizeof(*cv->y), cv->nptmax);
				cv->rgb = RGB(128,128,255);
				SharpnessDlg.focus = cv;
			}
			SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_ADD_CURVE, (WPARAM) SharpnessDlg.focus,  (LPARAM) 0);

			SetTimer(hdlg, TIMER_FOCUS_GRAPH_REDRAW, 200, NULL);									/* Redraw at max 5 Hz rate */
			rcode = TRUE; break;

		case WM_CLOSE:
			SharpnessDlg.hdlg = NULL;											/* Okay, we are now alive */
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_TIMER:
			if (wParam == TIMER_FOCUS_GRAPH_REDRAW && SharpnessDlg.cv != NULL && SharpnessDlg.cv->modified) {
				int i;
				double x0,y0,ymin,ymax,s0,sxy,sy;

				cv = SharpnessDlg.cv;
				if (SharpnessDlg.mode == FOCUS_SWEEP && cv->npt > 10) {
					ymin = ymax = cv->y[0];
					for (i=0; i<cv->npt; i++) {
						if (cv->y[i] > ymax) ymax = cv->y[i];
						if (cv->y[i] < ymin) ymin = cv->y[i];
					}
					x0 = cv->x[cv->npt/2];
					y0 = ymin + 0.25*(ymax-ymin);								/* Only include y points 25% baseline */
					s0 = sxy = sy = 0.0;
					for (i=0; i<cv->npt; i++) {
						if (cv->y[i] > y0) {
							s0  += 1.0;
							sy  += pow(cv->y[i],2);
							sxy += pow(cv->y[i],2)*(cv->x[i]-x0);
						}
					}
					x0 += (sy > 0) ? sxy/sy : 0;
					cv = SharpnessDlg.focus;
					cv->npt = cv->nptmax;
					for (i=0; i<cv->npt; i++) {
						cv->x[i] = x0;
						cv->y[i] = ymin + (ymax-ymin)*i/(cv->npt-1.0);
					}
					cv->modified = TRUE;
					SetDlgItemDouble(hdlg, IDT_EST_FOCUS, "%.3f", x0);
					EnableDlgItem(hdlg, IDB_SET_EST_FOCUS, TRUE);
				}
				SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_REDRAW, 0L, 0L);
			}
			rcode = TRUE; break;
			
		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				case IDR_TIME_SEQUENCE:
				case IDR_FOCUS_SWEEP:
					rc = GetRadioButtonIndex(hdlg, IDR_TIME_SEQUENCE, IDR_FOCUS_SWEEP);
					if (rc != SharpnessDlg.mode) {
						SharpnessDlg.mode = rc;
						SendDlgItemMessage(hdlg, IDG_FOCUS_GRAPH, WMP_SET_X_TITLE, (WPARAM) (SharpnessDlg.mode == TIME_SEQUENCE ? "frame" : "Z position [mm]"), (LPARAM) 0);
						if (SharpnessDlg.cv != NULL) {
							SharpnessDlg.cv->npt = 0;
							SharpnessDlg.focus->npt = 0;
							SharpnessDlg.cv->modified = TRUE;
						}
						EnableDlgItem(hdlg, IDB_SET_EST_FOCUS, FALSE);
						SetDlgItemText(hdlg, IDT_EST_FOCUS, "");
					}
					rcode = TRUE; break;

				case IDC_PAUSE:
					SharpnessDlg.paused = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;

				case IDB_CLEAR_FOCUS_GRAPH:
					if (BN_CLICKED == wNotifyCode) {
						if (SharpnessDlg.cv != NULL) {
							SharpnessDlg.cv->npt = 0;
							SharpnessDlg.focus->npt = 0;
							SharpnessDlg.cv->modified = TRUE;
						}
						EnableDlgItem(hdlg, IDB_SET_EST_FOCUS, FALSE);
						SetDlgItemText(hdlg, IDT_EST_FOCUS, "");
					}
					rcode = TRUE; break;

				case IDB_SET_EST_FOCUS:
					if (BN_CLICKED == wNotifyCode) {
						zposn = GetDlgItemDouble(hdlg, IDT_EST_FOCUS);
						if (zposn != 0) {
							Focus_Remote_Set_Focus_Posn(zposn, TRUE);
						} else {
							Beep(300,200);
						}
					}
					rcode = TRUE; break;

				/* Known unused items */
				case IDT_EST_FOCUS:
					rcode = TRUE; break;
					
				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return 0;
}


/* ===========================================================================
-- Handle updates to the focus dislog box from frame updates
--
-- Usage: int Update_Focus_Dialog(int sharpness);
--
-- Inputs: sharpness - delta_max from region around cursor in new frame
--
-- Output: potentially updates the graph window if open
--
-- Return: 0 on success
--           1 ==> SharpnessDlg.mode unknown
--           2 ==> no remote focus client and too soon to try checking again
--           3 ==> unable to connect to focus client on given IP address
--           4 ==> focus client/server versions don't match
--				 5 ==> unable to get status from focus server
--				 6
=========================================================================== */
int Update_Focus_Dialog(int sharpness) {

	int rc;
	int client_version, server_version;
	double zposn;
	int status;
	GRAPH_CURVE *cv;
	static BOOL InSweep = FALSE;

	/* Connection to focus client */
	static BOOL Have_Focus_Client = FALSE;
	static char *server_IP = LOOPBACK_SERVER_IP_ADDRESS;		/* Local test (server on same machine) */
//	static char *server_IP = "128.253.129.74";					/* Machine in laser room */
//	static char *server_IP = "128.253.129.71";					/* Machine in open lab room */

	static time_t time_last_check = 0;

	/* Do we have remote and the sharpness dialog up? */
	if (SharpnessDlg.hdlg == NULL || SharpnessDlg.paused || SharpnessDlg.cv == NULL) return 0;
	cv = SharpnessDlg.cv;

	/* Make sure there is space for additional points */
	if (cv->npt >= cv->nptmax) {
		cv->nptmax += 1024;
		cv->x = realloc(cv->x, sizeof(*cv->x)*cv->nptmax);
		cv->y = realloc(cv->y, sizeof(*cv->y)*cv->nptmax);
	}

	/* Time sequence operations do not depend on the client/server interaction.  Handle now */
	if (SharpnessDlg.mode == TIME_SEQUENCE) {
		cv->x[cv->npt] = cv->npt ;
		cv->y[cv->npt] = sharpness;
		cv->npt++;
		cv->modified = TRUE;
		return 0;
	}

	/* Only understand FOCUS_SWEEP and TIME_SEQUENCE for modes */
	if (SharpnessDlg.mode != FOCUS_SWEEP) return 1;

	/* Need client for FOCUS_SWEEP */
	if (! Have_Focus_Client) {

		/* Only try every 10 seconds to reconnect */
		if (time(NULL) < time_last_check+10) { InSweep = FALSE; return 2; }
		time_last_check = time(NULL);

		/* Try to connect */
		if ( (rc = Init_Focus_Client(server_IP)) != 0) {
			fprintf(stderr, "ERROR: Unable to connect to the server at the specified IP address (%s)\n", server_IP); fflush(stderr);
			InSweep = FALSE; return 3;
		}

		/* Verify versions */
		client_version = Focus_Remote_Query_Client_Version();
		server_version = Focus_Remote_Query_Server_Version();
		printf("Client/Server versions: %4.4d/%4.4d\n", client_version, server_version); fflush(stderr);
		if (client_version != server_version) {
			fprintf(stderr, "ERROR: Version mismatch between focus client and server.\n"); fflush(stderr);
			InSweep = FALSE; return 4;
		}

		Have_Focus_Client = TRUE;
	}

	/* Query status of the focus position */
	if ( (rc = Focus_Remote_Get_Focus_Status(&status)) != 0) {
		fprintf(stderr, "ERROR: Looks like we lost the remote focus client.  Will retry every 10 seconds.  (rc=%d)\n", rc); fflush(stderr);
		Have_Focus_Client = FALSE;
		InSweep = FALSE; return 5;
	}

	if (status & FM_MOTOR_STATUS_SWEEP) {
		Focus_Remote_Get_Focus_Posn(&zposn);
		if (! InSweep) { cv->npt = 0; InSweep = TRUE; }
		cv->x[cv->npt] = zposn;
		cv->y[cv->npt] = sharpness;
		cv->npt++;
		cv->modified = TRUE;
	} else {
		InSweep = FALSE;
	}

	return 0;
}
#endif


/* ===========================================================================
-- Main callback routine for DCx camera info
=========================================================================== */
BOOL CALLBACK DCX_CameraInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "DCX_CameraInfoDlgProc";

	WND_INFO *wnd;
	int rc, wID, wNotifyCode, rcode;
	char szTmp[256];
	CAMINFO camInfo;
	SENSORINFO SensorInfo;
	UINT nRange[3], nPixelClock, newPixelClock;

	/* Copy the source of all information */
	wnd = main_wnd;
	
/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			DlgCenterWindowEx(hdlg, main_wnd->hdlg);
			rc = is_GetCameraInfo(wnd->dcx->hCam, &camInfo);
			if (rc == IS_SUCCESS) {
				SetDlgItemText(hdlg, IDT_CAMERA_SERIAL_NO,		camInfo.SerNo);
				SetDlgItemText(hdlg, IDT_CAMERA_MANUFACTURER,	camInfo.ID);
				SetDlgItemText(hdlg, IDT_CAMERA_VERSION,			camInfo.Version);
				SetDlgItemText(hdlg, IDT_CAMERA_DATE,				camInfo.Date);
				SetDlgItemInt(hdlg,  IDT_CAMERA_ID, camInfo.Select, FALSE);
				// camInfo.Type == IS_CAMERA_TYPE_UC480_USB_SE ? "IS_CAMERA_TYPE_UC480_USB_SE" : camInfo.Type == IS_CAMERA_TYPE_UC480_USB_LE ? "IS_CAMERA_TYPE_UC480_USB_LE" : camInfo.Type == IS_CAMERA_TYPE_UC480_USB3_CP ? "IS_CAMERA_TYPE_UC480_USB3_CP" : "Unknown");
			}

			rc = is_GetSensorInfo(wnd->dcx->hCam, &SensorInfo);
			if (rc == 0) {
				SetDlgItemText(hdlg, IDT_CAMERA_MODEL,      SensorInfo.strSensorName);
				SetDlgItemText(hdlg, IDT_CAMERA_COLOR_MODE, SensorInfo.nColorMode == IS_COLORMODE_BAYER ? "Bayer" : SensorInfo.nColorMode == IS_COLORMODE_MONOCHROME ? "Monochrome" : "Unknown");
				sprintf_s(szTmp, sizeof(szTmp), "%d x %d", SensorInfo.nMaxWidth, SensorInfo.nMaxHeight);
				SetDlgItemText(hdlg, IDT_CAMERA_IMAGE_SIZE, szTmp);
				SetDlgItemInt(hdlg, IDT_CAMERA_PIXEL_PITCH, SensorInfo.wPixelSize, FALSE);
			}

			/* Get pixel clock data */
			rc = is_PixelClock(wnd->dcx->hCam, IS_PIXELCLOCK_CMD_GET_RANGE, (void *) &nRange, sizeof(nRange));
			if (rc == 0) {
				SetDlgItemInt(hdlg, IDT_PIXEL_CLOCK_MIN, nRange[0], FALSE);
				SetDlgItemInt(hdlg, IDT_PIXEL_CLOCK_MAX, nRange[1], FALSE);
				SetDlgItemInt(hdlg, IDT_PIXEL_CLOCK_INC, nRange[2], FALSE);
			}
			rc = is_PixelClock(wnd->dcx->hCam, IS_PIXELCLOCK_CMD_GET, (void *) &nPixelClock, sizeof(nPixelClock));
			if (rc == 0) SetDlgItemInt(hdlg, IDT_PIXEL_CLOCK_CURRENT, nPixelClock, FALSE);
			rc = is_PixelClock(wnd->dcx->hCam, IS_PIXELCLOCK_CMD_GET_DEFAULT, (void *) &nPixelClock, sizeof(nPixelClock));
			if (rc == 0) SetDlgItemInt(hdlg, IDT_PIXEL_CLOCK_DEFAULT, nPixelClock, FALSE);

			rcode = TRUE; break;

		case WM_CLOSE:
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				case IDB_PIXEL_CLOCK_SET:
					if (BN_CLICKED == wNotifyCode) {
						newPixelClock = GetDlgItemIntEx(hdlg, IDV_PIXEL_CLOCK_SET);
						is_PixelClock(wnd->dcx->hCam, IS_PIXELCLOCK_CMD_GET_RANGE, (void *) &nRange, sizeof(nRange));
						if (newPixelClock < nRange[0]) newPixelClock = nRange[0];
						if (newPixelClock > nRange[1]) newPixelClock = nRange[1];
						SetDlgItemInt(hdlg, IDV_PIXEL_CLOCK_SET, newPixelClock, FALSE);
						is_PixelClock(wnd->dcx->hCam, IS_PIXELCLOCK_CMD_SET, (void *) &newPixelClock, sizeof(newPixelClock));
						is_PixelClock(wnd->dcx->hCam, IS_PIXELCLOCK_CMD_GET, (void *) &nPixelClock, sizeof(nPixelClock));
						SetDlgItemInt(hdlg, IDT_PIXEL_CLOCK_CURRENT, nPixelClock, FALSE);
					}
					rcode = TRUE; break;

				/* Intentionally unused IDs */
				case IDV_PIXEL_CLOCK_SET:
				case IDT_PIXEL_CLOCK_MIN:
				case IDT_PIXEL_CLOCK_MAX:
				case IDT_PIXEL_CLOCK_INC:
				case IDT_PIXEL_CLOCK_CURRENT:
				case IDT_PIXEL_CLOCK_DEFAULT:
				case IDT_CAMERA_MANUFACTURER:
				case IDT_CAMERA_MODEL:
				case IDT_CAMERA_SERIAL_NO:
				case IDT_CAMERA_ID:
				case IDT_CAMERA_IMAGE_SIZE:
				case IDT_CAMERA_PIXEL_PITCH:
				case IDT_CAMERA_VERSION:
				case IDT_CAMERA_DATE:
				case IDT_CAMERA_COLOR_MODE:
					rcode = TRUE; break;
					
				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return 0;
}

/* ===========================================================================
-- Callback routine for TL camera information dialog
=========================================================================== */
BOOL CALLBACK TL_CameraInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "TL_CameraInfoDlgProc";

	WND_INFO *wnd;
	TL_CAMERA *camera;
	int wID, wNotifyCode, rcode;
	double db_min, db_max;
	char szTmp[256];

	/* Copy the source of all information */
	wnd = main_wnd;
	camera = (TL_CAMERA *) wnd->Camera.details;

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			DlgCenterWindowEx(hdlg, main_wnd->hdlg);
			if (camera != NULL && camera->magic == TL_CAMERA_MAGIC) {
				SetDlgItemText(hdlg, IDT_CAMERA_ID,             camera->ID);
				SetDlgItemText(hdlg, IDT_CAMERA_NAME,           camera->name);
				SetDlgItemText(hdlg, IDT_CAMERA_SERIAL_NO,		camera->serial);
				SetDlgItemText(hdlg, IDT_CAMERA_MODEL,          camera->model);
				SetDlgItemText(hdlg, IDT_CAMERA_FIRMWARE,			camera->firmware);

				sprintf_s(szTmp, sizeof(szTmp), "%d x %d",      camera->sensor_width, camera->sensor_height);
				SetDlgItemText(hdlg, IDT_CAMERA_SENSOR_SIZE, szTmp);
				sprintf_s(szTmp, sizeof(szTmp), "%.2f x %.2f",  camera->pixel_width_um, camera->pixel_height_um);
				SetDlgItemText(hdlg, IDT_CAMERA_PIXEL_PITCH, szTmp);

				SetDlgItemInt(hdlg, IDT_CAMERA_BIT_DEPTH, camera->bit_depth, TRUE);

				sprintf_s(szTmp, sizeof(szTmp), "%d x %d",      camera->width, camera->height);
				SetDlgItemText(hdlg, IDT_CAMERA_IMAGE_SIZE, szTmp);

				SetDlgItemDouble(hdlg, IDT_CAMERA_EXPOSE_MIN, "%.3f", 0.001*camera->us_expose_min);
				SetDlgItemDouble(hdlg, IDT_CAMERA_EXPOSE_MAX, "%.0f", 0.001*camera->us_expose_max);

				TL_GetMasterGainInfo(camera, NULL, NULL, &db_min, &db_max);
				SetDlgItemDouble(hdlg, IDT_CAMERA_GAIN_MIN, "%.1f", db_min);
				SetDlgItemDouble(hdlg, IDT_CAMERA_GAIN_MAX, "%.1f", db_max);

				SetDlgItemDouble(hdlg, IDT_CAMERA_FPS_MIN, "%.2f", camera->fps_min);
				SetDlgItemDouble(hdlg, IDT_CAMERA_FPS_MAX, "%.2f", camera->fps_max);

				SetDlgItemText(hdlg, IDT_CAMERA_COLOR_MODE,
									(camera->sensor_type == TL_CAMERA_SENSOR_TYPE_BAYER) ? "color (Bayer)" :
									(camera->sensor_type == TL_CAMERA_SENSOR_TYPE_MONOCHROME) ? "monochrome" :
									(camera->sensor_type == TL_CAMERA_SENSOR_TYPE_MONOCHROME_POLARIZED) ? "b/w polarized" :
									"unknown");
			}
			rcode = TRUE; break;

		case WM_CLOSE:
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				/* Intentionally unused IDs */
				case IDT_CAMERA_ID:
				case IDT_CAMERA_NAME:
				case IDT_CAMERA_SERIAL_NO:
				case IDT_CAMERA_MODEL:
				case IDT_CAMERA_IMAGE_SIZE:
				case IDT_CAMERA_SENSOR_SIZE:
				case IDT_CAMERA_PIXEL_PITCH:
				case IDT_CAMERA_BIT_DEPTH:
				case IDT_CAMERA_EXPOSE_MIN:
				case IDT_CAMERA_EXPOSE_MAX:
				case IDT_CAMERA_GAIN_MIN:
				case IDT_CAMERA_GAIN_MAX:
				case IDT_CAMERA_FPS_MIN:
				case IDT_CAMERA_FPS_MAX:
				case IDT_CAMERA_COLOR_MODE:
					rcode = TRUE; break;

				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return 0;
}

/* ===========================================================================
=========================================================================== */
#ifdef	STANDALONE

int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	FILE *fdebug;

	/* If not done, make sure we are loaded.  Assume safe to call multiply */
	InitCommonControls();
	LoadLibrary("RICHED20.DLL");

	if ( (fopen_s(&fdebug, "c:/lsa/ZooCam.log", "a")) != 0) {
		MessageBox(NULL, "Failed to open the debug log file.  No debug data will be collected", "ZooCam.log open fail", MB_OK | MB_ICONERROR);
		fdebug = NULL;
	} else {
		TL_SetDebugLog(fdebug);
	}

	/* Load the class for the graph window */
	Graph_StartUp(hThisInstance);					/* Initialize the graphics control */

//	_beginthread(test_thread, 0, NULL);
	
	/* And show the dialog box */
	hInstance = hThisInstance;
	DialogBox(hInstance, "ZOOCAM_DIALOG", HWND_DESKTOP, (DLGPROC) CameraDlgProc);

	/* Shut down internet server service */
	Shutdown_ZooCam_Server();
	
	/* Cloase all cameras and shutdown the TL interface */
	TL_Shutdown();						/* Shutdown would do the close also, but be clean */

	return 0;
}
#endif

/* ===========================================================================
-- Called from INITDIALOG to start up the graph window
=========================================================================== */
void FreeCurve(GRAPH_CURVE *cv) {
	if (cv != NULL) {
		if (cv->x != NULL) free(cv->x);
		if (cv->y != NULL) free(cv->y);
		free(cv);
	}
	return;
}

int InitializeHistogramCurves(HWND hdlg, WND_INFO *wnd) {

	int i, npt;
	GRAPH_CURVE *cv;
	GRAPH_SCALES scales;
	GRAPH_AXIS_PARMS parms;

	npt = 256;

	/* Initialize the curves for histograms */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 0;											/* Main curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "red");
	cv->master        = TRUE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->nptmax = cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(255,0,0);
	wnd->red_hist = cv;

	/* Initialize the green histogram */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 1;											/* dark curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "green");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->nptmax = cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(0,255,0);
	wnd->green_hist = cv;

	/* Initialize the blue histogram */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* blue curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "blue");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->nptmax = cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(128,128,255);
	wnd->blue_hist = cv;

	/* Set up the scales ... have to modify Y each time but X is infrequent */
	memset(&scales, 0, sizeof(scales));
	scales.xmin = 0;	scales.xmax = npt;
	scales.ymin = 0;  scales.ymax = 10000;
	scales.autoscale_x = FALSE; scales.force_scale_x = TRUE;   
	scales.autoscale_y = TRUE; scales.force_scale_y = FALSE;
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_SCALES, (WPARAM) &scales, (LPARAM) 0);

	/* Turn off the y lines */
	memset(&parms, 0, sizeof(parms));
	parms.suppress_y_grid = parms.suppress_y_ticks = TRUE;
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_AXIS_PARMS, (WPARAM) &parms, (LPARAM) 0);

	/* Clear the graph, and set the force parameters */
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_CLEAR, (WPARAM) 0, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_X_TITLE, (WPARAM) "counts in pixel", (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_Y_TITLE, (WPARAM) "number", (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_SET_LABEL_VISIBILITY, 0, 0);

	/* Set up the horizontal profile */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 1;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row scan");
	cv->master        = TRUE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1920;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(255,255,255);
	wnd->horz_w = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row red scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1920;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,0,0);
	wnd->horz_r = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 3;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row green scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1920;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(0,255,0);
	wnd->horz_g = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 4;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row blue scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1920;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(128,128,255);
	wnd->horz_b = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 5;											/* sum curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "sum scan");
	cv->master        = FALSE;
	cv->visible       = FALSE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1920;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,255,0);
	wnd->horz_sum = cv;

	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_CLEAR, (WPARAM) 0, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_LABEL_VISIBILITY, 0, 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_TITLE_VISIBILITY, 0, 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_NO_MARGINS, 1, 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_BACKGROUND_COLOR, RGB(0,0,64), 0);
	memset(&parms, 0, sizeof(parms));
	parms.suppress_grid = parms.suppress_ticks = TRUE;
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_AXIS_PARMS, (WPARAM) &parms, (LPARAM) 0);

	/* Set up the vertical profiles */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 1;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col scan");
	cv->master        = TRUE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1600;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,255,255);
	wnd->vert_w = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col red scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1600;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,0,0);
	wnd->vert_r = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 3;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col green scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1600;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(0,255,0);
	wnd->vert_g = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 4;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col blue scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1600;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(128,128,255);
	wnd->vert_b = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 5;											/* sum curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "sum scan");
	cv->master        = FALSE;
	cv->visible       = FALSE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->nptmax = cv->npt = 1600;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,255,0);
	wnd->vert_sum = cv;

	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_CLEAR, (WPARAM) 0, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_LABEL_VISIBILITY, 0, 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_TITLE_VISIBILITY, 0, 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_NO_MARGINS, 1, 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_BACKGROUND_COLOR, RGB(0,0,64), 0);
	memset(&parms, 0, sizeof(parms));
	parms.suppress_grid = parms.suppress_ticks = TRUE;
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_AXIS_PARMS, (WPARAM) &parms, (LPARAM) 0);

	/* Add all the curves now; only cv_raw should show initially */
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) wnd->red_hist,    (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) wnd->green_hist,  (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) wnd->blue_hist,   (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->horz_w,   (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->horz_r, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->horz_g, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->horz_b, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->horz_sum, (LPARAM) 0);

	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->vert_w,   (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->vert_r, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->vert_g, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->vert_b, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) wnd->vert_sum, (LPARAM) 0);

	return 0;
}

/* ===========================================================================
=========================================================================== */
int InitializeScrollBars(HWND hdlg, WND_INFO *wnd) {
	int i, wID;

	/* Set up the scroll bar for the frame rate */
	SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETRANGE, FALSE, MAKELPARAM(0,200));			/* In about 0.1 Hz increments */
	SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_CLEARTICS, FALSE, 0);
	SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETTICFREQ, 10, 0);
	SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETLINESIZE, 0, 10);
	SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETPAGESIZE, 0, 20);

	/* Set up the scroll bar for the exposure time */
	SendDlgItemMessage(hdlg, IDS_EXPOSURE_TIME, TBM_SETRANGE, FALSE, MAKELPARAM(1,200));		/* 2 decades, 100 per decade */
	SendDlgItemMessage(hdlg, IDS_EXPOSURE_TIME, TBM_CLEARTICS, FALSE, 0);
	SendDlgItemMessage(hdlg, IDS_EXPOSURE_TIME, TBM_SETTICFREQ, 20, 0);
	SendDlgItemMessage(hdlg, IDS_EXPOSURE_TIME, TBM_SETLINESIZE, 0, 1);
	SendDlgItemMessage(hdlg, IDS_EXPOSURE_TIME, TBM_SETPAGESIZE, 0, 5);

	/* Set up the scroll bar for gamma */
	SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETRANGE, FALSE, MAKELPARAM(1,1000));		/* In 1 ms increments */
	SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_CLEARTICS, FALSE, 0);
	SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETTICFREQ, 100, 0);
	SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETLINESIZE, 0,  2);
	SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETPAGESIZE, 0, 10);

	/* Set up the scroll bars for gains */
	for (i=0; i<4; i++) {
		static int wID_list[] = {IDV_MASTER_GAIN, IDV_RED_GAIN, IDV_GREEN_GAIN, IDV_BLUE_GAIN};
		wID = wID_list[i];
		SendDlgItemMessage(hdlg, wID, TBM_SETRANGE, FALSE, MAKELPARAM(1,100));		/* Range allowed */
		SendDlgItemMessage(hdlg, wID, TBM_CLEARTICS, FALSE, 0);
		SendDlgItemMessage(hdlg, wID, TBM_SETTICFREQ, 10, 0);
		SendDlgItemMessage(hdlg, wID, TBM_SETLINESIZE, 0, 2);
		SendDlgItemMessage(hdlg, wID, TBM_SETPAGESIZE, 0, 5);
	}

	return 0;
}


/* ===========================================================================
-- Start a thread to run the dialog box for the camera
--
-- Usage: void ZooCam_Start_Dialog(void *arglist);
--
-- Inputs: arglist - passed to WM_INITDIALOG as lParam (currently unused)
--
-- Output: simply launches the dialog box
--
-- Return: none
=========================================================================== */
void ZooCam_Start_Dialog(void *arglist) {

	DialogBoxParam(NULL, "ZOOCAM_DIALOG", HWND_DESKTOP, (DLGPROC) CameraDlgProc, (LPARAM) arglist);

	return;
}

/* ===========================================================================
-- Display dialog box with camera information
--
-- Usage: void Camera_Info_Thread(void *arglist);
--
-- Inputs: arglist - pointer to valid WND_INFO *wnd structure
--
-- Output: displays a dialog box with the camera information
--
-- Return: none
=========================================================================== */
static void Camera_Info_Thread(void *arglist) {
	WND_INFO *wnd;
	static BOOL running = FALSE;

	wnd = (WND_INFO *) arglist;
	if (! running) {
		running = TRUE;
		switch (wnd->Camera.driver) {
			case DCX:
				DialogBox(hInstance, "DCX_CAMERA_INFO", HWND_DESKTOP, (DLGPROC) DCX_CameraInfoDlgProc);
				break;
			case TL:
				DialogBox(hInstance, "TL_CAMERA_INFO", HWND_DESKTOP, (DLGPROC) TL_CameraInfoDlgProc);
				break;
			default:
				break;
		}
		running = FALSE;
	}
	return;
}

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
--         msTimeout - timeout for some operations (<=1000 ms)
--         response - pointer to for return code (beyond success)
--
-- Output: *response - action dependent return codes if ! NULL
--         Sets internal parameters for the burst mode capture
--         Sends message to mainhdlg (if !NULL) to modify controls
--
-- Return: 0 if successful, 1 if burst mode is unavailable, 2 other errors
--
-- *response codes
--     BURST_STATUS: status byte
--				(0) BURST_STATUS_INIT				Initial value on program start ... no request ever received
--				(1) BURST_STATUS_ARM_REQUEST		An arm request received ... but thread not yet running
--				(2) BURST_STATUS_ARMED				Armed and awaiting a stripe start message
--				(3) BURST_STATUS_RUNNING			In stripe run
--				(4) BURST_STATUS_COMPLETE			Stripe completed with images saved
--				(5) BURST_STATUS_ABORT				Capture was aborted
--				(6) BURST_STATUS_FAIL				Capture failed for other reason (no semaphores, etc.)
--		BURST_ARM:		0 if successful (or if already armed)
--		BURST_ABORT:	0 if successful (or wasn't armed)
--		BURST_WAIT:		0 if complete, 1 on timeout
=========================================================================== */
int Burst_Actions(BURST_ACTION request, int msTimeout, int *response) {
	static char *rname = "Burst_Actions";

	WND_INFO *wnd;
	HWND hdlg;
	int rc;

	/* Set response code to -1 to indicate major error */
	if (response == NULL) response = &rc;			/* So don't have to check */
	*response = -1;

	/* Must have been started at some point to be able to do anything */
	if (main_wnd == NULL) return 1;
	wnd  = main_wnd;
	hdlg = wnd->hdlg;
	if (hdlg != NULL && ! IsWindow(hdlg)) hdlg = NULL;		/* Mark hdlg if not window */

	/* If trigger is not set for TRIG_BURST mode, just beep and return error */ 
	if (Camera_GetTriggerMode(wnd, NULL) != TRIG_BURST) {
		Beep(200,300);
		*response = -1;
		return 1;
	}

	switch (request) {
		case BURST_STATUS:
			*response = wnd->BurstModeStatus;
			break;

		case BURST_ARM:
			SendMessage(hdlg, WMP_BURST_ARM, 0, 0);
			wnd->BurstModeActive = TRUE;
			wnd->BurstModeStatus = BURST_STATUS_ARM_REQUEST;	/* Not active */

			_beginthread(trigger_burst_mode, 0, wnd);
			Sleep(30);														/* Time for thread to start running */
			*response = 0;	
			break;
			
		case BURST_ABORT:
			if (wnd->BurstModeActive) {
				SetDlgItemText(hdlg, IDC_ARM, "Burst: aborting");
				wnd->BurstModeActive = FALSE;							/* Mark for abort */
				Sleep(600);													/* Timeout within the thread */
			}
			*response = 0;	break;

		case BURST_WAIT:
			if (wnd->BurstModeActive) {
				if (msTimeout <= 0) msTimeout = 1000;					/* Maximum 1 minute wait */
				if (msTimeout > 1000) msTimeout = 1000;				/* Also don't allow requests for more than 1 second */
				*response = 1;													/* Indicate timeout before done */
				while (msTimeout > 0) {
					if (! wnd->BurstModeActive) { *response = 0; break; }
					Sleep(min(100, msTimeout));							/* Wait in 100 ms blocks */
					msTimeout -= 100;											/* May go negative but who cares */
				}
			} else {
				*response = 0;
			}
			break;
			
		default:
			return 2;
	}

	return 0;
}


/* ===========================================================================
-- Query the preferred image storage format from radio button
--
-- Usage: FILE_FORMAT GetDfltImageFormat(HWND hdlg);
--
-- Inputs: hdlg - pointer to the dialog box with controls
--
-- Output: none
--
-- Return: Image format to use as default.  FILE_BMP returned as default.
=========================================================================== */
static FILE_FORMAT GetDfltImageFormat(HWND hdlg) {
	int rc;

	switch (GetRadioButton(hdlg, IDR_IMAGE_BMP, IDR_IMAGE_PNG)) {
		case IDR_IMAGE_RAW:
			rc = FILE_RAW; break;
		case IDR_IMAGE_JPG:
			rc = FILE_JPG; break;
		case IDR_IMAGE_PNG:
			rc = FILE_PNG; break;
		case IDR_IMAGE_BMP:
		default:
			rc = FILE_BMP; break;
	}
	return rc;
}


/* ===========================================================================
-- Draw crosshair (full or small cross) on a bitmap window
--
-- Usage: int GenerateCrosshair(WND_INFO *wnd, HWND hwnd);
--
-- Inputs: wnd - general information about camera application
--               uses only wnd->cursor_posn
--         hwnd - handle to window to draw in
--
-- Output: adds crosshair to image
--
-- Return: 0 if successful, 1 if hwnd is not a window
=========================================================================== */
int GenerateCrosshair(WND_INFO *wnd, HWND hwnd) {
	RECT rect;
	int ix,iy, width, height;
	HDC hdc;

	/* Create brushes for the cross-hair */
	static HBRUSH background, foreground;
	static HPEN hpen;
	static BOOL first = TRUE;

	/* Create brushes and keep them persistent throughout run */
	if (first) {
		background = CreateSolidBrush(RGB(0,0,0));
		foreground = CreateSolidBrush(RGB(255,255,255));
		hpen = CreatePen(PS_SOLID, 1, RGB(255,255,255));
		first = FALSE;
	}

	/* If the specified hwnd is not a valid window, we can safely exit */
	if (! IsWindow(hwnd)) return 1;

	/* Process the request */
	GetClientRect(hwnd, &rect);
	width = rect.right; height = rect.bottom;
	ix = (int) (wnd->cursor_posn.x*width  + 0.5);
	iy = (int) (wnd->cursor_posn.y*height + 0.5);

	/* Get DC */
	hdc = GetDC(hwnd);

	if (! wnd->cursor_posn.fullwidth) {
		int isize;
		isize = min(20,width/10);
		SetRect(&rect, max(ix-isize,0),max(iy-1,0), min(ix+isize,width),min(iy+2,height));
		FillRect(hdc, &rect, background);
		SetRect(&rect, max(0,ix-1),max(0,iy-isize), min(ix+2,width),min(iy+isize,height));
		FillRect(hdc, &rect, background);

		SelectObject(hdc, hpen);
		MoveToEx(hdc, max(ix-isize,0), iy, NULL); LineTo(hdc, min(ix+isize,width), iy);
		MoveToEx(hdc, ix, max(iy-isize,0), NULL); LineTo(hdc, ix, min(iy+isize,height));
	} else {
		SetRect(&rect, 0,max(iy-1,0), width,min(iy+2,height));
		FillRect(hdc, &rect, background);
		SetRect(&rect, max(0,ix-1),0, min(ix+2,width),height);
		FillRect(hdc, &rect, background);

		SelectObject(hdc, hpen);
		MoveToEx(hdc, 0, iy, NULL); LineTo(hdc, width, iy);
		MoveToEx(hdc, ix, 0, NULL); LineTo(hdc, ix, height);
	}		

	/* Release DC */
	ReleaseDC(hwnd, hdc);

	/* Return successful */
	return 0;
}



/* ===========================================================================
================ SUPPORT ROUTINES ... usage is obvious =======================
=========================================================================== */

/* ===========================================================================
-- Display a specified image (PID) on windows with statistics
--
-- Usage: void Camera_ShowImage(WND_INFO *wnd, int index);
--
-- Inputs: wnd   - structure with all the info
--         index - index of frame to display
--         pSharp - pointer to variable to get sharpness estimate (if ! NULL)
--
-- Output: *pSharp - estimate of the sharpness (in some units)
--
-- Return: 0 - all info displayed
--         1 - only images were shown ... main HDLG not a window
--         2 - PauseImageRendering TRUE ... thread currently in a critical section 
=========================================================================== */
static int DCx_ShowImage(WND_INFO *wnd, int index, int *pSharp);
static int TL_ShowImage(WND_INFO *wnd, int index, int *pSharp);

static int Camera_ShowImage(WND_INFO *wnd, int frame, int *pSharp) {
	static char *rname = "Camera_ShowImage";
	int rc;

	if (wnd == NULL) return -1;

	/* This is a number potentially incremented/decremented from existing. Bound to valid range */
	switch (wnd->Camera.driver) {
		case DCX:
			rc = DCx_ShowImage(wnd, frame, pSharp);		/* Includes crosshair */
			break;
		case TL:
			rc = TL_ShowImage(wnd, frame, pSharp);			/* Includes crosshair */
			break;
		default:
			rc = -1;
			break;
	}
	return rc;
}


static int TL_ShowImage(WND_INFO *wnd, int index, int *pSharp) {
	static char *rname = "TL_ShowImage";

	TL_CAMERA *tl;
	char szBuf[256];
	TL_IMAGE *image;

	/* Default return values */
	if (pSharp != NULL) *pSharp = 0;

	/* Validate the parameters */
	if (wnd == NULL || wnd->Camera.driver != TL) return 1;

	/* First ... see if we need to avoid any access to the buffers */
	if (wnd->PauseImageRendering) return 2;

	/* Get pointer to the TL camera structure */
	tl = (TL_CAMERA *) wnd->Camera.details;

	/* If we want the current one, it is also the last valid in the ring */
	if (index < 0) index = tl->iLast;			/* As we set iShow, has to be valid */

	/* Render the bitmap and report statistics */
	if (IsWindow(float_image_hwnd)) {
		TL_RenderFrame(tl, index, float_image_hwnd);
		GenerateCrosshair(wnd, float_image_hwnd);
	}
	if (IsWindow(wnd->thumbnail)) {
		TL_RenderFrame(tl, index, wnd->thumbnail);
		GenerateCrosshair(wnd, wnd->thumbnail);
		if (! CalcStatistics_Active) CalcStatistics(wnd, index, tl->rgb24, pSharp);

		image = &tl->images[index];							/* Currently shown image (after render) */
		sprintf_s(szBuf, sizeof(szBuf), "[%6d] %2.2d:%2.2d:%2.2d.%3.3d", image->imageID,
					 image->system_time.wHour, image->system_time.wMinute, image->system_time.wSecond, image->system_time.wMilliseconds);
		SetDlgItemText(wnd->hdlg, IDT_IMAGE_INFO, szBuf);
	}
	tl->iShow = index;				/* Also done in the TL_RenderFrame code */

	/* Identify the particular ring entry on the main dialog window */
	SetDlgItemInt(wnd->hdlg, IDV_CURRENT_FRAME, tl->iShow, FALSE);

	return 0;
}


static int DCx_ShowImage(WND_INFO *wnd, int index, int *pSharp) {
	static char *rname = "DCx_ShowImage";

	int PID;
	unsigned char *pMem;
	char szBuf[256];
	DCX_CAMERA *dcx;
	UC480IMAGEINFO ImageInfo;

	/* First ... see if we need to avoid any access to the buffers */
	if (wnd->PauseImageRendering) return 2;

	/* Get pointer to the DCX camera structure */
	dcx = wnd->Camera.details;

	/* Get the PID and memory of the requested image.  If not using rings, index is ignored */
#ifndef USE_RINGS
	PID = dcx->Image_PID;
	pMem = dcx->Image_Mem;
#else
	if (dcx->nValid <= 0) return 1;						/* No valid images to display */

	/* Less than zero implies using current ... query camera */
	if (index < 0) {
		is_GetImageMem(wnd->dcx->hCam, &pMem);					/* Determine the PID of last stored image */
		if ( (PID = FindImagePIDFrompMem(wnd->dcx, pMem, &index)) == -1) {
			fprintf(stderr, "[%s] Unable to determine PID for returned current index (%d)\n", rname, index);
			index = 0;
		}
	}
	if (index >= dcx->nValid) index = dcx->nValid-1;
	PID = dcx->Image_PID[index];
	pMem = dcx->Image_Mem[index];
#endif
	if (pMem == NULL) return 1;

	/* Render the bitmap and report statistics */
	/* For DCX cameras, onlyi works if in IS_CM_BGR8_PACKED mode ... also could use is_GetImageHistogram */
	if (IsWindow(float_image_hwnd)) {
		is_RenderBitmap(dcx->hCam, PID, float_image_hwnd, IS_RENDER_FIT_TO_WINDOW);
		GenerateCrosshair(wnd, float_image_hwnd);
	}
	if (IsWindow(wnd->thumbnail))   {
		is_RenderBitmap(dcx->hCam, PID, wnd->thumbnail, IS_RENDER_FIT_TO_WINDOW);
		GenerateCrosshair(wnd, wnd->thumbnail);
		if (! CalcStatistics_Active) CalcStatistics(wnd, 0, pMem, pSharp);

		is_GetImageInfo(dcx->hCam, PID, &ImageInfo, sizeof(ImageInfo));
		sprintf_s(szBuf, sizeof(szBuf), "[%6lld] %2.2d:%2.2d:%2.2d.%3.3d", ImageInfo.u64FrameNumber,
					 ImageInfo.TimestampSystem.wHour, ImageInfo.TimestampSystem.wMinute, ImageInfo.TimestampSystem.wSecond, 
					 ImageInfo.TimestampSystem.wMilliseconds);
		SetDlgItemText(wnd->hdlg, IDT_IMAGE_INFO, szBuf);
	}
	dcx->iShow = index;


	/* Identify the particular ring entry on the main dialog window */
	SetDlgItemInt(wnd->hdlg, IDV_CURRENT_FRAME, index, FALSE);

	return 0;
}

/* ===========================================================================
-- Threads that handle events of a new image available in buffer memory
--
-- This is split into a high priority thread to try to recognize every one
-- and a helper that does the calls to display the buffer into windows and
-- compute the statistics.  This can occasionally be missed.
--
-- Usage: static void RenderImageThread(void *arglist);
--        static void ActualRenderThread(void *arglist);
=========================================================================== */
static void DCx_ImageThread(void *arglist) {
	static char *rname = "DCx_ImageThread";

	int rc, CurrentImageIndex;
	int delta_max;
	unsigned char *pMem;
	WND_INFO *wnd;
	DCX_CAMERA *dcx;
	int PID;
	HIRES_TIMER *timer;

	/* Just wait for events that mean I should render the images */
	printf("DCx_ImageThread thread started\n"); fflush(stdout);

	/* Create a timer so can limit how often */
	timer = HiResTimerCreate();						/* Create the timer and then reset	*/
	HiResTimerReset(timer, 0.00);						/* to report 0.00 at this moment		*/

	/* Now just wait around for images to arrive and process them */
	while (main_wnd != NULL && ! abort_all_threads) {

		if (WAIT_OBJECT_0 != WaitForSingleObject(main_wnd->dcx->FrameEvent, 1000) || main_wnd == NULL) continue;

		wnd = main_wnd;									/* Recover wnd */
		if (wnd->Camera.driver != DCX) continue;	/* We are not active */

		dcx = wnd->Camera.details;						/* Pointer to valid DCX_CAMERA structure	*/
		if (dcx->hCam <= 0) continue;					/* Holy shit ... how would this be			*/
		wnd->Image_Count++;								/* Increment number of images (we think)	*/

		/* Determine the PID and index of last stored image */
		rc = is_GetImageMem(dcx->hCam, &pMem);
		if ( (PID = FindImagePIDFrompMem(wnd->dcx, pMem, &CurrentImageIndex)) == -1) continue;

#ifdef USE_RINGS
		/* Update the main dialog window with the number of images in the ring */
		dcx->iLast = CurrentImageIndex;					/* Shown image will be same as last one valid */
		if (CurrentImageIndex >= dcx->nValid) dcx->nValid = CurrentImageIndex+1;
#endif

#ifdef USE_NUMATO
		if (wnd->numato.enabled && wnd->numato.mode == DIO_TOGGLE) {
			NumatoSetBit(wnd->numato.dio, 0, wnd->numato.phase < wnd->numato.on);
			if (++wnd->numato.phase >= wnd->numato.on+wnd->numato.off) wnd->numato.phase = 0;
		}
#endif

		/* Skip processing if (i) so requested or (ii) too many per second */
		if (wnd->PauseImageRendering) continue;
		if (HiResTimerDelta(timer) < 1.0/MAX_FRAME_DISPLAY_HZ) continue;

		/* Reset timer to control overall allowed display rate */
		HiResTimerReset(timer, 0.00);

		/* Show image with crosshairs */
		Camera_ShowImage(wnd, CurrentImageIndex, &delta_max);						/* Call to ShowImage adds cross-hairs for me */
		if (! IsWindow(wnd->hdlg)) continue;

#ifdef USE_FOCUS
		Update_Focus_Dialog(delta_max);
#endif
	}

	HiResTimerDestroy(timer);
	printf("DCx_ImageThread thread exiting\n"); fflush(stdout);
	return;									/* Only happens when main_wnd is destroyed */
}


/* ---------------------------------------------------------------------------
	--------------------------------------------------------------------------- */

static int DCx_CameraOpen(HWND hdlg, WND_INFO *wnd, DCX_CAMERA *dcx, UC480_CAMERA_INFO *info) {
	static char *rname = "DCx_CameraOpen";

	int i, rc, nformat;
	IMAGE_FORMAT_INFO *ImageFormatInfo;
	CB_INT_LIST *list;
	char szBuf[80];

	/* Disable live video in case currently running */
	wnd->LiveVideo = FALSE;													/* Disable live video */

	/* Make sure we ahve the camera modes dialog box visible */

	ShowDlgItem(hdlg, IDC_CAMERA_MODES, TRUE);

	/* Clear combo selection boxes and mark camera invalid now */
	ComboBoxClearList(hdlg, IDC_CAMERA_MODES);

	/* Try to select and open the camera */
	if ( (rc = DCx_Select_Camera(dcx, info->dwCameraID, &nformat)) != 0) {
		fprintf(stderr, "DCx_Select_Camera failed on camera id %d (rc=%d)\n", info->dwCameraID, rc);
		fflush(stderr);
		return 1;
	}

	/* Populate the combo box in case want to change resolution */
	list = calloc(dcx->NumImageFormats, sizeof(*list));
	for (i=0; i<dcx->NumImageFormats; i++) {
		ImageFormatInfo = dcx->ImageFormatList->FormatInfo+i;
		list[i].value = ImageFormatInfo->nFormatID;
		sprintf_s(szBuf, sizeof(szBuf), "%s", ImageFormatInfo->strFormatName);
		list[i].id = _strdup(szBuf);
	}
	ComboBoxFillIntList(hdlg, IDC_CAMERA_MODES, list, dcx->NumImageFormats);
	if (nformat >= 0) ComboBoxSetByIntValue(hdlg, IDC_CAMERA_MODES, nformat);
	EnableDlgItem(hdlg, IDC_CAMERA_MODES, TRUE);
	for (i=0; i<dcx->NumImageFormats; i++) if (list[i].id != NULL) free(list[i].id);
	free(list);

	/* Set the resolution */
	if ( (rc = Set_DCx_Resolution(hdlg, wnd, nformat)) != 0) {
		fprintf(stderr, "Set_DCx_Resolution failed (rc=%d)\n", rc);
		fflush(stderr);
		return 2;
	}

	/* If needed, start a thread to monitor DCx events and render the image */
	if (! dcx->ProcessNewImageThreadActive) {
		printf("Starting Image Rendering Thread\n"); fflush(stdout);
		_beginthread(DCx_ImageThread, 0, NULL);
		dcx->ProcessNewImageThreadActive = TRUE;
	}

	/* If using rings, start a thread that might be useful for those events */
#ifdef USE_RINGS
	if (! dcx->SequenceThreadActive) {
		printf("Starting Sequence monitor thread\n"); fflush(stdout);
		_beginthread(DCx_SequenceThread, 0, NULL);
		dcx->SequenceThreadActive = TRUE;
	}
#endif

	/* Camera is now fully open ... set wnd->Camera appropriately */
	wnd->Camera.driver  = DCX;
	wnd->Camera.details = dcx;

	/* Enable and disable optional controls (CameraOnControls automatically enabled) */
	EnableDlgItem(hdlg, IDS_MASTER_GAIN, dcx->SensorInfo.bMasterGain);
	EnableDlgItem(hdlg, IDV_MASTER_GAIN, dcx->SensorInfo.bMasterGain);
	EnableDlgItem(hdlg, IDV_RED_GAIN,    dcx->SensorInfo.bRGain);
	EnableDlgItem(hdlg, IDS_RED_GAIN,    dcx->SensorInfo.bRGain);
	EnableDlgItem(hdlg, IDV_GREEN_GAIN,  dcx->SensorInfo.bGGain);
	EnableDlgItem(hdlg, IDS_GREEN_GAIN,  dcx->SensorInfo.bGGain);
	EnableDlgItem(hdlg, IDV_BLUE_GAIN,   dcx->SensorInfo.bBGain);
	EnableDlgItem(hdlg, IDS_BLUE_GAIN,   dcx->SensorInfo.bBGain);
	EnableDlgItem(hdlg, IDB_RESET_GAINS, TRUE);

	EnableDlgItem(hdlg, IDV_FRAME_RATE, TRUE);
	EnableDlgItem(hdlg, IDS_FRAME_RATE, TRUE);

	EnableDlgItem(hdlg, IDS_GAMMA, TRUE);
	EnableDlgItem(hdlg, IDV_GAMMA, TRUE);
	EnableDlgItem(hdlg, IDB_GAMMA_NEUTRAL, TRUE);

	EnableDlgItem(hdlg, IDB_LOAD_PARAMETERS, TRUE);
	EnableDlgItem(hdlg, IDB_SAVE_PARAMETERS, TRUE);

	/* And, finally, start the video now */
	is_StopLiveVideo(dcx->hCam, IS_WAIT);
	is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
	dcx->trigger.mode = TRIG_FREERUN;
	dcx->trigger.bArmed = TRUE;
	dcx->iLast = dcx->iShow = dcx->nValid = 0;

	/* Mark save controls that are visible */
	EnableDlgItem(hdlg, IDB_SAVE,			TRUE);
	EnableDlgItem(hdlg, IDB_SAVE_BURST, TRUE);

	/* Mark trigger controls that are visible */
	memset(&dcx->trigger.capabilities, 0, sizeof(dcx->trigger.capabilities));
	dcx->trigger.capabilities.bFreerun    = TRUE;
	dcx->trigger.capabilities.bExternal   = TRUE;
	dcx->trigger.capabilities.bSingleShot = TRUE;
	dcx->trigger.capabilities.bSoftware   = TRUE;
	dcx->trigger.capabilities.bBurst      = TRUE;
	dcx->trigger.capabilities.bForceExtTrigger = TRUE;
	dcx->trigger.capabilities.bArmDisarm  = TRUE;
	dcx->trigger.capabilities.bExtTrigSlope = TRUE;
	SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);

	/* Camera now active in freerun triggering - tell window */
	wnd->LiveVideo = TRUE;
	SetDlgItemCheck(hdlg, IDC_LIVE, wnd->LiveVideo);

	/* May have changed image, so show cursor (is this needed anymore?) */
	SendMessage(hdlg, WMP_UPDATE_IMAGE_WITH_CURSOR, 0, 0);

	return 0;
}

/* ---------------------------------------------------------------------------
-- Open and initialize structures for working with TL cameras
--
-- Key will be setting Camera.driver to TL and Camera.details to the 
-- TL * structure
--------------------------------------------------------------------------- */
static int TL_CameraOpen(HWND hdlg, WND_INFO *wnd, TL_CAMERA *tl) {
	static char *rname = "TL_CameraOpen";

	int i;
	char szBuf[256];
	
	static struct {
		int wID;
		BOOL enable;
	} TL_Control_List[] = {
		{IDS_GAMMA, FALSE}, {IDV_GAMMA, FALSE}, {IDB_GAMMA_NEUTRAL, FALSE},
		{IDR_COLOR_DISABLE, FALSE}, {IDR_COLOR_ENABLE, FALSE}, {IDR_COLOR_BG40, FALSE}, {IDR_COLOR_HQ, FALSE}, {IDR_COLOR_AUTO_IR, FALSE},
		{IDV_COLOR_CORRECT_FACTOR, FALSE},
		{IDB_LOAD_PARAMETERS, FALSE}, {IDB_SAVE_PARAMETERS, FALSE}
	};

	/* Don't need the camera modes dialog box, but do use ROI */
	ShowDlgItem(hdlg, IDC_CAMERA_MODES, FALSE);
	ShowDlgItem(hdlg, IDB_ROI, TRUE);			EnableDlgItem(hdlg, IDB_ROI, TRUE);
	ShowDlgItem(hdlg, IDT_ROI_INFO, TRUE);		EnableDlgItem(hdlg, IDT_ROI_INFO, TRUE);

	/* Open the requested camera */
	if (TL_OpenCamera(tl, 10) != 0) {
		wnd->Camera.driver = UNKNOWN;
		return 1;
	} else {
		wnd->Camera.driver  = TL;								/* Use TL case in calls */
		wnd->Camera.details = tl;								/* Camera structure is same as passed request details */
	}

	/* Possibly rename the camera */
//	TL_SetCameraName(tl, "40Hz Scientific");

	/* Set the exposure */
	TL_SetExposure(tl, 10.0);									/* Start at 10 ms exposure */

	tl->trigger.mode = -1;										/* Set to invalid so will definitely change */
	TL_SetTriggerMode(tl, TRIG_FREERUN, NULL);			/* Put into continuous trigger */

	_beginthread(TL_ImageThread, 0, (void *) tl);

	/* Enable and disable optional controls (CameraOnControls automatically enabled) */
	EnableDlgItem(hdlg, IDV_MASTER_GAIN, tl->bGainControl);
	EnableDlgItem(hdlg, IDS_MASTER_GAIN, tl->bGainControl);
	EnableDlgItem(hdlg, IDV_RED_GAIN,    tl->IsSensorColor);
	EnableDlgItem(hdlg, IDV_GREEN_GAIN,  tl->IsSensorColor);
	EnableDlgItem(hdlg, IDV_BLUE_GAIN,   tl->IsSensorColor);
	EnableDlgItem(hdlg, IDS_RED_GAIN,    tl->IsSensorColor);
	EnableDlgItem(hdlg, IDS_GREEN_GAIN,  tl->IsSensorColor);
	EnableDlgItem(hdlg, IDS_BLUE_GAIN,   tl->IsSensorColor);
	EnableDlgItem(hdlg, IDB_RESET_GAINS, tl->bGainControl || tl->IsSensorColor);
	EnableDlgItem(hdlg, IDV_FRAME_RATE, tl->bFrameRateControl);
	EnableDlgItem(hdlg, IDS_FRAME_RATE, tl->bFrameRateControl);

	/* Now enable/disable those that don't depend on any camera characteristics */
	for (i=0; i<sizeof(TL_Control_List)/sizeof(TL_Control_List[0]); i++)
		EnableDlgItem(hdlg, TL_Control_List[i].wID, TL_Control_List[i].enable);
	
	/* Mark save controls that are visible */
	EnableDlgItem(hdlg, IDB_SAVE,			TRUE);
	EnableDlgItem(hdlg, IDB_SAVE_BURST, TRUE);

	/* Mark trigger controls that are visible */
	memset(&tl->trigger.capabilities, 0, sizeof(tl->trigger.capabilities));
	tl->trigger.capabilities.bFreerun    = TRUE;
	tl->trigger.capabilities.bExternal   = TRUE;
	tl->trigger.capabilities.bSingleShot = TRUE;
	tl->trigger.capabilities.bSoftware   = TRUE;
	tl->trigger.capabilities.bBurst      = TRUE;
	tl->trigger.capabilities.bArmDisarm  = TRUE;
	tl->trigger.capabilities.bMultipleFramesPerTrigger = TRUE;
	tl->trigger.capabilities.bExtTrigSlope = TRUE;

	SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);

	/* Encode current ROI info */
	sprintf_s(szBuf, sizeof(szBuf), "%dx%d @ (%d,%d)", tl->width, tl->height, tl->roi.dx, tl->roi.dy);
	SetDlgItemText(hdlg, IDT_ROI_INFO, szBuf);

	/* Camera now active in freerun triggering - tell window */
	wnd->LiveVideo = TRUE;
	SetDlgItemCheck(hdlg, IDC_LIVE, wnd->LiveVideo);

	return 0;
}

/* ===========================================================================
-- Process data from TL camera and display in frames as needed
-- 
-- Usage: Proces_TL_Images(void *arglist);
--
-- Inputs: arglist - void * cast of a TL_CAMERA * structure defining the
--                   camera to monitor
--
-- Output: displays frames and generates statistics
--
-- Return: none
--
-- Notes: Thread will exit cleanly if the camera is closed (camera->handle NULL)
--
-- Note: Cannot have any messages to main window since may hang thread trying
--       to change cameras.  Use WM_TIMER instead to update statistics
--       and frame counts
=========================================================================== */
static void TL_ImageThread(void *arglist) {
	static char *rname = "TL_ImageThread";

	TL_CAMERA *tl;
	int delta_max;
	HIRES_TIMER *timer;
	WND_INFO *wnd;
	int rc;

	/* Get the camera to monitor */
	TL_Process_Image_Thread_Active = TRUE;
	TL_Process_Image_Thread_Abort  = FALSE;

	tl = (TL_CAMERA *) arglist;
	fprintf(stderr, "[%s] Thread started monitoring camera: %p\n", rname, tl); fflush(stderr);

	/* Create a timer so can limit how often */
	timer = HiResTimerCreate();						/* Create the timer and then reset	*/
	HiResTimerReset(timer, 0.00);						/* to report 0.00 at this moment		*/

	/* Create an event semaphore and register it to be signaled when new images are available */
	TL_Process_Image_Thread_Trigger = CreateEvent(NULL, FALSE, FALSE, NULL);
	TL_AddImageSignal(tl, TL_Process_Image_Thread_Trigger);

	while (main_wnd != NULL && ! TL_Process_Image_Thread_Abort && TL_IsValidCamera(tl) && ! abort_all_threads) {

		/* Wait for the next image to arrive and increment counters */
		rc = WaitForSingleObject(TL_Process_Image_Thread_Trigger, 1000);
		if (TL_Process_Image_Thread_Abort) break;	/* Immediately check if in abort state */
		if (rc != WAIT_OBJECT_0) continue;

		if ( (wnd = main_wnd) == NULL) continue;	/* Recover most current wnd */
		wnd->Image_Count++;								/* Increment number of images (we think) */

		/* Has user enable autosave? */
		if (wnd->autosave.enable) {
			rc = Camera_GetTriggerMode(wnd, NULL);	/* But only if not LiveVideo */
			if (rc == TRIG_SOFTWARE || rc == TRIG_EXTERNAL || rc == TRIG_SS) {
				char fname[PATH_MAX];
				sprintf_s(fname, sizeof(fname), "%s_%3.3d.raw", wnd->autosave.template, wnd->autosave.next_index);
				SetDlgItemText(wnd->hdlg, IDT_AUTOSAVE_FNAME, fname);
				sprintf_s(fname, sizeof(fname), "%s/%s_%3.3d.raw", wnd->autosave.directory, wnd->autosave.template, wnd->autosave.next_index);
				if (TL_SaveImage(tl, fname, -1, FILE_RAW) != 0) SetDlgItemText(wnd->hdlg, IDT_AUTOSAVE_FNAME, "-- failed --");
				wnd->autosave.next_index++;
			}
		}

		/* Skip processing if (i) so requested or (ii) too many per second */
		if (wnd->PauseImageRendering) continue;
		if (HiResTimerDelta(timer) < 1.0/MAX_FRAME_DISPLAY_HZ) continue;


		/* Reset timer to control overall allowed display rate */
		HiResTimerReset(timer, 0.00);

		/* Show image with crosshairs */
		Camera_ShowImage(wnd, -1, &delta_max);				/* Call to ShowImage adds cross-hairs for me */
		
		if (IsWindow(wnd->hdlg)) {
			#ifdef USE_FOCUS
				Update_Focus_Dialog(delta_max);
			#endif
		}
	}
	TL_Process_Image_Thread_Active = FALSE;				/* Once out of loop, no more chance of lockup */

	/* Cleanup ... remove from chain of camera notifications, close semaphore, delete timer */
	TL_RemoveImageSignal(tl, TL_Process_Image_Thread_Trigger);
	CloseHandle(TL_Process_Image_Thread_Trigger);
	TL_Process_Image_Thread_Trigger = NULL;
	HiResTimerDestroy(timer);

	fprintf(stderr, "[%s] Exited\n", rname); fflush(stderr);
	return;
}

/*===========================================================================
 -- Routine that is a "do nothing" ... just gets signal that a sequence
 -- has completed.  Needed for rings to work with TL cameras
 =========================================================================== */
#ifdef USE_RINGS
static void DCx_SequenceThread(void *arglist) {
	int rc;

	/* Just wait for events that mean I should handle a full block of images */
	printf("DCx_SequenceThread started\n"); fflush(stdout);

	while (main_wnd != NULL) {

		while (main_wnd->dcx->SequenceEvent == NULL) { Sleep(1000); continue; }

		if ( (rc = WaitForSingleObject(main_wnd->dcx->SequenceEvent, 1000)) != WAIT_OBJECT_0) continue;
//		fprintf(stderr, "Got Sequence Event flag\n"); fflush(stderr);
		if (main_wnd == NULL) break;					/* Make sure we are not invalid now */
	}											/* while (main_wnd != NULL) */

	printf("DCx_SequenceThread exiting\n"); fflush(stdout);
	return;									/* Only happens when main_wnd is destroyed */
}
#endif

/* ===========================================================================
-- Reset based on existing loaded camera
=========================================================================== */
int Init_Connected_Camera(HWND hdlg, WND_INFO *wnd, int CameraID) {

	int i, rc, n_formats;
	HCAM hCam;

	IMAGE_FORMAT_LIST *ImageFormatList;	/* Local copy - will be copied to dcx */
	IMAGE_FORMAT_INFO *ImageFormatInfo;

	CB_INT_LIST *list;
	char szBuf[60];

	/* Clear combo selection boxes and mark camera invalid now */
	if (wnd->dcx->ImageFormatList != NULL) free(wnd->dcx->ImageFormatList);
	ComboBoxClearList(hdlg, IDC_CAMERA_MODES);

	/* Verify that we have a valid camera */
	if (wnd->dcx->CameraID <= 0) {
		MessageBox(NULL, "Request made to re-intialize an invalid CameraID", "No cameras available", MB_ICONERROR | MB_OK);
		printf("No cameras\n"); fflush(stdout);
		return 3;
	}
	hCam = wnd->dcx->hCam;

	/* Enumerate the imaging modes */
	rc = is_ImageFormat(hCam, IMGFRMT_CMD_GET_NUM_ENTRIES, &n_formats, sizeof(n_formats)); 
	printf(" Number of image formats: %d\n", n_formats); fflush(stdout);
	if (n_formats <= 0) {
		MessageBox(NULL, "Camera selected appears to have no valid imaging formats.  Don't know what to do", "No image formats available", MB_ICONERROR | MB_OK);
		printf("No image formats were reported to exist\n"); fflush(stdout);
		return 5;
	}

	ImageFormatList = wnd->dcx->ImageFormatList;
	n_formats = wnd->dcx->NumImageFormats;
	list = calloc(n_formats, sizeof(*list));

	for (i=0; i<n_formats; i++) {
		ImageFormatInfo = ImageFormatList->FormatInfo+i;
		list[i].value = ImageFormatInfo->nFormatID;
		sprintf_s(szBuf, sizeof(szBuf), "%s", ImageFormatInfo->strFormatName);
		list[i].id = _strdup(szBuf);
		printf(" %2d:  ID: %2d  Width: %4d  Height: %4d  X0,Y0: %d,%d  Capture: 0x%4.4x  Binning: %d  SubSampling: %2d  Scaler: %g  Format: %s\n", i, 
				 ImageFormatInfo->nFormatID, ImageFormatInfo->nWidth, ImageFormatInfo->nHeight, ImageFormatInfo->nX0, ImageFormatInfo->nY0,
				 ImageFormatInfo->nSupportedCaptureModes, ImageFormatInfo->nBinningMode, ImageFormatInfo->nSubsamplingMode, ImageFormatInfo->dSensorScalerFactor,
				 ImageFormatInfo->strFormatName);
		fflush(stdout);
	}

	ComboBoxFillIntList(hdlg, IDC_CAMERA_MODES, list, n_formats);
	ComboBoxSetByIntValue(hdlg, IDC_CAMERA_MODES, wnd->dcx->ImageFormatID);
	EnableDlgItem(hdlg, IDC_CAMERA_MODES, TRUE);
	EnableDlgItem(hdlg, IDB_CAMERA_DETAILS, TRUE);
	EnableDlgItem(hdlg, IDB_CAMERA_DISCONNECT, TRUE);

	for (i=0; i<n_formats; i++) if (list[i].id != NULL) free(list[i].id);
	free(list);

	return 0;
}

/* ===========================================================================
-- Selects and intializes a specified resolution mode
--
-- Notes:
--   (1) Calls Init_Known_Resolution after the resolution is selected
========================================================================== */
static int Set_DCx_Resolution(HWND hdlg, WND_INFO *wnd, int ImageFormatID) {

	char *Image_Mem = NULL;
	DCX_CAMERA *dcx;

	/* Grab the camera information block directly */
	dcx = wnd->dcx;

	/* Ask the API to initialize the camera with that resolution */
	switch (DCx_Initialize_Resolution(dcx, ImageFormatID)) {
		case 0:
			break;
		case 7:
			MessageBox(NULL, "The requested ImageFormatID did not show up in the camera's list\n", "Image resolution not available", MB_ICONERROR | MB_OK);
			return 7;
		case 8:
			MessageBox(NULL, "Failed to initialize the requested resolution image format", "Select Resolution Failed", MB_ICONERROR | MB_OK);
			return 8;
		default:
			MessageBox(NULL, "Unknown error return from DCx_Select_Resolution", "Select Resolution Failed", MB_ICONERROR | MB_OK);
			return 9;
	}

	/* Save aspect ratio in main information also */
	wnd->height = dcx->height;
	wnd->width  = dcx->width;

	/* Allocate new memory buffers */
	DCx_AllocRingBuffers(wnd, 0);

	/* Do much now the same as if we already had the selected resolution (avoid duplicate code) */
	Init_Known_Resolution(hdlg, wnd, dcx->hCam);


	/* Start the events to actually render the images */
	if (dcx->FrameEvent == NULL) dcx->FrameEvent = CreateEvent(NULL, FALSE, FALSE, FALSE);
	is_InitEvent(dcx->hCam, dcx->FrameEvent, IS_SET_EVENT_FRAME);
	is_EnableEvent(dcx->hCam, IS_SET_EVENT_FRAME);

#ifdef USE_RINGS
	if (dcx->SequenceEvent == NULL) dcx->SequenceEvent = CreateEvent(NULL, FALSE, FALSE, FALSE);
	is_InitEvent(dcx->hCam, dcx->SequenceEvent, IS_SET_EVENT_SEQ);
	is_EnableEvent(dcx->hCam, IS_SET_EVENT_SEQ);
#endif

	/* For some reason, have to stop the live video again to avoid error messages */
	is_StopLiveVideo(dcx->hCam, IS_WAIT);
	wnd->LiveVideo = FALSE;
	if (GetDlgItemCheck(hdlg, IDC_LIVE)) {
		is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
		wnd->LiveVideo = TRUE;
		dcx->iLast = dcx->iShow = dcx->nValid = 0;
		EnableDlgItem(hdlg, IDB_TRIGGER, FALSE);
	} else {
		SetDlgItemCheck(hdlg, IDC_LIVE, FALSE);
		EnableDlgItem(hdlg, IDB_TRIGGER, TRUE);
	}

	return 0;
}


/* ===========================================================================
-- Selects and intializes a specified resolution mode
--
-- Notes:
--   (1) Called from Set_DCx_Resolution after a resolution is selected
========================================================================== */
int Init_Known_Resolution(HWND hdlg, WND_INFO *wnd, HCAM hCam) {

	int rc;
	double rval;
	DCX_CAMERA *dcx;

	/* Load local copy of camera structure */
	dcx = wnd->dcx;

	/* Determine the available color correction modes and enable the radio buttons */
	rval = 0.0;
	rc = is_SetColorCorrection(hCam, IS_GET_SUPPORTED_CCOR_MODE, &rval);
	EnableDlgItem(hdlg, IDG_COLOR_CORRECTION, TRUE);
	EnableDlgItem(hdlg, IDR_COLOR_DISABLE, TRUE);
	EnableDlgItem(hdlg, IDR_COLOR_ENABLE,         rc &  IS_CCOR_ENABLE_NORMAL);
	EnableDlgItem(hdlg, IDR_COLOR_BG40,           rc &                          IS_CCOR_ENABLE_BG40_ENHANCED);
	EnableDlgItem(hdlg, IDR_COLOR_HQ,             rc &                                                         IS_CCOR_ENABLE_HQ_ENHANCED);
	EnableDlgItem(hdlg, IDR_COLOR_AUTO_IR,        rc & (                        IS_CCOR_ENABLE_BG40_ENHANCED | IS_CCOR_ENABLE_HQ_ENHANCED));
	EnableDlgItem(hdlg, IDV_COLOR_CORRECT_FACTOR, rc & (IS_CCOR_ENABLE_NORMAL | IS_CCOR_ENABLE_BG40_ENHANCED | IS_CCOR_ENABLE_HQ_ENHANCED));
	EnableDlgItem(hdlg, IDS_TEXT_0,					 rc & (IS_CCOR_ENABLE_NORMAL | IS_CCOR_ENABLE_BG40_ENHANCED | IS_CCOR_ENABLE_HQ_ENHANCED));

	SendMessage(hdlg, WMP_SHOW_COLOR_CORRECT, 0, 0);
	SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);
	SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
	SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
	SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);

	return 0;
}


/* ===========================================================================
-- Query data associated with a frame
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
int DCx_Query_Frame_Data(int frame, double *tstamp, int *width, int *height, int *pitch, char **pMem) {
	static char *rname = "DCx_Query_Frame_Data";

	WND_INFO *wnd;
	UC480IMAGEINFO ImageInfo;
	DCX_CAMERA *dcx;

	/* Default return values */
	if (tstamp != NULL) *tstamp = 0.0;
	if (width  != NULL) *width  = 0;
	if (height != NULL) *height = 0;
	if (pitch  != NULL) *pitch  = 0;
	if (pMem   != NULL) *pMem   = NULL;

	/* Must have been started at some point to be able to do anything */
	if (main_wnd == NULL) return 1;
	wnd = main_wnd;
	dcx = wnd->dcx;
	if (dcx->hCam <= 0) return 1;

	/* Can't return something we don't have */
	if (frame < 0 || frame >= dcx->nValid) return 2;

	/* Okay, return the data */
	is_GetImageInfo(wnd->dcx->hCam, dcx->Image_PID[frame], &ImageInfo, sizeof(ImageInfo));
	if (tstamp != NULL) *tstamp = ImageInfo.u64TimestampDevice*100E-9;
	if (width  != NULL) *width  = wnd->width;
	if (height != NULL) *height = wnd->height;
	if (pitch  != NULL) is_GetImageMemPitch(wnd->dcx->hCam, pitch);
	if (pMem   != NULL) *pMem   = dcx->Image_Mem[frame];

	return 0;
}

#if 0

/* ===========================================================================
-- Write DCx camera properties to a logfile
--
-- Usage: int DCx_WriteParameters(char *pre_text, FILE *funit);
--
-- Inputs: pre_text - if not NULL, text written each line before the information (possibly "/* ")
--         funit    - open file handle
--
-- Output: Outputs text based information on the camera to the log file
--
-- Return: 0 - successful (camera valid and funit valid)
--         1 - camera not initialized or funit invalid
=========================================================================== */
int DCx_WriteParameters(char *pre_text, FILE *funit) {

	DCX_STATUS status;
	int rc;

	if (pre_text == NULL) pre_text = "";

	if (DCx_Status(NULL, &status) == 0 && funit != NULL) {
		fprintf(funit, "%sThorLabs DCx Camera\n", pre_text);
		fprintf(funit, "%s  Manufacturer: %s\n", pre_text, status.manufacturer);
		fprintf(funit, "%s  Model:        %s\n", pre_text, status.model);
		fprintf(funit, "%s  Serial:       %s\n", pre_text, status.serial);
		fprintf(funit, "%s  Version:      %s\n", pre_text, status.version);
		fprintf(funit, "%s  Date:         %s\n", pre_text, status.date);
		fprintf(funit, "%s  CameraID:     %d\n", pre_text, status.CameraID);
		fprintf(funit, "%s  ColorMode:    %s\n", pre_text, status.color_mode == IMAGE_MONOCHROME ? "Monochrome" : "Color");
		fprintf(funit, "%s  Pixel Pitch:  %d\n", pre_text, status.pixel_pitch);
		fprintf(funit, "%s  Frame rate:   %.2f\n", pre_text, status.fps);
		fprintf(funit, "%s  Exposure:     %.2f ms\n", pre_text, status.exposure);
		fprintf(funit, "%s  Gamma:        %.2f\n", pre_text, status.gamma);
		fprintf(funit, "%s  Gains: Master: %d   RGB: %d,%d,%d\n", pre_text, status.master_gain, status.red_gain, status.green_gain, status.blue_gain);
		fprintf(funit, "%s  Color correction: %d %f\n", pre_text, status.color_correction, status.color_correction_factor);
		rc = 0;
	} else {
		rc = 1;
	}

	return rc;
}

#endif

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
int DCx_Enable_Live_Video(int state) {
	static char *rname = "DCx_Enable_Live_Video";

	WND_INFO *wnd;
	DCX_CAMERA *dcx;
	HWND hdlg;

	/* Must have been started at some point to be able to do anything */
	if (main_wnd == NULL) return 1;
	wnd = main_wnd;
	dcx = wnd->dcx;
	if (dcx->hCam <= 0) return -1;
	hdlg = wnd->hdlg;
	if (hdlg != NULL && ! IsWindow(hdlg)) hdlg = NULL;		/* Mark hdlg if not window */

	/* See IDC_LIVE button for actions */
	if (state == 1) {								/* Do we want to enable? */
		DCx_SetTriggerMode(dcx, TRIG_FREERUN, NULL);
		wnd->LiveVideo = TRUE;
	} else if (state == 0) {
		DCx_SetTriggerMode(dcx, TRIG_SOFTWARE, NULL);
		wnd->LiveVideo = FALSE;
	}

	/* Reset all the others to availability */
	SendMessage(hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);

	/* And return the current state */
	return wnd->LiveVideo;
}

/* ===========================================================================
-- Routines to allocate and release image ring buffers on either size change 
-- or when camera is released / changed
--
-- Usage: int DCx_AllocRingBuffers(WND_INFO *wnd, int nRing);
--
-- Inputs: wnd   - pointer to valid structure for the camera window
--         nRing - number of ring buffers desired.
--                 if <= 1, use current ring size but reallocate (maybe new camera)
--
-- Output: Release clears the sequence for the camera and releases the memory 
--         Alloc will allocate/change memory if there is a valid camera (hCam)
--
-- Return: 0 on success; otherwise an error code
=========================================================================== */
static int DCx_AllocRingBuffers(WND_INFO *wnd, int nRequest) {
	static char *rname = "DCx_AllocRingBuffers";

	int rc;
	DCX_CAMERA *dcx;

	/* Make sure valid arguments */
	if (wnd == NULL) return -1;
	dcx = wnd->dcx;

	/* Pause any image rendering to avoid potential collisions */
	wnd->PauseImageRendering = TRUE;
	Sleep(100);																		/* Sleep to let anything currently in progress complete */

	dcx->trigger.mode = wnd->LiveVideo ? TRIG_FREERUN : TRIG_SOFTWARE ;
	rc = DCx_SetRingBufferSize(dcx, nRequest);
	wnd->LiveVideo = (dcx->trigger.mode == TRIG_FREERUN);

	/* Sleep a moment and then restart image rendering */
	Sleep(200);
	wnd->PauseImageRendering = FALSE;

	return rc;
}
