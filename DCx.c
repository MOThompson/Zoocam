/* dcx camera utility */
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
#include <stdint.h>             /* C99 extension to get known width integers */

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
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "win32ex.h"
#include "graph.h"
#include "resource.h"

#define	_PURE_C
	#include "uc480.h"
#undef	_PURE_C

#define	INCLUDE_DCX_DETAIL_INFO							/* Get all of the typedefs and internal details */
#include "dcx.h"
#include "dcx_server.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#ifndef PATH_MAX
	#define	PATH_MAX	(260)
#endif

#define	nint(x)	(((x)>0) ? ( (int) (x+0.5)) : ( (int) (x-0.5)) )

#define	WMP_SET_FRAMERATE			(WM_APP+2)
#define	WMP_SHOW_FRAMERATE		(WM_APP+3)
#define	WMP_SHOW_EXPOSURE			(WM_APP+4)
#define	WMP_SET_GAMMA				(WM_APP+5)
#define	WMP_SHOW_GAMMA				(WM_APP+6)
#define	WMP_SHOW_COLOR_CORRECT	(WM_APP+7)
#define	WMP_SHOW_GAINS				(WM_APP+8)
#define	WMP_SHOW_CURSOR_POSN		(WM_APP+9)

#define	MIN_FPS		(0.5)
#define	MAX_FPS		(15)

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
int  DCx_Init_Driver(void);
void DCx_Final_Closeout(void);
int  DCx_Enum_Camera_List(int *pcount, UC480_CAMERA_INFO **pinfo);
int  DCx_Select_Camera(HWND hdlg, DCX_WND_INFO *dcx, int CameraID, int *nBestFormat);
int  DCx_Select_Resolution(HWND hdlg, DCX_WND_INFO *dcx, int ImageFormatID);

int InitializeScrollBars(HWND hdlg, DCX_WND_INFO *dcx);
int InitializeHistogramCurves(HWND hdlg, DCX_WND_INFO *dcx);
int Init_Known_Resolution(HWND hdlg, DCX_WND_INFO *dcx, HCAM hCam);
void FreeCurve(GRAPH_CURVE *cv);

static int ReleaseRingBuffers(DCX_WND_INFO *dcx);
static int AllocRingBuffers(DCX_WND_INFO *dcx, int nRing);
static int SaveBurstImages(DCX_WND_INFO *dcx);

static void show_camera_info_thread(void *arglist);
BOOL CALLBACK CameraInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

static double my_timer(BOOL reset);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

HWND DCx_main_hdlg = NULL;											/* Global identifier of my window handle */

/* Global information about the cameras in the system */
static int CameraCount = 0;										/* How many cameras are on the system */
static UC480_CAMERA_INFO *CameraDetails = NULL;				/* Enumerated set of cameras and information */

static HINSTANCE hInstance=NULL;
static HWND float_image_hwnd;										/* Handle to free-floating image window */

static DCX_WND_INFO *main_dcx = NULL;

/* List of modes corresponding to radio button order */
int ColorCorrectionModes[] = { IS_CCOR_DISABLE, IS_CCOR_ENABLE_NORMAL, IS_CCOR_ENABLE_BG40_ENHANCED, IS_CCOR_ENABLE_HQ_ENHANCED, IS_CCOR_SET_IR_AUTOMATIC };
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
	IDB_LIVE, IDB_UNDOCK, IDB_CAPTURE, IDB_SAVE,
	IDS_FRAME_RATE, IDV_FRAME_RATE, IDT_FRAMERATE, IDS_EXPOSURE_TIME, IDV_EXPOSURE_TIME,
	IDS_GAMMA, IDV_GAMMA, IDB_GAMMA_NEUTRAL,
	IDR_COLOR_DISABLE, IDR_COLOR_ENABLE, IDR_COLOR_BG40, IDR_COLOR_HQ, IDR_COLOR_AUTO_IR, 
	IDV_COLOR_CORRECT_FACTOR,
	IDT_RED_SATURATE, IDT_GREEN_SATURATE, IDT_BLUE_SATURATE, 
	IDS_MASTER_GAIN, IDV_MASTER_GAIN, IDS_RED_GAIN, IDV_RED_GAIN, IDS_GREEN_GAIN, IDV_GREEN_GAIN, IDS_BLUE_GAIN, IDV_BLUE_GAIN,
	IDR_EXPOSURE_100US, IDR_EXPOSURE_1MS, IDR_EXPOSURE_10MS, IDR_EXPOSURE_100MS,
	IDC_SHOW_INTENSITY, IDC_SHOW_RGB,
	ID_NULL
};
/* List of camera controls that get turned off when starting resolution change */
int CameraOffControls[] = { 
	IDB_SAVE_PARAMETERS, IDB_LOAD_PARAMETERS,
	IDB_LIVE, IDB_UNDOCK, IDB_CAPTURE, IDB_SAVE,
	IDS_FRAME_RATE, IDV_FRAME_RATE, IDT_FRAMERATE, IDS_EXPOSURE_TIME, IDV_EXPOSURE_TIME,
	IDS_GAMMA, IDV_GAMMA, IDB_GAMMA_NEUTRAL,
	IDR_COLOR_DISABLE, IDR_COLOR_ENABLE, IDR_COLOR_BG40, IDR_COLOR_HQ, IDR_COLOR_AUTO_IR, IDV_COLOR_CORRECT_FACTOR,
	IDV_COLOR_CORRECT_FACTOR,
	IDT_RED_SATURATE, IDT_GREEN_SATURATE, IDT_BLUE_SATURATE, 
	IDS_MASTER_GAIN, IDV_MASTER_GAIN, IDS_RED_GAIN, IDV_RED_GAIN, IDS_GREEN_GAIN, IDV_GREEN_GAIN, IDS_BLUE_GAIN, IDV_BLUE_GAIN,
	IDR_EXPOSURE_100US, IDR_EXPOSURE_1MS, IDR_EXPOSURE_10MS, IDR_EXPOSURE_100MS,
	IDC_SHOW_INTENSITY, IDC_SHOW_RGB,
	ID_NULL
};
/* List of camera controls that get turned on when resolution is set */
int CameraOnControls[] = { 
	IDC_CAMERA_LIST, IDC_CAMERA_MODES,
	IDB_SAVE_PARAMETERS, IDB_LOAD_PARAMETERS,
	IDB_LIVE, IDB_UNDOCK, IDB_CAPTURE, IDB_SAVE,
	IDS_FRAME_RATE, IDV_FRAME_RATE, IDT_FRAMERATE, IDS_EXPOSURE_TIME, IDV_EXPOSURE_TIME,
	IDS_GAMMA, IDV_GAMMA, IDB_GAMMA_NEUTRAL,
	IDV_COLOR_CORRECT_FACTOR,
	IDT_RED_SATURATE, IDT_GREEN_SATURATE, IDT_BLUE_SATURATE, 
	IDR_EXPOSURE_100US, IDR_EXPOSURE_1MS, IDR_EXPOSURE_10MS, IDR_EXPOSURE_100MS,
	IDC_SHOW_INTENSITY, IDC_SHOW_RGB,
	ID_NULL
};

/* ===========================================================================
-- Simple test thread to verify calls for SPEC are working properly
--
-- Temporarily replace some button with 
--   _beginthread(test_thread, 0, NULL);
=========================================================================== */
static void test_thread(void *arglist) {
	DCX_STATUS status;	
	DCX_IMAGE_INFO info;
	int rc;

	printf("test thread started\n"); fflush(stdout);
	Sleep(5000);											/* Wait a while before starting */
	printf("calling DCx_Status\n"); fflush(stdout);
	rc = DCx_Status(&status);
	printf("DCx_Status returns: %s\n", rc == 0 ? "Success" : "Fail"); fflush(stdout);
	if (rc == 0) {
		printf("  Manufacturer: %s\n", status.manufacturer);
		printf("  Model: %s\n", status.model);
		printf("  Serial: %s\n", status.serial);
		printf("  Version: %s\n", status.version);
		printf("  Date: %s\n", status.date);
		printf("  CameraID: %d\n", status.CameraID);
		printf("  ColorMode: %s\n", status.color_mode == IMAGE_MONOCHROME ? "monochrome" : "color");
		printf("  Pixel Pitch: %d\n", status.pixel_pitch);
		printf("  Frame rate: %.2f\n", status.fps);
		printf("  Exposure: %.2f ms\n", status.exposure);
		printf("  Gamma: %.2f\n", status.gamma);
		printf("  Gains: Master: %d   RGB: %d,%d,%d\n", status.master_gain, status.red_gain, status.green_gain, status.blue_gain);
		printf("  Color correction: %d %f\n", status.color_correction, status.color_correction_factor);
	}

	rc = DCx_Capture_Image("test_img.bmp", IMAGE_BMP, 100, &info, NULL);
	printf("DCx_Capture_Image returns: %d\n", rc); fflush(stdout);
	if (rc == 0) {
		double r,g,b;
		printf("  Image size: %d x %d\n", info.width, info.height); fflush(stdout);
		printf("  Exposure: %.2f ms\n", info.exposure); fflush(stdout);
		printf("  Gains: Master: %d   RGB: %d,%d,%d\n", info.master_gain, info.red_gain, info.green_gain, info.blue_gain); fflush(stdout);
		printf("  Color correction: %d %f\n", info.color_correction, info.color_correction_factor); fflush(stdout);
		r = 100.0 * info.red_saturate / (1.0*info.width*info.height);
		g = 100.0 * info.green_saturate / (1.0*info.width*info.height);
		b = 100.0 * info.blue_saturate / (1.0*info.width*info.height);
		printf("  Saturated pixels: RGB: %d %d %d (%.2f%%, %.2f%%, %.2f%%)\n", info.red_saturate, info.green_saturate, info.blue_saturate, r,g,b); fflush(stdout);
	}

	return;
}


/* ===========================================================================
=========================================================================== */
/* This is where all the input to the window goes to */
LRESULT CALLBACK FloatImageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	int cxClient, cyClient, cyOffset;
	RECT *pWindow, Client;
	POINT point;
	BOOL rc;
	double aspect;								/* needed Width/Height ratio (640/480) */
	HDC	hdc;									/* Device context for drawing */
	PAINTSTRUCT paintstruct;

	aspect = main_dcx->Image_Aspect;

	rc = FALSE;
	switch (msg) {

		case WM_CREATE:
			SetWindowText(hwnd, "DCx Camera Image");
			rc = TRUE; break;
			
		case WM_TIMER:											/* While running, update screen once every other second */
			rc = TRUE; break;

		case WM_PAINT:
			hdc = BeginPaint(hwnd, &paintstruct);		/* Get DC */
			EndPaint(hwnd, &paintstruct);					/* Release DC */
			rc = TRUE; break;

		case WM_RBUTTONDOWN:
			GetCursorPos((LPPOINT) &point);
			ScreenToClient(hwnd, &point);
			GetClientRect(hwnd, &Client);
			main_dcx->x_image_target = (1.0*point.x) / Client.right;
			main_dcx->y_image_target = (1.0*point.y) / Client.bottom;
			SendMessage(main_dcx->main_hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
			rc = TRUE; break;

		case WM_LBUTTONDBLCLK:								/* Magnify at this location */
		case WM_MBUTTONDOWN:
			break;

		case WM_SIZING:
			pWindow = (RECT *) lParam;		/* left, right, left, bottom */
			GetClientRect(hwnd, &Client);
			point.x = point.y = 0;
			ClientToScreen(hwnd, &point);
			cxClient = Client.right;
			cyClient = Client.bottom;
			cyOffset = (pWindow->bottom-pWindow->top) - cyClient;				/* 2 is estimated size of the bottom bar of the window */
			switch (wParam) {
				case WMSZ_RIGHT:
				case WMSZ_LEFT:
					cyClient = (int) (cxClient / aspect + 0.5);
					pWindow->bottom = pWindow->top + cyClient + cyOffset;
					break;
				case WMSZ_TOP:
				case WMSZ_BOTTOM:
					cxClient = (int) (cyClient * aspect + 0.5);
					pWindow->right = pWindow->left + cxClient;
					break;
				case WMSZ_BOTTOMLEFT:															/* Choose to keep the larger and expand to fit aspect ratio */
				case WMSZ_BOTTOMRIGHT:
				case WMSZ_TOPLEFT:
				case WMSZ_TOPRIGHT:
					if (1.0*cxClient/cyClient > aspect) {									/* Change cyClient */
						cyClient = (int) (cxClient / aspect + 0.5);
						if (wParam == WMSZ_BOTTOMLEFT || wParam == WMSZ_BOTTOMRIGHT) {
							pWindow->bottom = pWindow->top + cyClient + cyOffset;
						} else {
							pWindow->top = pWindow->bottom - cyClient - cyOffset;
						}
					} else {																			/* Change cxClient */
						cxClient = (int) (cyClient * aspect + 0.5);
						if (wParam == WMSZ_BOTTOMRIGHT || wParam == WMSZ_TOPRIGHT) {
							pWindow->right = pWindow->left + cxClient;
						} else {
							pWindow->left = pWindow->right - cxClient;
						}
					}
					break;
			}
			rc = TRUE; break;									/* Message was handled */

		case WM_SIZE:
			cxClient = LOWORD(lParam);
			cyClient = HIWORD(lParam);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

/* All other messages (a lot of them) are processed using default procedures */
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return rc;
}

/* ===========================================================================
=========================================================================== */
void GenerateCrosshair(	DCX_WND_INFO *dcx, HWND hwnd) {
	RECT rect;
	HDC hdc;
	int ix,iy, width, height;

	/* Create brushes for the cross-hair */
	static HBRUSH background, foreground;
	static HPEN hpen;
	static BOOL first = TRUE;

	if (first) {
		background = CreateSolidBrush(RGB(0,0,0));
		foreground = CreateSolidBrush(RGB(255,255,255));
		hpen = CreatePen(PS_SOLID, 1, RGB(255,255,255));
		first = FALSE;
	}

	GetClientRect(hwnd, &rect);
	width = rect.right; height = rect.bottom;
	ix = (int) (dcx->x_image_target*width  + 0.5);
	iy = (int) (dcx->y_image_target*height + 0.5);

	hdc = GetDC(hwnd);				/* Get DC */
	if (! dcx->full_width_cursor) {
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

	ReleaseDC(hwnd, hdc);			/* Release DC */
	return;
}


/* ===========================================================================
-- Routine to return the PID corresponding to the active pMem retrieved
-- via is_GetImageMem() routine.  For single image mode, just return
-- the PID from the initial memory allocation.  For ring buffering, search
-- through the list and return the matching one.
--
-- Usage: FindImagePID(DCX_WND_INFO *dcx, void *pMem, int *index);
--
-- Inputs: dcx - pointer to valid structure for the window
--         pMem - memory buffer returned from is_GetImageMem
--         index - if not NULL, variable to receive the index into the
--                 ring buffer table
--
-- Output: *index - actual index (0 based) into dcx->Image_PID, _Mem, and _Time
--
-- Returns: PID corresponding to the pMem if it exists.  Otherwise -1
=========================================================================== */
static int FindImagePID(DCX_WND_INFO *dcx, void *pMem, int *index) {

#ifndef USE_RINGS
	return dcx->Image_PID;

#else
	int i, PID;

	/* Set the default return on errors */
	PID = -1;
	if (index != NULL) *index = -1;

	/* Scan through the list */
	if (dcx != NULL && dcx->Image_Mem_Allocated) {
		for (i=0; i<dcx->nRing; i++) {
			if (dcx->Image_Mem[i] == pMem) {
				PID = dcx->Image_PID[i];
				if (index != NULL) *index = i;
				break;
			}
		}
		if (i >= dcx->nRing) { fprintf(stderr, "ERROR: Unable to find a PID corresponding to the image memory (%p)\n", pMem); fflush(stderr); }
//		fprintf(stderr, "Buffer %3.3d: PID=%3.3d  buffer=%p\n", i, PID, pMem); fflush(stderr);
	}

	return PID;
#endif
}


#ifdef USE_RINGS
static void SequenceThread(void *arglist) {
	int rc;
	
	/* Just wait for events that mean I should handle a full block of images */
	printf("SequenceThread started\n"); fflush(stdout);

	while (main_dcx != NULL) {

		while (main_dcx->SequenceEvent == NULL) { Sleep(100); continue; }

		if ( (rc = WaitForSingleObject(main_dcx->SequenceEvent, 1000)) != WAIT_OBJECT_0) continue;

		if (main_dcx == NULL) break;					/* Make sure we are not invalid now */

		fprintf(stderr, "INFO: Saw a SequenceEvent triggered\n");
	}											/* while (main_dcx != NULL) */

	printf("SequenceThread exiting\n"); fflush(stdout);
	return;									/* Only happens when main_dcx is destroyed */
}
#endif


/* ===========================================================================
-- Threads that handle events of a new image available in buffer memory
--
-- This is split into a high priority thread to try to recognize every one
-- and a helper that does the calls to display the buffer into windows and
-- compute the statistics.  This can occasionally be mised.
--
-- Usage: static void RenderImageThread(void *arglist);
--        static void ActualRenderThread(void *arglist);
=========================================================================== */

static void ProcessNewImage(void *arglist) {

	int i, rc, col, line, height, width, pitch, index;
	unsigned char *pMem, *aptr;
	GRAPH_CURVE *red, *green, *blue;
	GRAPH_CURVE *vert, *vert_r, *vert_g, *vert_b;
	GRAPH_CURVE *horz, *horz_r, *horz_g, *horz_b;
	HANDLE hdlg;
	DCX_WND_INFO *dcx;
	GRAPH_SCALES scales;
	int PID;

	/* Just wait for events that mean I should render the images */
	printf("ProcessNewImage thread started\n"); fflush(stdout);

	while (main_dcx != NULL) {

		if ( (rc = WaitForSingleObject(main_dcx->FrameEvent, 1000)) != WAIT_OBJECT_0 || main_dcx == NULL) continue;

		dcx = main_dcx;									/* Recover dcx */
		dcx->Image_Count++;								/* Increment number of images (we think) */
		if (dcx->hCam <= 0) continue;

		/* Determine the PID and pitch of last stored image */
		rc = is_GetImageMem(dcx->hCam, &pMem);
		rc = is_GetImageMemPitch(dcx->hCam, &pitch);
		if ( (PID = FindImagePID(dcx, pMem, &index)) == -1) continue;

#ifdef USE_RINGS
		/* Update the main dialog window with the number with the number of images in the ring */
		dcx->nLast = index;
		if (index >= dcx->nValid) dcx->nValid = index+1;
		if (IsWindow(dcx->main_hdlg)) {
			SetDlgItemInt(dcx->main_hdlg, IDT_FRAME_COUNT, dcx->nLast+1, FALSE);
			SetDlgItemInt(dcx->main_hdlg, IDT_FRAME_VALID, dcx->nValid, FALSE);
		}
#endif

		if (IsWindow(float_image_hwnd)) {
			is_RenderBitmap(dcx->hCam, PID, float_image_hwnd, IS_RENDER_FIT_TO_WINDOW);
			GenerateCrosshair(dcx, float_image_hwnd);
		}
		if (IsWindow(dcx->thumbnail)) {
			is_RenderBitmap(dcx->hCam, PID, dcx->thumbnail, IS_RENDER_FIT_TO_WINDOW);
			GenerateCrosshair(dcx, dcx->thumbnail);
		}

		/* If the maiin window isn't a window, don't bother with the histogram calculations */
		if (! IsWindow(dcx->main_hdlg)) continue;
		
		/* Do the histogram calculations */
		height = dcx->height;
		width  = dcx->width;
		hdlg   = dcx->main_hdlg;
		
		/* Split based on RGB or only monochrome */
		/* For some strange reason, only works if in IS_CM_BGR8_PACKED mode */
		red    = dcx->red_hist;
		green  = dcx->green_hist;
		blue   = dcx->blue_hist;
		for (i=0; i<red->npt; i++) red->y[i] = green->y[i] = blue->y[i] = 0;
		for (line=0; line<height; line++) {
			aptr = pMem + line*pitch;					/* Pointer to this line */
			for (col=0; col<width; col++) {
				if (dcx->SensorIsColor) {
					i = aptr[3*col+0]; if (i < 0) i = 0; if (i > 255) i = 255; blue->y[i]++;
					i = aptr[3*col+1]; if (i < 0) i = 0; if (i > 255) i = 255; green->y[i]++;
					i = aptr[3*col+2]; if (i < 0) i = 0; if (i > 255) i = 255; red->y[i]++;
				} else {
					i = aptr[col]; if (i < 0) i = 0; if (i > 255) i = 255; red->y[i]++;
				}
			}
		}
		if (dcx->SensorIsColor) {
			SetDlgItemDouble(hdlg, IDT_RED_SATURATE,   "%.2f%%", (100.0*red->y[255]  )/(1.0*height*width));
			SetDlgItemDouble(hdlg, IDT_GREEN_SATURATE, "%.2f%%", (100.0*green->y[255])/(1.0*height*width));
			SetDlgItemDouble(hdlg, IDT_BLUE_SATURATE,  "%.2f%%", (100.0*blue->y[255] )/(1.0*height*width));
			dcx->red_saturate   = (int) red->y[255];		red->y[255]   = 0;
			dcx->green_saturate = (int) green->y[255];	green->y[255] = 0;
			dcx->blue_saturate  = (int) blue->y[255];		blue->y[255]  = 0;
			red->modified = green->modified = blue->modified = TRUE;
			red->visible  = green->visible  = blue->visible  = TRUE;
			red->rgb = RGB(255,0,0);
			SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_REDRAW, 0, 0);
		} else {
			double rval;
			rval = (100.0*red->y[255]  )/(1.0*height*width);
			SetDlgItemDouble(hdlg, IDT_RED_SATURATE,   "%.2f%%", rval);
			SetDlgItemDouble(hdlg, IDT_GREEN_SATURATE, "%.2f%%", rval);
			SetDlgItemDouble(hdlg, IDT_BLUE_SATURATE,  "%.2f%%", rval);
			dcx->red_saturate = dcx->green_saturate = dcx->blue_saturate = (int) red->y[255];
			red->modified = TRUE;
			red->visible = TRUE; green->visible = FALSE; blue->visible = FALSE;
			red->rgb = RGB(225,225,255);
			SendDlgItemMessage(hdlg, IDG_HISTOGRAMS, WMP_REDRAW, 0, 0);
		}
		
		/* Do the horizontal profile at centerline */
		horz = dcx->horz_w;
		horz_r = dcx->horz_r;
		horz_g = dcx->horz_g;
		horz_b = dcx->horz_b;
		if (horz->npt < width) {
			horz->x   = realloc(horz->x, width*sizeof(*horz->x));
			horz->y   = realloc(horz->y, width*sizeof(*horz->y));
			horz_r->x = realloc(horz_r->x, width*sizeof(*horz_r->x));
			horz_r->y = realloc(horz_r->y, width*sizeof(*horz_r->y));
			horz_g->x = realloc(horz_g->x, width*sizeof(*horz_g->x));
			horz_g->y = realloc(horz_g->y, width*sizeof(*horz_g->y));
			horz_b->x = realloc(horz_b->x, width*sizeof(*horz_b->x));
			horz_b->y = realloc(horz_b->y, width*sizeof(*horz_b->y));
		}
		horz->npt = horz_r->npt = horz_g->npt = horz_b->npt = width;
		aptr = pMem + pitch*((int) (height*dcx->y_image_target+0.5));			/* Pointer to target line */
		for (i=0; i<width; i++) {
			horz->x[i] = horz_r->x[i] = horz_g->x[i] = horz_b->x[i] = i;
			if (dcx->SensorIsColor) {
				horz->y[i] = (aptr[3*i+0] + aptr[3*i+1] + aptr[3*i+2])/3.0 ;		/* Average intensity */
				horz_r->y[i] = aptr[3*i+2];
				horz_g->y[i] = aptr[3*i+1];
				horz_b->y[i] = aptr[3*i+0];
			} else {
				horz->y[i] = horz_r->y[i] = horz_g->y[i] = horz_b->y[i] = aptr[i];
			}
		}
		memset(&scales, 0, sizeof(scales));
		scales.xmin = 0;	scales.xmax = width-1;
		scales.ymin = 0;  scales.ymax = 256;
		scales.autoscale_x = FALSE; scales.force_scale_x = TRUE;
		scales.autoscale_y = FALSE; scales.force_scale_y = TRUE;
		horz->modified = TRUE;
		horz->modified = horz_r->modified = horz_g->modified = horz_b->modified = TRUE;
		SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_SET_SCALES, (WPARAM) &scales, (LPARAM) 0);
		SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_REDRAW, 0, 0);
		
		vert = dcx->vert_w;
		vert_r = dcx->vert_r;
		vert_g = dcx->vert_g;
		vert_b = dcx->vert_b;
		if (vert->npt < height) {
			vert->x = realloc(vert->x, height*sizeof(*vert->x));
			vert->y = realloc(vert->y, height*sizeof(*vert->y));
			vert_r->x = realloc(vert_r->x, height*sizeof(*vert_r->x));
			vert_r->y = realloc(vert_r->y, height*sizeof(*vert_r->y));
			vert_g->x = realloc(vert_g->x, height*sizeof(*vert_g->x));
			vert_g->y = realloc(vert_g->y, height*sizeof(*vert_g->y));
			vert_b->x = realloc(vert_b->x, height*sizeof(*vert_b->x));
			vert_b->y = realloc(vert_b->y, height*sizeof(*vert_b->y));
		}
		vert->npt = vert_r->npt = vert_g->npt = vert_b->npt = height;
		for (i=0; i<height; i++) {
			vert->y[i] = vert_r->y[i] = vert_g->y[i] = vert_b->y[i] = height-1-i;
			if (dcx->SensorIsColor) {
				aptr = pMem + i*pitch + 3*((int) (width*dcx->x_image_target+0.5));	/* Access first of the column */
				vert->x[i] = (3*256 - (aptr[0] + aptr[1] + aptr[2])) / 3.0;
				vert_r->x[i] = 256 - aptr[2];
				vert_g->x[i] = 256 - aptr[1];
				vert_b->x[i] = 256 - aptr[0];
			} else {
				aptr = pMem + i*pitch + ((int) (width*dcx->x_image_target+0.5));		/* Access first of the column */
				vert->x[i] = vert_r->x[i] = vert_g->x[i] = vert_b->x[i] = 256 - aptr[0];
			}
		}
		memset(&scales, 0, sizeof(scales));
		scales.xmin = 0; scales.xmax = 256;
		scales.ymin = 0; scales.ymax = height-1;
		scales.autoscale_x = FALSE;  scales.force_scale_x = TRUE;
		scales.autoscale_y = FALSE;  scales.force_scale_y = TRUE;
		vert->modified = vert_r->modified = vert_g->modified = vert_b->modified = TRUE;
		SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_SCALES, (WPARAM) &scales, (LPARAM) 0);
		SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_REDRAW, 0, 0);
	}

	printf("ProcessNewImage thread exiting\n"); fflush(stdout);
	return;									/* Only happens when main_dcx is destroyed */
}
	

/* ===========================================================================
-- Thread to autoset the intentity so max is between 95 and 100% with
-- no saturation of any of the channels
--
-- Usage: _beginthread(AutoExposureThread, NULL);
=========================================================================== */
static void AutoExposureThread(void *arglist) {

	static BOOL active=FALSE;

	int i, try;
	char msg[20];
	DCX_WND_INFO *dcx;
	HWND hdlg;
	double exposure, last_exposure;					/* Exposure time in ms */
	double upper_bound, lower_bound, min_increment;
	int LastImage, max_saturate, red_peak, green_peak, blue_peak, peak;

	/* Get a pointer to the data structure */
	dcx = (DCX_WND_INFO*) arglist;
	hdlg = dcx->main_hdlg;
	if (hdlg != NULL && ! IsWindow(hdlg)) hdlg = NULL;		/* Mark hdlg if not window */

	/* Avoid multiple by just monitoring myself */
	if (active || ! dcx->LiveVideo || ! dcx->Image_Mem_Allocated) {
		Beep(300,200);
		return;
	}
	active = TRUE;
	if (hdlg != NULL) {
		EnableDlgItem(hdlg, IDB_AUTO_EXPOSURE, FALSE);
		SetDlgItemText(hdlg, IDB_AUTO_EXPOSURE, "iter 0");
	}

/* -------------------------------------------------------------------------------
-- Get camera information on range of exposure allowed
-- Note that is_Exposure(IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE) is limited by current
-- framerate while is_GetFrameTimeRange() is not.  But deal with bug in the return 
-- values from is_GetFrameTimeRange()
--------------------------------------------------------------------------- */
	is_GetFrameTimeRange(dcx->hCam, &lower_bound, &upper_bound, &min_increment);
//	lower_bound   *= 1000;											/* Go from seconds to ms (but looks to already in ms so ignore) */
	upper_bound   *= 1000;											/* Go from seconds to ms */
	min_increment *= 1000;											/* Go from seconds to ms */

	/* Set maximum number of saturated pixels to tolerate */
	max_saturate = dcx->width*dcx->height/1000;															/* Max tolerated as saturated */

	/* Do binary search ... 1024 => 0.1% which is close enough */
	for (try=0; try<10; try++) {

		/* Get current exposure time (ms) and image info */
		is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &exposure, sizeof(exposure));
		last_exposure = exposure;									/* Hold so know change at end */
		LastImage = dcx->Image_Count;

		/* If saturated, new upper_bound and use mid-point of lower/upper next time */
		if (dcx->red_saturate > max_saturate || dcx->green_saturate > max_saturate || dcx->blue_saturate > max_saturate) {
			upper_bound = exposure;
			exposure = (upper_bound + lower_bound) / 2.0;

		/* Okay, intensity is low, not high ... ignore top max_saturate pixels to get highest intensity */
		} else {
			lower_bound = exposure;

			peak = max_saturate - dcx->red_saturate;
			for (i=255; i>=0,peak>0; i--) peak -= (int) dcx->red_hist->y[i];
			red_peak = i+1;

			peak = max_saturate - dcx->green_saturate;
			for (i=255; i>=0,peak>0; i--) peak -= (int) dcx->green_hist->y[i];
			green_peak = i+1;

			peak = max_saturate - dcx->blue_saturate;
			for (i=255; i>=0,peak>0; i--) peak -= (int) dcx->blue_hist->y[i];
			blue_peak = i+1;

			peak = max(red_peak, green_peak); peak = max(peak, blue_peak);
			if (peak >= 245) break;															/* We are done */

			exposure *= 250.0/peak;															/* New estimated exposure */
			if (exposure > upper_bound) exposure = (upper_bound + lower_bound) / 2.0;

		}
		if (fabs(exposure - last_exposure) < min_increment) break;				/* Not enough change to be done */

		/* Reset the gain now and collect a few images to stabilize */
		DCx_Set_Exposure(dcx, exposure, TRUE, dcx->main_hdlg);

		/* Wait for at least the 3th new exposure to stabilize image */
		if (hdlg != NULL) {
			sprintf_s(msg, sizeof(msg), "iter %d", try+1);
			SetDlgItemText(hdlg, IDB_AUTO_EXPOSURE, msg);
		}
		for (i=0; i<10; i++) {
			if (dcx->Image_Count > LastImage+2) break;
			Sleep(max(nint(exposure/2), 30));
		}
		if (dcx->Image_Count <= LastImage+2) break;									/* No new image - abort */
	}

	active = FALSE;						/* We are done with what we will try */
	if (hdlg != NULL) {
		SetDlgItemText(hdlg, IDB_AUTO_EXPOSURE, "Auto");
		EnableDlgItem(hdlg, IDB_AUTO_EXPOSURE, TRUE);
	}

	return;
}

/* ===========================================================================
-- The 'main' function of Win32 GUI programs: this is where execution starts
=========================================================================== */
int WINAPI ImageWindow(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	WNDCLASSEX wc;			/* A properties struct of our window */
//	HWND hwnd;				/* A 'HANDLE', hence the H, or a pointer to our window */
	MSG msg;					/* A temporary location for all messages */
	static BOOL first = TRUE;

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

	float_image_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, "FloatGraphClass", "Caption", WS_VISIBLE | WS_OVERLAPPEDWINDOW,
								 CW_USEDEFAULT, /* x */
								 CW_USEDEFAULT, /* y */
								 640, /* width */
								 500, /* height */
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

/* ===========================================================================
-- Routine to initialize the DCx driver.  Safe to call multiple times.
-- 
-- Usage: int DCx_InitDriver(void);
--
-- Inputs: none
--
-- Output: sets initial driver conditions
--
-- Return: 0 if successful (always)
=========================================================================== */
int DCx_Init_Driver(void) {
	static BOOL done=FALSE;

/* Initialize the DCx software to pop up errors (for now so know when doing something wrong */
	if (! done) {
		is_SetErrorReport(0, IS_ENABLE_ERR_REP);
		done = TRUE;
	}

	return 0;
}

/* ===========================================================================
-- Routine to clean up DCx driver on exit.  Use via atexit() routine.
-- 
-- Usage: void DCx_Final_Closeout(void);
--
-- Inputs: none
--
-- Output: releases camera if initialized, frees memory
--
-- Return: none
=========================================================================== */
void DCx_Final_Closeout(void) {
	DCX_WND_INFO *dcx;

	if ( (dcx = main_dcx) != NULL) {
		printf("Performing final shutdown of DCx camera\n"); fflush(stdout);
		/* Close the camera */
		if (dcx->hCam > 0) {
			is_DisableEvent(dcx->hCam, IS_SET_EVENT_FRAME);
			is_ExitEvent(dcx->hCam, IS_SET_EVENT_FRAME);
#ifdef USE_RINGS
			is_DisableEvent(dcx->hCam, IS_SET_EVENT_SEQ);
			is_ExitEvent(dcx->hCam, IS_SET_EVENT_SEQ);
#endif		
			is_ExitCamera(dcx->hCam);
		}

		/* Release resources */
		if (dcx->FrameEvent != NULL) {
			CloseHandle(dcx->FrameEvent);
			dcx->FrameEvent = NULL;
		}
#ifdef USE_RINGS
		if (dcx->SequenceEvent != NULL) {
			CloseHandle(dcx->SequenceEvent);
			dcx->SequenceEvent = NULL;
		}
#endif		

		/* Free memory and disable dealing with multipe calls */
		main_dcx = NULL;
		free(dcx);
	}

	return;
}

/* ===========================================================================
-- Routine to generate a list of cameras on the system
-- 
-- Usage: int DCx_Enum_Camera_List(int *pcount, UC480_CAMERA_INFO **plist);
--
-- Inputs: pcount - pointer to variable to receive count of number available
--         plist  - pointer to variable to receive allocated description of list 
--
-- Output: *pcount - if != NULL, set to number of available cameras
--         *plist  - if != NULL, set to allocated list of camera info
--
-- Return: >= 0 - number of cameras on the system (count) 
--           -1 - failure getting info from the DCX interface
--
-- Notes: plist should be released by calling program when no longer needed
=========================================================================== */
int DCx_Enum_Camera_List(int *pcount, UC480_CAMERA_INFO **pinfo) {

	int i, rc, count;
	UC480_CAMERA_LIST *list = NULL;					/* Enumerated set of cameras and information */
	UC480_CAMERA_INFO *info;

	/* Initial return values */
	if (pcount != NULL) *pcount = 0;
	if (pinfo  != NULL) *pinfo = NULL;

	/* Determine how many cameras are connected and build the combo box with the information */
	if ( (rc = is_GetNumberOfCameras(&count)) != IS_SUCCESS) {
		printf("is_GetNumberOfCameras() failed (rc=%d)\n", rc); fflush(stdout);
		return -1;
	}

	if (count > 0) {
		list = calloc(1, sizeof(UC480_CAMERA_LIST) + count*sizeof(UC480_CAMERA_INFO));
		list->dwCount = count;
		if ( (rc = is_GetCameraList(list)) != IS_SUCCESS) {
			printf("Error getting camera list (rc=%d)\n", rc); fflush(stdout);
			free(list);
			return -2;
		}
		info = calloc(count, sizeof(*info));
		for (i=0; i<count; i++) info[i] = list->uci[i];
		free(list);
	}

	/* Return values */
	if (pcount != NULL) *pcount = count;
	if (pinfo  != NULL) *pinfo  = info;

	return count;
}

/* ===========================================================================
-- Routine to populate the camera selection combo box and identify best choice
-- 
-- Usage: int Fill_Camera_List_Control(HWND hdlg, DCX_WND_INFO *dcx, int *nAvailable, int *nFirstID);
--
-- Inputs: hdlg - handle to the window
--         dcx  - handle to the main information structure
--
-- Output: Fills in the global CameraCount and CameraList[] variables
--
-- Return: Number of cameras on the system (CameraCount)
=========================================================================== */
int Fill_Camera_List_Control(HWND hdlg, DCX_WND_INFO *dcx, int *nAvailable, int *nFirstID) {

	int i, nfree, nfirst;
	UC480_CAMERA_INFO *info;

	CB_INT_LIST *combolist;
	char szBuf[60];

	/* Prefill return values if we have points */
	nfree = nfirst = 0;

	/* Clear combo selection boxes and mark camera invalid now */
	ComboBoxClearList(hdlg, IDC_CAMERA_LIST);

	combolist = calloc(CameraCount, sizeof(*combolist));
	if (CameraCount > 0) {
		for (i=0; i<CameraCount; i++) {
			info = CameraDetails+i;
			combolist[i].value = info->dwCameraID;											/* Loading a camera is by CameraID */
			sprintf_s(szBuf, sizeof(szBuf), "[%d]:%s (%s)", info->dwCameraID, info->Model, info->SerNo); fflush(stdout);
			combolist[i].id = _strdup(szBuf);

			if (! info->dwInUse) nfree++;
			if (! info->dwInUse && nfirst <= 0) nfirst = info->dwCameraID;	/* First available camera */

			printf("  %d:  CameraID: %d  DeviceID: %d  SensorID: %d  InUse: %d S/N: %s  Model: %s  Status: %d\n", i, info->dwCameraID, info->dwDeviceID, info->dwSensorID, info->dwInUse, info->SerNo, info->Model, info->dwStatus); fflush(stdout);
		}

		/* Fill in the list box and enable it */
		ComboBoxFillIntList(hdlg, IDC_CAMERA_LIST, combolist, CameraCount);
		EnableDlgItem(hdlg, IDC_CAMERA_LIST, TRUE);

		/* Free the memory allocated to create the combo box */
		for (i=0; i<CameraCount; i++) if (combolist[i].id != NULL) free(combolist[i].id);
		free(combolist);
	}

	/* And either activate the available camera, or return an error */
	if (nAvailable != NULL) *nAvailable = nfree;
	if (nFirstID   != NULL) *nFirstID   = nfirst;

	printf("  CameraCount: %d\n", CameraCount); fflush(stdout);
	return CameraCount;
}

/* ===========================================================================
-- Selects and intializes a specified camera
=========================================================================== */
int DCx_Select_Camera(HWND hdlg, DCX_WND_INFO *dcx, int CameraID, int *nBestFormat) {

	int i, nsize, rc, n_formats;
	HCAM hCam;

	CAMINFO camInfo;							/* Local copies - will be copied to dcx */
	SENSORINFO SensorInfo;					/* Local copies - will be copied to dcx */
	IMAGE_FORMAT_LIST *ImageFormatList;	/* Local copy - will be copied to dcx */

	IMAGE_FORMAT_INFO *ImageFormatInfo;
	int ImageFormatID = 0;					/* What image resolution format to use */
	unsigned int width=0, height=0;

	CB_INT_LIST *list;
	char szBuf[60];

	/* In case of errors, set the best format option now */
	if (nBestFormat != NULL) *nBestFormat = 0;

	/* Disable any existing camera and free memory */
	if (dcx->hCam > 0) {
		rc = is_StopLiveVideo(dcx->hCam, IS_WAIT);
		dcx->LiveVideo = FALSE;
		rc = is_ExitCamera(dcx->hCam);					/* This also frees the image mem */
	}
	if (dcx->ImageFormatList != NULL) free(dcx->ImageFormatList);
	dcx->CameraID = 0;
	dcx->hCam = 0;
	dcx->ImageFormatID = 0;

/* Clear combo selection boxes and mark camera invalid now */
	ComboBoxClearList(hdlg, IDC_CAMERA_MODES);

	/* Verify that we have a valid camera */
	if (CameraID < 0) {
		MessageBox(NULL, "Request made to intialize an invalid CameraID", "No cameras available", MB_ICONERROR | MB_OK);
		printf("No cameras\n"); fflush(stdout);
		return 3;
	}

	/* Open the camera */
	hCam = CameraID;
	if ( (rc = is_InitCamera(&hCam, NULL)) != IS_SUCCESS) {
		if (rc == IS_ALL_DEVICES_BUSY || rc == IS_DEVICE_ALREADY_PAIRED) {
			MessageBox(NULL, "The device is not available ... looks like it is in use", "Device not available", MB_ICONERROR | MB_OK);
			printf("ERROR: Failed to initialize the camera (rc=%d)\n", rc); fflush(stdout);
		} else {
			MessageBox(NULL, "Unknown error attempting to initialize the requested camera", "Device not available", MB_ICONERROR | MB_OK);
			printf("ERROR: Failed to initialize the camera (rc=%d)\n", rc); fflush(stdout);
		}
		return 4;
	}

	/* Mark the main database with the camera handle */
	dcx->hCam = hCam;
	dcx->CameraID = CameraID;
	printf("  hCAM: %u  (for Camera %d)\n", hCam, CameraID); fflush(stdout);

	rc = is_ResetToDefault(hCam);
	rc = is_SetDisplayMode(hCam, IS_SET_DM_DIB);
	rc = is_GetCameraInfo(hCam, &camInfo);
	if (rc == IS_SUCCESS) {
		printf("  S/N: %s  Manufacturer: %s  Version: %s  Date: %s  CameraID: %d  Type: %s\n", 
				 camInfo.SerNo, camInfo.ID, camInfo.Version, camInfo.Date, camInfo.Select, 
				 camInfo.Type == IS_CAMERA_TYPE_UC480_USB_SE ? "IS_CAMERA_TYPE_UC480_USB_SE" : camInfo.Type == IS_CAMERA_TYPE_UC480_USB_LE ? "IS_CAMERA_TYPE_UC480_USB_LE" : camInfo.Type == IS_CAMERA_TYPE_UC480_USB3_CP ? "IS_CAMERA_TYPE_UC480_USB3_CP" : "Unknown"); 
		fflush(stdout);
	}
	dcx->CameraInfo = camInfo;
	
	rc = is_GetSensorInfo(hCam, &SensorInfo);
	if (rc == 0) {
		printf(" Sensor ID: %d  SensorName: %s  ColorMode: %s  MaxWidth: %d  MaxHeight: %d  Gain: %d (%d,%d,%d)  Shutter: %d  Pixel um: %d\n", 
				 SensorInfo.SensorID, SensorInfo.strSensorName,
				 SensorInfo.nColorMode == IS_COLORMODE_BAYER ? "Bayer" : SensorInfo.nColorMode == IS_COLORMODE_MONOCHROME ? "Monochrome" : "Unknown",
				 SensorInfo.nMaxWidth, SensorInfo.nMaxHeight,
				 SensorInfo.bMasterGain, SensorInfo.bRGain, SensorInfo.bGGain, SensorInfo.bBGain, SensorInfo.bGlobShutter, SensorInfo.wPixelSize);
		fflush(stdout);
	} else {
		MessageBox(NULL, "Camera selected will not report on sensor capabilities.  Don't know what to do", "No camera sensor info", MB_ICONERROR | MB_OK);
		printf("No sensor information reported\n"); fflush(stdout);
		return 5;
	}
	dcx->SensorInfo = SensorInfo;
	dcx->SensorIsColor = SensorInfo.nColorMode != IS_COLORMODE_MONOCHROME ;

	rc = is_EnableAutoExit(hCam, IS_DISABLE_AUTO_EXIT);
	rc = is_CameraStatus(hCam, IS_STANDBY_SUPPORTED, IS_GET_STATUS);
	if (rc == 1) rc = is_CameraStatus(hCam, IS_STANDBY, FALSE);

/* Enumerate the imaging modes */
	rc = is_ImageFormat(hCam, IMGFRMT_CMD_GET_NUM_ENTRIES, &n_formats, sizeof(n_formats)); 
	printf(" Number of image formats: %d\n", n_formats); fflush(stdout);
	if (n_formats <= 0) {
		MessageBox(NULL, "Camera selected appears to have no valid imaging formats.  Don't know what to do", "No image formats available", MB_ICONERROR | MB_OK);
		printf("No image formats were reported to exist\n"); fflush(stdout);
		return 5;
	}

	list = calloc(n_formats, sizeof(*list));
	nsize = sizeof(IMAGE_FORMAT_LIST) + sizeof(IMAGE_FORMAT_INFO)*n_formats;
	ImageFormatList = calloc(1, nsize);
	ImageFormatList->nSizeOfListEntry = sizeof(IMAGE_FORMAT_INFO);
	ImageFormatList->nNumListElements = n_formats;
	rc = is_ImageFormat(hCam, IMGFRMT_CMD_GET_LIST, ImageFormatList, nsize);

	if (rc != IS_SUCCESS) {
		MessageBox(NULL, "Failed to enumerate the imaging modes for this camera.  Don't know what to do", "No image formats available", MB_ICONERROR | MB_OK);
		free(ImageFormatList);
		return 5;
	}
	dcx->ImageFormatList = ImageFormatList;
	dcx->NumImageFormats = n_formats;

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
		if (ImageFormatInfo->nWidth*ImageFormatInfo->nHeight > width*height) {
			width = ImageFormatInfo->nWidth; 
			height = ImageFormatInfo->nHeight;
			ImageFormatID = ImageFormatInfo->nFormatID;
		}
	}

	ComboBoxFillIntList(hdlg, IDC_CAMERA_MODES, list, n_formats);
	if (ImageFormatID >= 0) ComboBoxSetByIntValue(hdlg, IDC_CAMERA_MODES, ImageFormatID);
	EnableDlgItem(hdlg, IDC_CAMERA_MODES, TRUE);
	EnableDlgItem(hdlg, IDB_CAMERA_DETAILS, TRUE);
	EnableDlgItem(hdlg, IDB_CAMERA_DISCONNECT, TRUE);

	/* Disable the RGB profile option when monochrome */
	EnableDlgItem  (hdlg, IDC_SHOW_RGB, dcx->SensorIsColor);
	SetDlgItemCheck(hdlg, IDC_SHOW_RGB, dcx->SensorIsColor);
	dcx->vert_r->visible = dcx->vert_g->visible = dcx->vert_b->visible = dcx->SensorIsColor;
	dcx->horz_r->visible = dcx->horz_g->visible = dcx->horz_b->visible = dcx->SensorIsColor;

	for (i=0; i<n_formats; i++) if (list[i].id != NULL) free(list[i].id);
	free(list);

	/* Return the recommended format (if we got one).  Return 0 with it, otherwise 4 */
	if (nBestFormat != NULL) *nBestFormat = ImageFormatID;
	return (ImageFormatID > 0) ? 0 : 4 ;
}

/* ===========================================================================
-- Reset based on existing loaded camera
=========================================================================== */
int Init_Connected_Camera(HWND hdlg, DCX_WND_INFO *dcx, int CameraID) {

	int i, rc, n_formats;
	HCAM hCam;

	IMAGE_FORMAT_LIST *ImageFormatList;	/* Local copy - will be copied to dcx */
	IMAGE_FORMAT_INFO *ImageFormatInfo;

	CB_INT_LIST *list;
	char szBuf[60];

/* Clear combo selection boxes and mark camera invalid now */
	if (dcx->ImageFormatList != NULL) free(dcx->ImageFormatList);
	ComboBoxClearList(hdlg, IDC_CAMERA_MODES);

	/* Verify that we have a valid camera */
	if (dcx->CameraID <= 0) {
		MessageBox(NULL, "Request made to re-intialize an invalid CameraID", "No cameras available", MB_ICONERROR | MB_OK);
		printf("No cameras\n"); fflush(stdout);
		return 3;
	}
	hCam = dcx->hCam;

/* Enumerate the imaging modes */
	rc = is_ImageFormat(hCam, IMGFRMT_CMD_GET_NUM_ENTRIES, &n_formats, sizeof(n_formats)); 
	printf(" Number of image formats: %d\n", n_formats); fflush(stdout);
	if (n_formats <= 0) {
		MessageBox(NULL, "Camera selected appears to have no valid imaging formats.  Don't know what to do", "No image formats available", MB_ICONERROR | MB_OK);
		printf("No image formats were reported to exist\n"); fflush(stdout);
		return 5;
	}

	ImageFormatList = dcx->ImageFormatList;
	n_formats = dcx->NumImageFormats;
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
	ComboBoxSetByIntValue(hdlg, IDC_CAMERA_MODES, dcx->ImageFormatID);
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
int DCx_Select_Resolution(HWND hdlg, DCX_WND_INFO *dcx, int ImageFormatID) {

	int i, rc;
	IS_LUT_ENABLED_STATE nLutEnabled;

	IMAGE_FORMAT_INFO *ImageFormatInfo;

	struct {
		int capabilities;	/* Max with capabilities */
		double dflt,		/* Default exposure */
		current,				/* Current exposure */
		min, max, inc;		/* Minimum, maximum and increment allowed */
	} ExposureParms;

	char *Image_Mem = NULL;
	double min,max,interval, rval1, rval2;

	/* Disable controls that may no longer be valid */
	for (i=0; CameraOffControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOffControls[i], FALSE);

	/* Look up the requested ImageFormatID in the dcx list of known formats */
	for (i=0; i<dcx->NumImageFormats; i++) {
		ImageFormatInfo = dcx->ImageFormatList->FormatInfo+i;
		if (ImageFormatInfo->nFormatID == ImageFormatID) break;
	}
	if (i >= dcx->NumImageFormats) {
		MessageBox(NULL, "The requested ImageFormatID did not show up in the camera's list\n", "Image resolution not available", MB_ICONERROR | MB_OK);
		return 7;
	}

/* Release current ring buffers at this point ... will try to restart again */
	ReleaseRingBuffers(dcx);

/* Set the resolution */
	if (is_ImageFormat(dcx->hCam, IMGFRMT_CMD_SET_FORMAT, &ImageFormatID, sizeof(ImageFormatID)) != IS_SUCCESS) {
		MessageBox(NULL, "Failed to initialize the requested resolution image format", "Select Resolution Failed", MB_ICONERROR | MB_OK);
		return 8;
	}

/* Set the aspect ratio and confirm */
	dcx->ImageFormatID = ImageFormatID;
	dcx->ImageFormatInfo = ImageFormatInfo;
	dcx->height = ImageFormatInfo->nHeight;
	dcx->width  = ImageFormatInfo->nWidth;
	dcx->Image_Aspect = 1.0 * dcx->width / dcx->height;
	printf("  Using format: %d  (%d x %d)\n", ImageFormatID, dcx->width, dcx->height); fflush(stdout);

/* Set the color model */
	rc = is_SetColorMode(dcx->hCam, dcx->SensorIsColor ? IS_CM_BGR8_PACKED : IS_CM_MONO8); 

/* Allocate new memory buffers */
	AllocRingBuffers(dcx, 0);
	
	/* Set trigger mode off (so autorun) */
	rc = is_SetExternalTrigger(dcx->hCam, IS_SET_TRIGGER_OFF); 

	rc = is_GetFrameTimeRange(dcx->hCam, &min, &max, &interval);
	printf("  Min: %g  Max: %g  Inc: %g\n", min, max, interval); fflush(stdout);

	rc = is_SetFrameRate(dcx->hCam, 5, &min);
	printf("  New FPS: %g\n", min); fflush(stdout);

	/* Disable any autogain on the sensor */
	/* Errors reported if set IS_SET_ENABLE_AUTO_SENSOR_GAIN, IS_SET_ENABLE_AUTO_SENSOR_SHUTTER, IS_SET_ENABLE_AUTO_SENSOR_WHITEBALANCE, IS_SET_ENABLE_AUTO_SENSOR_FRAMERATE */
	rval1 = rval2 = 0;
	rc = is_SetAutoParameter(dcx->hCam, IS_SET_ENABLE_AUTO_GAIN, &rval1, &rval2);
	rval1 = rval2 = 0;
	rc = is_SetAutoParameter(dcx->hCam, IS_SET_ENABLE_AUTO_SHUTTER, &rval1, &rval2);
	if (dcx->SensorInfo.nColorMode != IS_COLORMODE_MONOCHROME) {
		rval1 = rval2 = 0;
		rc = is_SetAutoParameter(dcx->hCam, IS_SET_ENABLE_AUTO_WHITEBALANCE, &rval1, &rval2);
	}
	rval1 = rval2 = 0;
	rc = is_SetAutoParameter(dcx->hCam, IS_SET_ENABLE_AUTO_FRAMERATE, &rval1, &rval2);

	/* Disable any look up tables */
	nLutEnabled = IS_LUT_DISABLED;
	rc = is_LUT(dcx->hCam, IS_LUT_CMD_SET_ENABLED, (void*) &nLutEnabled, sizeof(nLutEnabled));

	rc = is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_CAPS, &ExposureParms.capabilities, sizeof(ExposureParms.capabilities)); 
	if (rc == 0) {
		printf("  Exposure: %d  Fine_Increment: %d  Long_Exposure: %d  Dual_Exposure: %d\n", 
				 ExposureParms.capabilities & IS_EXPOSURE_CAP_EXPOSURE,	     ExposureParms.capabilities & IS_EXPOSURE_CAP_FINE_INCREMENT,
				 ExposureParms.capabilities & IS_EXPOSURE_CAP_LONG_EXPOSURE, ExposureParms.capabilities & IS_EXPOSURE_CAP_DUAL_EXPOSURE);
		fflush(stdout);
	}
	rc = is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE_DEFAULT,   &ExposureParms.dflt, sizeof(ExposureParms.dflt));	
	rc = is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE,           &ExposureParms.current, sizeof(ExposureParms.current));	
	rc = is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE,     &ExposureParms.min, 3*sizeof(ExposureParms.min));			
	printf("  Default: %g  Current: %g   Min: %g   Max: %g   Inc: %g\n", ExposureParms.dflt, ExposureParms.current, ExposureParms.min, ExposureParms.max, ExposureParms.inc); fflush(stdout);

	/* Do much now the same as if we already had the selected resolution (avoid duplicate code) */
	Init_Known_Resolution(hdlg, dcx, dcx->hCam);
	
	/* Start the events to actually render the images */
	is_InitEvent(dcx->hCam, dcx->FrameEvent, IS_SET_EVENT_FRAME);
	is_EnableEvent(dcx->hCam, IS_SET_EVENT_FRAME);
#ifdef USE_RINGS
	is_InitEvent(dcx->hCam, dcx->SequenceEvent, IS_SET_EVENT_SEQ);
	is_EnableEvent(dcx->hCam, IS_SET_EVENT_SEQ);
#endif

	/* For some reason, have to stop the live video again to avoid error messages */
	is_StopLiveVideo(dcx->hCam, IS_WAIT);
	dcx->LiveVideo = FALSE;
	if (GetDlgItemCheck(hdlg, IDB_LIVE)) {
		is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
		dcx->LiveVideo = TRUE;
		dcx->nLast = dcx->nValid = 0;
		EnableDlgItem(hdlg, IDB_CAPTURE, FALSE);
	} else {
		SetDlgItemCheck(hdlg, IDB_LIVE, FALSE);
		EnableDlgItem(hdlg, IDB_CAPTURE, TRUE);
	}

	return 0;
}

/* ===========================================================================
-- Selects and intializes a specified resolution mode
--
-- Notes:
--   (1) Called from DCx_Select_Resolution after a resolution is selected
========================================================================== */
int Init_Known_Resolution(HWND hdlg, DCX_WND_INFO *dcx, HCAM hCam) {

	int i, rc;
	double rval;

	/* Disable controls that may no longer be valid (will be duplicated when called by DCx_Select_Resolution() */
	for (i=0; CameraOffControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOffControls[i], FALSE);

/* Determine the available color correction modes and enable the radio buttons */
	rval = 0.0;
	rc = is_SetColorCorrection(hCam, IS_GET_SUPPORTED_CCOR_MODE, &rval);
	EnableDlgItem(hdlg, IDR_COLOR_DISABLE, TRUE);
	EnableDlgItem(hdlg, IDR_COLOR_ENABLE,         rc &  IS_CCOR_ENABLE_NORMAL);
	EnableDlgItem(hdlg, IDR_COLOR_BG40,           rc &                          IS_CCOR_ENABLE_BG40_ENHANCED);
	EnableDlgItem(hdlg, IDR_COLOR_HQ,             rc &                                                         IS_CCOR_ENABLE_HQ_ENHANCED);
	EnableDlgItem(hdlg, IDR_COLOR_AUTO_IR,        rc & (                        IS_CCOR_ENABLE_BG40_ENHANCED | IS_CCOR_ENABLE_HQ_ENHANCED));
	EnableDlgItem(hdlg, IDV_COLOR_CORRECT_FACTOR, rc & (IS_CCOR_ENABLE_NORMAL | IS_CCOR_ENABLE_BG40_ENHANCED | IS_CCOR_ENABLE_HQ_ENHANCED));

/* Determine which of the gains can be used */
	if (dcx->SensorInfo.bMasterGain) {
		EnableDlgItem(hdlg, IDS_MASTER_GAIN, TRUE);
		EnableDlgItem(hdlg, IDV_MASTER_GAIN, TRUE);
	}
	if (dcx->SensorInfo.bRGain) {
		EnableDlgItem(hdlg, IDS_RED_GAIN, TRUE);
		EnableDlgItem(hdlg, IDV_RED_GAIN, TRUE);
	}
	if (dcx->SensorInfo.bGGain) {
		EnableDlgItem(hdlg, IDS_GREEN_GAIN, TRUE);
		EnableDlgItem(hdlg, IDV_GREEN_GAIN, TRUE);
	}
	if (dcx->SensorInfo.bBGain) {
		EnableDlgItem(hdlg, IDS_BLUE_GAIN, TRUE);
		EnableDlgItem(hdlg, IDV_BLUE_GAIN, TRUE);
	}

	/* Enable all of the controls now */
	for (i=0; CameraOnControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOnControls[i], TRUE);

	SendMessage(hdlg, WMP_SHOW_COLOR_CORRECT, 0, 0);
	SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);
	SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
	SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
	SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);

	return 0;
}

/* ===========================================================================
=========================================================================== */
#define	TIMER_FRAME_RATE_UPDATE				(1)

BOOL CALLBACK DCxDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "DCxDlgProc";

	BOOL rcode;
	int i, ineed, nfree, nfirst, nformat, rc;
	int wID, wNotifyCode;
	char szBuf[256];
	int nGamma;

	double fps, rval;
	POINT point;
	RECT rect;

	IMAGE_FILE_PARAMS ImageParams;

	/* List of controls which will respond to <ENTER> with a WM_NEXTDLGCTL message */
	/* Make <ENTER> equivalent to losing focus */
	static int DfltEnterList[] = {					
		IDV_EXPOSURE_TIME, IDV_FRAME_RATE, IDV_GAMMA,IDT_CURSOR_X_PIXEL, IDT_CURSOR_Y_PIXEL,
		IDV_RING_SIZE,
		ID_NULL };

	DCX_WND_INFO *dcx;
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
		dcx = (DCX_WND_INFO *) GetWindowLongPtr(hdlg, GWLP_USERDATA);
	}

/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			printf("Initializing DCx Camera Interface window\n"); fflush(stdout);
			DlgCenterWindow(hdlg);

			/* Since may not actually be the call, look up this applications instance */
			hInstance = (HINSTANCE) GetWindowLongPtr(hdlg, GWLP_HINSTANCE);

			sprintf_s(szBuf, sizeof(szBuf), "Version 1.0 [ %s ]", LinkDate);
			SetDlgItemText(hdlg, IDT_COMPILE_VERSION, szBuf);

			/* Immediately register a closeout procedure (since this dialog box may open/close many times */
			atexit(DCx_Final_Closeout);

			/* Disable all controls for now */
			EnableDlgItem(hdlg, IDC_CAMERA_LIST, FALSE);				/* Will be enabled when scanning cameras */
			for (i=0; AllCameraControls[i]!=ID_NULL; i++) EnableDlgItem(hdlg, AllCameraControls[i], FALSE);

			/* Create the information block and save it within this hdlg */
			if (main_dcx == NULL) main_dcx = (DCX_WND_INFO *) calloc(1, sizeof(DCX_WND_INFO));
			dcx = main_dcx;

			SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR) dcx);
			dcx->main_hdlg = hdlg;								/* Have this available for other use */
			DCx_main_hdlg = hdlg;								/* Let the outside world know also */
			dcx->thumbnail = GetDlgItem(hdlg, IDC_DISPLAY);
			if (dcx->x_image_target == 0 && dcx->y_image_target == 0) dcx->x_image_target = dcx->y_image_target = 0.5;

			/* Initialize buffers */
#ifdef USE_RINGS													/* Value is default number to use */
			dcx->nRing = USE_RINGS;
			SetDlgItemInt(hdlg, IDV_RING_SIZE, dcx->nRing, FALSE);
			SetDlgItemInt(hdlg, IDT_FRAME_COUNT, 0, FALSE);
			SetDlgItemInt(hdlg, IDT_FRAME_VALID, 0, FALSE);
#else
			SetDlgItemInt(hdlg, IDV_RING_SIZE, 1, FALSE);
			SetDlgItemInt(hdlg, IDT_FRAME_COUNT, 0, FALSE);
			SetDlgItemInt(hdlg, IDT_FRAME_VALID, 0, FALSE);
			EnableDlgItem(hdlg, IDV_RING_SIZE, FALSE);
			EnableDlgItem(hdlg, IDB_BURST, FALSE);
#endif

			/* Now, initialize the rest of the windows (will fill in parts of dcx */
			InitializeScrollBars(hdlg, dcx);
			InitializeHistogramCurves(hdlg, dcx);
			SetDlgItemCheck(hdlg, IDC_SHOW_INTENSITY, TRUE);
			SetDlgItemCheck(hdlg, IDC_SHOW_RGB, TRUE);
			SetDlgItemCheck(hdlg, IDC_FULL_WIDTH_CURSOR, dcx->full_width_cursor);
			SetRadioButton(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS, ExposureList[0].wID);
			SetDlgItemText(hdlg, IDT_MIN_EXPOSURE, ExposureList[0].str_min);
			SetDlgItemText(hdlg, IDT_MID_EXPOSURE, ExposureList[0].str_mid);
			SetDlgItemText(hdlg, IDT_MAX_EXPOSURE, ExposureList[0].str_max);

			/* Create the event for rendering and start the thread */
			if (dcx->FrameEvent == NULL) dcx->FrameEvent = CreateEvent(NULL, FALSE, FALSE, FALSE);
#ifdef USE_RINGS
			if (dcx->SequenceEvent == NULL) dcx->SequenceEvent = CreateEvent(NULL, FALSE, FALSE, FALSE);
#endif

			/* Initialize the DCx driver and build list of existing cameras */
			if (CameraDetails != NULL) { free(CameraDetails); CameraDetails = NULL; }		/* Free previously used storage */
			DCx_Init_Driver();													/* Safe to call multiple times */

			/* Set the error reporting mode for the driver */
			is_SetErrorReport(0, dcx->EnableErrorReports ? IS_ENABLE_ERR_REP : IS_DISABLE_ERR_REP);
			SetDlgItemCheck(hdlg, IDC_ENABLE_DCX_ERRORS, dcx->EnableErrorReports);

			DCx_Enum_Camera_List(&CameraCount, &CameraDetails);
			Fill_Camera_List_Control(hdlg, dcx, &nfree, &nfirst);

			/* Either select the one we are coming back to, or maybe choose one (if only one available) */
			if (dcx->hCam != 0) {
				printf("Re-initializing to camera ID: %d / hCam: %d\n", dcx->CameraID, dcx->hCam); fflush(stdout);
				rc = 0;
				if ( (rc = ComboBoxSetByIntValue(hdlg, IDC_CAMERA_LIST, dcx->CameraID)) == 0) {		/* Make sure camera is still there */
					if ( (rc = Init_Connected_Camera(hdlg, dcx, dcx->CameraID)) == 0) {
						if ( (rc = ComboBoxSetByIntValue(hdlg, IDC_CAMERA_MODES, dcx->ImageFormatID)) == 0) {
							if ( (rc = Init_Known_Resolution(hdlg, dcx, dcx->hCam)) != 0) {
								printf("failed to re-initialize the resolution (rc=%d)\n", rc); fflush(stdout);
							}
						}
					}
				}
				if (rc != 0) {
					printf("ERROR: Something went wrong in trying to reconnect to camera (rc=%d)\n", rc); fflush(stdout);
					SendMessage(hdlg, IDB_CAMERA_DISCONNECT, 0, 0);
				}
			}

			/* If there is no camera active, and only one possible, go ahead and initialize it */
			if (dcx->hCam == 0 && nfree == 1) {													/* If only one free, then open it */
				if ( DCx_Select_Camera(hdlg, dcx, nfirst, &nformat) == 0) {
					ComboBoxSetByIntValue(hdlg, IDC_CAMERA_LIST, nfirst);					/* We have it selected now */
					if ( DCx_Select_Resolution(hdlg, dcx, nformat) == 0) {
						ComboBoxSetByIntValue(hdlg, IDC_CAMERA_MODES, nformat);
						is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
						dcx->nLast = dcx->nValid = 0;
						dcx->LiveVideo = TRUE;
						SetDlgItemCheck(hdlg, IDB_LIVE, TRUE);
						EnableDlgItem(hdlg, IDB_CAPTURE, FALSE);
					}
				}
			}

			/* Finally, start a thread to monitor events and render the image */
			if (! dcx->ProcessNewImageThreadActive) {
				printf("Starting Image Rendering Thread\n"); fflush(stdout);
				_beginthread(ProcessNewImage, 0, NULL);
				dcx->ProcessNewImageThreadActive = TRUE;
			}
#ifdef USE_RINGS
			if (! dcx->SequenceThreadActive) {
				printf("Starting Sequence Thread\n"); fflush(stdout);
				_beginthread(SequenceThread, 0, NULL);
				dcx->SequenceThreadActive = TRUE;
			}
#endif

			/* Update the cursor position to initial value (probably 0) */
			SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);

			SetTimer(hdlg, TIMER_FRAME_RATE_UPDATE, 1000, NULL);									/* Redraw at roughtly 1 Hz rate */

			/* Initialize the DCx TCP server for remote image requests */
			Init_DCx_Server();

			rcode = TRUE; break;

		case WM_CLOSE:
			
			printf("WM_CLOSE received ..."); fflush(stdout);
			dcx->main_hdlg = NULL;						/* Mark this window as invalid so can restart */
			dcx->thumbnail = NULL;						/* Eliminate the thumbnail options */
			DCx_main_hdlg  = NULL;						/* And is gone for the outside world */
			SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_CLEAR, (WPARAM) 0,    (LPARAM) 0);

			/* Give a few 100 ms for any rendering to complete before dumping memory */
			Sleep(200);

			FreeCurve(dcx->red_hist);   dcx->red_hist = NULL;
			FreeCurve(dcx->green_hist); dcx->green_hist = NULL;
			FreeCurve(dcx->blue_hist);  dcx->blue_hist = NULL;
			FreeCurve(dcx->vert_w);		 dcx->vert_w = NULL;
			FreeCurve(dcx->vert_r);		 dcx->vert_r = NULL;
			FreeCurve(dcx->vert_g);		 dcx->vert_g = NULL;
			FreeCurve(dcx->vert_b);		 dcx->vert_b = NULL;
			FreeCurve(dcx->horz_w);		 dcx->horz_w = NULL;
			FreeCurve(dcx->horz_r);		 dcx->horz_r = NULL;
			FreeCurve(dcx->horz_b);		 dcx->horz_b = NULL;
			FreeCurve(dcx->horz_g);		 dcx->horz_g = NULL;

			printf(" calling EndDialog ..."); fflush(stdout);
			EndDialog(hdlg,0);
			printf(" returning\n"); fflush(stdout);
			rcode = TRUE; break;

			/* Need to release memory associated with the curves */
			EndDialog(hdlg,0);
			rcode = TRUE; break;

		case WM_TIMER:
			if (wParam == TIMER_FRAME_RATE_UPDATE) {
				fps = 0.0;
				if (dcx->hCam > 0) is_GetFramesPerSecond(dcx->hCam, &fps);
				SetDlgItemDouble(hdlg, IDT_FRAMERATE, "%.2f", fps);
			}
			rcode = TRUE; break;

		case WM_RBUTTONDOWN:
			GetCursorPos((LPPOINT) &point);
			GetWindowRect(GetDlgItem(hdlg, IDC_DISPLAY), &rect);
			if (point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom) {
				dcx->x_image_target = (1.0*point.x-rect.left) / (rect.right-rect.left);
				dcx->y_image_target = (1.0*point.y-rect.top)  / (rect.bottom-rect.top);
			}
			SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
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
			wID = ID_NULL;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_MASTER_GAIN)) wID = IDS_MASTER_GAIN;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_RED_GAIN))    wID = IDS_RED_GAIN;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_GREEN_GAIN))  wID = IDS_GREEN_GAIN;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_BLUE_GAIN))   wID = IDS_BLUE_GAIN;
			if (wID != ID_NULL) {
				int ipos;
				ipos = -99999;
				switch (LOWORD(wParam)) {
					case SB_THUMBPOSITION:									/* Moved manually */
						ipos = HIWORD(wParam); break;
					case SB_THUMBTRACK:
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
					ipos = 100-ipos;
					if (wID == IDS_MASTER_GAIN) is_SetHardwareGain(dcx->hCam, ipos, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);
					if (wID == IDS_RED_GAIN)    is_SetHardwareGain(dcx->hCam, IS_IGNORE_PARAMETER, ipos, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);
					if (wID == IDS_GREEN_GAIN)  is_SetHardwareGain(dcx->hCam, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, ipos, IS_IGNORE_PARAMETER);
					if (wID == IDS_BLUE_GAIN)   is_SetHardwareGain(dcx->hCam, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, ipos);
					SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
				}
			}
			rcode = TRUE; break;

		case WM_HSCROLL:
			wID = ID_NULL;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_FRAME_RATE))    wID = IDS_FRAME_RATE;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_EXPOSURE_TIME)) wID = IDS_EXPOSURE_TIME;
			if ((HWND) lParam == GetDlgItem(hdlg, IDS_GAMMA))			 wID = IDS_GAMMA;
			if (wID != ID_NULL) {
				int ipos;
				ipos = -99999;
				switch (LOWORD(wParam)) {
					case SB_THUMBPOSITION:									/* Moved manually */
						ipos = HIWORD(wParam); break;
					case SB_THUMBTRACK:
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
							fps = ipos / 10.0;										/* Requested frame rate as a floating point number */
							SendMessage(hdlg, WMP_SET_FRAMERATE, (int) (100*fps+0.5), 0);
							break;
						case IDS_EXPOSURE_TIME:										/* Just send the value of the scroll bar */
							i = GetRadioButtonIndex(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS);
							rval = ExposureList[i].exp_min * pow(10.0,ipos/100.0);			/* Scale */
							DCx_Set_Exposure(dcx, rval, TRUE, hdlg);
							break;
						case IDS_GAMMA:
							SendMessage(hdlg, WMP_SET_GAMMA, ipos, 0);
							break;
					}
				}
			}
			rcode = TRUE; break;

		case WMP_SHOW_CURSOR_POSN:
			SetDlgItemInt(hdlg, IDT_CURSOR_X_PIXEL, nint((dcx->x_image_target-0.5)*dcx->width),  TRUE);
			SetDlgItemInt(hdlg, IDT_CURSOR_Y_PIXEL, nint((0.5-dcx->y_image_target)*dcx->height), TRUE);	/* Remember Y is top down */
			rc = TRUE; break;

		case WMP_SET_GAMMA:
			nGamma = (int) wParam;
			is_Gamma(dcx->hCam, IS_GAMMA_CMD_SET, &nGamma, sizeof(nGamma));
			SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETPOS, TRUE, nGamma);
			SetDlgItemDouble(hdlg, IDV_GAMMA, "%.2f", nGamma/100.0);
			rcode = TRUE; break;

		case WMP_SHOW_GAMMA:
			is_Gamma(dcx->hCam, IS_GAMMA_CMD_GET, &nGamma, sizeof(nGamma));
			SendDlgItemMessage(hdlg, IDS_GAMMA, TBM_SETPOS, TRUE, nGamma);
			SetDlgItemDouble(hdlg, IDV_GAMMA, "%.2f", nGamma/100.0);
			rcode = TRUE; break;

		case WMP_SHOW_COLOR_CORRECT:
			rval = 0;
			rc = is_SetColorCorrection(dcx->hCam, IS_GET_CCOR_MODE, &rval);
			for (i=0; i<N_COLOR_MODES; i++) {
				if (rc == ColorCorrectionModes[i]) break;
			}
			if (i >= N_COLOR_MODES) i = 0;
			SetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR, i);
			SetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR, "%.2f", rval);
			rcode = TRUE; break;

		case WMP_SHOW_GAINS:
			rc = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
			SetDlgItemInt(hdlg, IDV_MASTER_GAIN, rc, FALSE);
			SendDlgItemMessage(hdlg, IDS_MASTER_GAIN, TBM_SETPOS, TRUE, 100-rc);

			rc = (dcx->SensorInfo.bRGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_RED_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
			SetDlgItemInt(hdlg, IDV_RED_GAIN, rc, FALSE);
			SendDlgItemMessage(hdlg, IDS_RED_GAIN, TBM_SETPOS, TRUE, 100-rc);

			rc = (dcx->SensorInfo.bGGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_GREEN_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
			SetDlgItemInt(hdlg, IDV_GREEN_GAIN, rc, FALSE);
			SendDlgItemMessage(hdlg, IDS_GREEN_GAIN, TBM_SETPOS, TRUE, 100-rc);

			rc = (dcx->SensorInfo.bBGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_BLUE_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
			SetDlgItemInt(hdlg, IDV_BLUE_GAIN, rc, FALSE);
			SendDlgItemMessage(hdlg, IDS_BLUE_GAIN, TBM_SETPOS, TRUE, 100-rc);
			rcode = TRUE; break;

		case WMP_SET_FRAMERATE:										/* From IDV_FRAME_RATE and on ENTER of the same */
			fps = ((int) wParam) / 100.0;							/* Passed value is 100* desired rate */
			if (fps < MIN_FPS) fps = MIN_FPS;
			if (fps > MAX_FPS) fps = MAX_FPS;
			is_SetFrameRate(dcx->hCam, fps, &fps);				/* Set and query simultaneously */
			SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETPOS, TRUE, (int) (10.0*fps));
			SetDlgItemDouble(hdlg, IDV_FRAME_RATE, "%.2f", fps);
			SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
			rcode = TRUE; break;

		case WMP_SHOW_FRAMERATE:
			is_SetFrameRate(dcx->hCam, IS_GET_FRAMERATE, &fps);
			SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETPOS, TRUE, (int) (10.0*fps));
			SetDlgItemDouble(hdlg, IDV_FRAME_RATE, "%.2f", fps);
			rcode = TRUE; break;

		case WMP_SHOW_EXPOSURE:
			is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &rval, sizeof(rval));
			if (rval < 0.010) rval = 0.010;										/* Assume 10 us minimum exposure */
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

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			rcode = FALSE;												/* Assume we don't process */
			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
					hwndTest = GetFocus();							/* Just see if we use to change focus */
					for (hptr=DfltEnterList; *hptr!=ID_NULL; hptr++) {
						if (GetDlgItem(hdlg, *hptr) == hwndTest) {
							if (*hptr == IDV_FRAME_RATE) {
								SendMessage(hdlg, WMP_SET_FRAMERATE, (int) (100*GetDlgItemDouble(hdlg, IDV_FRAME_RATE)+0.5), 0);
							} else if (*hptr == IDV_EXPOSURE_TIME) {
								DCx_Set_Exposure(dcx, GetDlgItemDouble(hdlg, IDV_EXPOSURE_TIME), TRUE, hdlg);
							} else if (*hptr == IDV_GAMMA) {
								SendMessage(hdlg, WMP_SET_GAMMA, (int) (100*GetDlgItemDouble(hdlg, IDV_GAMMA)+0.5), 0);
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
					dcx->EnableErrorReports = GetDlgItemCheck(hdlg, wID);
					is_SetErrorReport(0, dcx->EnableErrorReports ? IS_ENABLE_ERR_REP : IS_DISABLE_ERR_REP);
					rcode = TRUE; break;
					
				case IDB_AUTO_EXPOSURE:
					_beginthread(AutoExposureThread, 0, dcx);
					rcode = TRUE; break;
					
				case IDB_RESET_CURSOR:
					dcx->x_image_target = dcx->y_image_target = 0.5;
					SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
					rcode = TRUE; break;
					
				case IDC_SHOW_INTENSITY:
					dcx->vert_w->visible = dcx->horz_w->visible = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;

				case IDC_SHOW_RGB:
					dcx->vert_r->visible = dcx->vert_g->visible = dcx->vert_b->visible = GetDlgItemCheck(hdlg, wID);
					dcx->horz_r->visible = dcx->horz_g->visible = dcx->horz_b->visible = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;
					
				case IDC_FULL_WIDTH_CURSOR:
					dcx->full_width_cursor = GetDlgItemCheck(hdlg, wID);
					rcode = TRUE; break;
							
				case IDB_CAMERA_DISCONNECT:
					if (dcx->hCam > 0) {
						ReleaseRingBuffers(dcx);
						is_DisableEvent(dcx->hCam, IS_SET_EVENT_FRAME);
						is_ExitEvent(dcx->hCam, IS_SET_EVENT_FRAME);
						is_ExitCamera(dcx->hCam);
						dcx->hCam = 0;
					}
					for (i=0; CameraOffControls[i] != ID_NULL; i++) EnableDlgItem(hdlg, CameraOffControls[i], FALSE);
					ComboBoxClearSelection(hdlg, IDC_CAMERA_LIST);		/* Should be "unselect" */
					ComboBoxClearSelection(hdlg, IDC_CAMERA_MODES);		/* Should be "unselect" */
					EnableDlgItem(hdlg, IDB_CAMERA_DETAILS, FALSE);
					EnableDlgItem(hdlg, IDB_CAMERA_DISCONNECT, FALSE);
					rcode = TRUE; break;
					
				case IDB_CAMERA_DETAILS:
					_beginthread(show_camera_info_thread, 0, NULL);
					rcode = TRUE; break;

				case IDC_CAMERA_LIST:
					if (wNotifyCode == CBN_SELENDOK) {
						if (DCx_Select_Camera(hdlg, dcx, ComboBoxGetIntValue(hdlg, wID), &nformat) == 0) {
							if (DCx_Select_Resolution(hdlg, dcx, nformat) == 0) {
								is_StopLiveVideo(dcx->hCam, IS_WAIT);
								is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
								dcx->LiveVideo = TRUE;
								dcx->nLast = dcx->nValid = 0;
								SetDlgItemCheck(hdlg, IDB_LIVE, TRUE);
								EnableDlgItem(hdlg, IDB_CAPTURE, FALSE);
								SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
							}
						}
					}
					rcode = TRUE; break;

				case IDC_CAMERA_MODES:
					if (wNotifyCode == CBN_SELENDOK && dcx->hCam > 0) {
						if (dcx->LiveVideo) is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);
						dcx->LiveVideo = FALSE;
						SetDlgItemCheck(hdlg, IDB_LIVE, FALSE);
						EnableDlgItem(hdlg, IDB_CAPTURE, FALSE);
						is_StopLiveVideo(dcx->hCam, IS_WAIT);

						if (DCx_Select_Resolution(hdlg, dcx, ComboBoxGetIntValue(hdlg, wID)) == 0) {
							is_StopLiveVideo(dcx->hCam, IS_WAIT);
							is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
							dcx->LiveVideo = TRUE;
							dcx->nLast = dcx->nValid = 0;
							SetDlgItemCheck(hdlg, IDB_LIVE, TRUE);
							EnableDlgItem(hdlg, IDB_CAPTURE, FALSE);
							SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
						} else {
							ComboBoxClearSelection(hdlg, IDC_CAMERA_MODES);
						}
					}
					rcode = TRUE; break;
					
				case IDB_UNDOCK:
					if (! IsWindow(float_image_hwnd)) _beginthread(start_image_window, 0, NULL);
					rcode = TRUE; break;

				case IDB_LOAD_PARAMETERS:
					is_ParameterSet(dcx->hCam, IS_PARAMETERSET_CMD_LOAD_FILE, NULL, 0);
					SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);
					SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
					SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
					SendMessage(hdlg, WMP_SHOW_COLOR_CORRECT, 0, 0);
					SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
					rcode = TRUE; break;

				case IDB_SAVE_PARAMETERS:
					is_ParameterSet(dcx->hCam, IS_PARAMETERSET_CMD_SAVE_FILE, NULL, 0);
					rcode = TRUE; break;

				case IDV_FRAME_RATE:
					if (wNotifyCode == EN_KILLFOCUS) {
						SendMessage(hdlg, WMP_SET_FRAMERATE, (int) (100*GetDlgItemDouble(hdlg, wID)+0.5), 0);
					}
					rcode = TRUE; break;
					
				case IDV_EXPOSURE_TIME:
					if (wNotifyCode == EN_KILLFOCUS) DCx_Set_Exposure(dcx, GetDlgItemDouble(hdlg, wID), TRUE, hdlg);
					rcode = TRUE; break;

				case IDV_GAMMA:
					if (wNotifyCode == EN_KILLFOCUS) {
						SendMessage(hdlg, WMP_SET_GAMMA, (int) (100*GetDlgItemDouble(hdlg, wID)+0.5), 0);
					}
					rcode = TRUE; break;

				case IDB_GAMMA_NEUTRAL:
					SendMessage(hdlg, WMP_SET_GAMMA, 100, 0);
					rcode = TRUE; break;

				case IDR_EXPOSURE_100US:
				case IDR_EXPOSURE_1MS:
				case IDR_EXPOSURE_10MS:
				case IDR_EXPOSURE_100MS:
					i = GetRadioButtonIndex(hdlg, IDR_EXPOSURE_100US, IDR_EXPOSURE_100MS);
					SetDlgItemText(hdlg, IDT_MIN_EXPOSURE, ExposureList[i].str_min);
					SetDlgItemText(hdlg, IDT_MID_EXPOSURE, ExposureList[i].str_mid);
					SetDlgItemText(hdlg, IDT_MAX_EXPOSURE, ExposureList[i].str_max);
					is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &rval, sizeof(rval));					
					if (rval < ExposureList[i].exp_min) {
						DCx_Set_Exposure(dcx, ExposureList[i].exp_min, TRUE, hdlg);
					} else if (rval > ExposureList[i].exp_max) {
						DCx_Set_Exposure(dcx, ExposureList[i].exp_max, TRUE, hdlg);
					} else {
						SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
					}
					rcode = TRUE; break;

					/* Only time I get a message is when it is clicked, so just take for granted */
				case IDR_COLOR_DISABLE:
				case IDR_COLOR_ENABLE:
				case IDR_COLOR_BG40:
				case IDR_COLOR_HQ:
				case IDR_COLOR_AUTO_IR:
					i = GetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR);
					if (i >= 0 && i < N_COLOR_MODES) {
						rval = GetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR);
						is_SetColorCorrection(dcx->hCam, ColorCorrectionModes[i], &rval);
					}
					rval = 0;
					rc = is_SetColorCorrection(dcx->hCam, IS_GET_CCOR_MODE, &rval);
					rcode = TRUE; break;
					
				case IDV_COLOR_CORRECT_FACTOR:
					if (wNotifyCode == EN_KILLFOCUS) {
						static double last_value = -10.0;
						rval = GetDlgItemDouble(hdlg, IDV_COLOR_CORRECT_FACTOR);
						if (rval < 0.0) rval = 0.0;
						if (rval > 1.0) rval = 1.0;
						if (rval != last_value) {
							i = GetRadioButtonIndex(hdlg, IDR_COLOR_DISABLE, IDR_COLOR_AUTO_IR);
							if (i > 0 && i < N_COLOR_MODES) {
								is_SetColorCorrection(dcx->hCam, ColorCorrectionModes[i], &rval);
								last_value = rval;
							}
						}
						SetDlgItemDouble(hdlg, wID, "%.2f", rval);
						rval = 0;
						rc = is_SetColorCorrection(dcx->hCam, IS_GET_CCOR_MODE, &rval);
					}
					rcode = TRUE; break;
					
				case IDV_MASTER_GAIN:
				case IDV_RED_GAIN:
				case IDV_GREEN_GAIN:
				case IDV_BLUE_GAIN:
					if (wNotifyCode == EN_KILLFOCUS) {
						rc = GetDlgItemIntEx(hdlg, wID);
						if (rc < 0) rc = 0;
						if (rc > 100) rc = 100;
						if (wID == IDV_MASTER_GAIN) is_SetHardwareGain(dcx->hCam, rc, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);
						if (wID == IDV_RED_GAIN)    is_SetHardwareGain(dcx->hCam, IS_IGNORE_PARAMETER, rc, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER);
						if (wID == IDV_GREEN_GAIN)  is_SetHardwareGain(dcx->hCam, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, rc, IS_IGNORE_PARAMETER);
						if (wID == IDV_BLUE_GAIN)   is_SetHardwareGain(dcx->hCam, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, rc);
						SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
					}
					rcode = TRUE; break;

				case IDT_CURSOR_X_PIXEL:
					if (wNotifyCode == EN_KILLFOCUS) {
						rval = ((double) GetDlgItemIntEx(hdlg, wID)) / dcx->width + 0.5;
						dcx->x_image_target = (rval < 0.0) ? 0.0 : (rval > 1.0) ? 1.0 : rval ;
						SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
					}
					rcode = TRUE; break;
				case IDT_CURSOR_Y_PIXEL:
					if (wNotifyCode == EN_KILLFOCUS) {
						rval = 0.5 - ((double) GetDlgItemIntEx(hdlg, wID)) / dcx->height;
						dcx->y_image_target = (rval < 0.0) ? 0.0 : (rval > 1.0) ? 1.0 : rval ;
						SendMessage(hdlg, WMP_SHOW_CURSOR_POSN, 0, 0);
					}
					rcode = TRUE; break;

				case IDV_RING_SIZE:
#ifdef USE_RINGS
					if (wNotifyCode == EN_KILLFOCUS) {
						AllocRingBuffers(dcx, GetDlgItemIntEx(hdlg, IDV_RING_SIZE));
						SetDlgItemInt(hdlg, IDV_RING_SIZE, dcx->nRing, FALSE);
					}
#endif
					rcode = TRUE; break;

				case IDB_LIVE:
					if (GetDlgItemCheck(hdlg, IDB_LIVE)) {
						is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
						dcx->LiveVideo = TRUE;
						dcx->nLast = dcx->nValid = 0;
						EnableDlgItem(hdlg, IDB_CAPTURE, FALSE);
					} else {
						is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);
						dcx->LiveVideo = FALSE;
						EnableDlgItem(hdlg, IDB_CAPTURE, TRUE);
					}
					rcode = TRUE; break;

				case IDB_CAPTURE:
					is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);
					dcx->LiveVideo = FALSE;
					rcode = TRUE; break;

				case IDB_SAVE:
					if (dcx->LiveVideo) rc = is_FreezeVideo(dcx->hCam, IS_WAIT);
					dcx->LiveVideo = FALSE;

					ImageParams.pwchFileName = NULL;		/* fname; */
					ImageParams.pnImageID    = NULL;	
					ImageParams.ppcImageMem  = NULL;
					ImageParams.nQuality     = 0;
					ImageParams.nFileType = IS_IMG_BMP;
					rc = is_ImageFile(dcx->hCam, IS_IMAGE_FILE_CMD_SAVE, &ImageParams, sizeof(ImageParams));

					if (GetDlgItemCheck(hdlg, IDB_LIVE)) {
						is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
						dcx->nLast = dcx->nValid = 0;
						dcx->LiveVideo = TRUE;
					}
					rcode = TRUE; break;

				case IDB_BURST:
					SaveBurstImages(dcx);
					rcode = TRUE; break;

				/* Intentionally unused IDs */
				case IDT_RED_SATURATE:
				case IDT_GREEN_SATURATE:
				case IDT_BLUE_SATURATE:
				case IDT_FRAMERATE:
				case IDT_FRAME_COUNT:
				case IDT_FRAME_VALID:
					break;

				default:
					printf("Unused wID in %s: %d\n", rname, wID); fflush(stdout);
					break;
			}

			return rcode;
	}
	return rcode;
}

static void show_camera_info_thread(void *arglist) {
	DialogBox(hInstance, "IDD_CAMERA_INFO", main_dcx->main_hdlg, (DLGPROC) CameraInfoDlgProc);
	return;
}

static int SaveBurstImages(DCX_WND_INFO *dcx) {

#ifndef USE_RINGS
	return 0;
#else
	int i, rc, inow, icount;
	size_t cnt;
	BOOL wasLive;
	FILE *csv_log;
	char pattern[PATH_MAX], pathname[PATH_MAX], *aptr;
	wchar_t wc_pathname[PATH_MAX];
	double tstamp, tstamp_0;
	double fps;

	IMAGE_FILE_PARAMS ImageParams;
	UC480IMAGEINFO ImageInfo;
	OPENFILENAME ofn;

	static char local_dir[PATH_MAX] = "";

	if (dcx->Image_Mem_Allocated) {
		if ( (wasLive = dcx->LiveVideo) ) {					/* Try to make sure we are stopped */
			rc = is_FreezeVideo(dcx->hCam, IS_WAIT);
			is_GetFramesPerSecond(dcx->hCam, &fps);						
			Sleep((int) (2000/fps+1));
		}
		dcx->LiveVideo = FALSE;
		
		/* Get the pattern for the save (directory and name without the extension */
		strcpy_m(pattern, sizeof(pattern), "basename");			/* Default name must be initialized with something */
		ofn.lStructSize       = sizeof(ofn);
		ofn.hwndOwner         = dcx->main_hdlg;
		ofn.lpstrTitle        = "Burst image database save";
		ofn.lpstrFilter       = "Excel csv file (*.csv)\0*.csv\0\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 1;
		ofn.lpstrFile         = pattern;					/* Full path */
		ofn.nMaxFile          = sizeof(pattern);
		ofn.lpstrFileTitle    = NULL;						/* Partial path */
		ofn.nMaxFileTitle     = 0;
		ofn.lpstrDefExt       = "csv";
		ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
		ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		
		if (GetSaveFileName(&ofn)) {									/* If aborted, just skip and go back to re-enabling the image */
			
			/* Save the directory for the next time */
			strcpy_m(local_dir, sizeof(local_dir), pattern);
			local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */
			
			aptr = pattern + strlen(pattern) - 4;					/* Should be the ".csv" */
			if (_stricmp(aptr, ".csv") == 0) *aptr = '\0';
			
			sprintf_s(pathname, sizeof(pathname), "%s.csv", pattern);
			fopen_s(&csv_log, pathname, "w");
			fprintf(csv_log, "/* Index,filename,t_relative,t_clock\n");
			
			if (dcx->nValid < dcx->nRing) {
				inow = 0; 
				icount = dcx->nValid;
			} else {
				inow = (dcx->nLast+1) % dcx->nRing;
				icount = dcx->nRing;
			}
			
			/* Prepopulate the parameters for the call */
			ImageParams.nQuality     = 0;
			ImageParams.nFileType    = IS_IMG_BMP;
			ImageParams.pwchFileName = wc_pathname;
			
			for (i=0; i<icount; i++) {
				is_GetImageInfo(dcx->hCam, dcx->Image_PID[inow], &ImageInfo, sizeof(ImageInfo));
				tstamp = ImageInfo.u64TimestampDevice*100E-9;
				if (i == 0) tstamp_0 = tstamp;
				
				sprintf_s(pathname, sizeof(pathname), "%s_%3.3d.bmp", pattern, i);
				fprintf(csv_log, "%d,%s,%.4f,%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d.%3.3d\n", i, pathname, tstamp-tstamp_0,
						  ImageInfo.TimestampSystem.wYear, ImageInfo.TimestampSystem.wMonth, ImageInfo.TimestampSystem.wDay, ImageInfo.TimestampSystem.wHour, ImageInfo.TimestampSystem.wMinute, ImageInfo.TimestampSystem.wSecond, ImageInfo.TimestampSystem.wMilliseconds);
				mbstowcs_s(&cnt, wc_pathname, PATH_MAX, pathname, _TRUNCATE);
				
				ImageParams.pnImageID    = &dcx->Image_PID[inow];
				ImageParams.ppcImageMem  = &dcx->Image_Mem[inow];
				rc = is_ImageFile(dcx->hCam, IS_IMAGE_FILE_CMD_SAVE, &ImageParams, sizeof(ImageParams));
				inow = (inow+1) % dcx->nRing;
			}
			if (csv_log != NULL) fclose(csv_log);
			
		}
		
		if (wasLive) {
			is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
			dcx->nLast = dcx->nValid = 0;
			dcx->LiveVideo = TRUE;
		}
	}
	return 0;
#endif
}


/* ===========================================================================
=========================================================================== */
BOOL CALLBACK CameraInfoDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "CameraInfoDlgProc";

	DCX_WND_INFO *dcx;
	int rc, wID, wNotifyCode, rcode;
	char szTmp[256];
	CAMINFO camInfo;
	SENSORINFO SensorInfo;

	/* Copy the source of all information */
	dcx = main_dcx;
	
/* The message loop */
	rcode = FALSE;
	switch (msg) {

		case WM_INITDIALOG:
			rc = is_GetCameraInfo(dcx->hCam, &camInfo);
			if (rc == IS_SUCCESS) {
				SetDlgItemText(hdlg, IDT_CAMERA_SERIAL_NO,		camInfo.SerNo);
				SetDlgItemText(hdlg, IDT_CAMERA_MANUFACTURER,	camInfo.ID);
				SetDlgItemText(hdlg, IDT_CAMERA_VERSION,			camInfo.Version);
				SetDlgItemText(hdlg, IDT_CAMERA_DATE,				camInfo.Date);
				SetDlgItemInt(hdlg,  IDT_CAMERA_ID, camInfo.Select, FALSE);
				// camInfo.Type == IS_CAMERA_TYPE_UC480_USB_SE ? "IS_CAMERA_TYPE_UC480_USB_SE" : camInfo.Type == IS_CAMERA_TYPE_UC480_USB_LE ? "IS_CAMERA_TYPE_UC480_USB_LE" : camInfo.Type == IS_CAMERA_TYPE_UC480_USB3_CP ? "IS_CAMERA_TYPE_UC480_USB3_CP" : "Unknown");
			}

			rc = is_GetSensorInfo(dcx->hCam, &SensorInfo);
			if (rc == 0) {
				SetDlgItemText(hdlg, IDT_CAMERA_MODEL,      SensorInfo.strSensorName);
				SetDlgItemText(hdlg, IDT_CAMERA_COLOR_MODE, SensorInfo.nColorMode == IS_COLORMODE_BAYER ? "Bayer" : SensorInfo.nColorMode == IS_COLORMODE_MONOCHROME ? "Monochrome" : "Unknown");
				sprintf_s(szTmp, sizeof(szTmp), "%d x %d", SensorInfo.nMaxWidth, SensorInfo.nMaxHeight);
				SetDlgItemText(hdlg, IDT_CAMERA_IMAGE_SIZE, szTmp);
				SetDlgItemInt(hdlg, IDT_CAMERA_PIXEL_PITCH, SensorInfo.wPixelSize, FALSE);
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
=========================================================================== */
#ifdef	STANDALONE

int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	/* If not done, make sure we are loaded.  Assume safe to call multiply */
	InitCommonControls();
	LoadLibrary("RICHED20.DLL");

	/* Load the class for the graph window */
	Graph_StartUp(hThisInstance);					/* Initialize the graphics control */

//	_beginthread(test_thread, 0, NULL);

	/* And show the dialog box */
	hInstance = hThisInstance;
	printf("Calling dialog box procedure\n"); fflush(stdout);
	DialogBox(hInstance, "DCX_DIALOG", HWND_DESKTOP, (DLGPROC) DCxDlgProc);

	printf("WinMain: returned from dialog box procedure\n"); fflush(stdout);

	/* And shut down the DCX server */
	Shutdown_DCx_Server();
	
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

int InitializeHistogramCurves(HWND hdlg, DCX_WND_INFO *dcx) {

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
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(255,0,0);
	dcx->red_hist = cv;

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
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(0,255,0);
	dcx->green_hist = cv;

	/* Initialize the blue histogram */
	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "reference");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = TRUE;
	cv->npt = npt;
	cv->x = calloc(sizeof(*cv->x), npt);
	cv->y = calloc(sizeof(*cv->y), npt);
	for (i=0; i<npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(128,128,255);
	dcx->blue_hist = cv;

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
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->x[i] = i; cv->y[i] = 0; }
	cv->rgb = RGB(255,255,255);
	dcx->horz_w = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row red scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,0,0);
	dcx->horz_r = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 3;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row green scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(0,255,0);
	dcx->horz_g = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 4;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "row blue scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(128,128,255);
	dcx->horz_b = cv;

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
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,255,255);
	dcx->vert_w = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 2;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col red scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(255,0,0);
	dcx->vert_r = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 3;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col green scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(0,255,0);
	dcx->vert_g = cv;

	cv = calloc(sizeof(GRAPH_CURVE), 1);
	cv->ID = 4;											/* reference curve */
	strcpy_m(cv->legend, sizeof(cv->legend), "col blue scan");
	cv->master        = FALSE;
	cv->visible       = TRUE;
	cv->free_on_clear = FALSE;
	cv->draw_x_axis   = cv->draw_y_axis   = FALSE;
	cv->force_scale_x = cv->force_scale_y = FALSE;
	cv->autoscale_x   = FALSE;	cv->autoscale_y = FALSE;
	cv->npt = 1280;									/* Normally large enough, but will expand when set */
	cv->x = calloc(sizeof(*cv->x), cv->npt);
	cv->y = calloc(sizeof(*cv->y), cv->npt);
	for (i=0; i<cv->npt; i++) { cv->y[i] = i; cv->x[i] = 0; }
	cv->rgb = RGB(128,128,255);
	dcx->vert_b = cv;

	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_CLEAR, (WPARAM) 0, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_LABEL_VISIBILITY, 0, 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_TITLE_VISIBILITY, 0, 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_NO_MARGINS, 1, 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_BACKGROUND_COLOR, RGB(0,0,64), 0);
	memset(&parms, 0, sizeof(parms));
	parms.suppress_grid = parms.suppress_ticks = TRUE;
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_SET_AXIS_PARMS, (WPARAM) &parms, (LPARAM) 0);

	/* Add all the curves now; only cv_raw should show initially */
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) dcx->red_hist,    (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) dcx->green_hist,  (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HISTOGRAMS,   WMP_ADD_CURVE, (WPARAM) dcx->blue_hist,   (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->horz_w,   (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->horz_r, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->horz_g, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_HORZ_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->horz_b, (LPARAM) 0);

	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->vert_w,   (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->vert_r, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->vert_g, (LPARAM) 0);
	SendDlgItemMessage(hdlg, IDG_VERT_PROFILE, WMP_ADD_CURVE, (WPARAM) dcx->vert_b, (LPARAM) 0);

	return 0;
}

/* ===========================================================================
=========================================================================== */
int InitializeScrollBars(HWND hdlg, DCX_WND_INFO *dcx) {
	int i, wID;

	/* Set up the scroll bar for the frame rate */
	SendDlgItemMessage(hdlg, IDS_FRAME_RATE, TBM_SETRANGE, FALSE, MAKELPARAM(10,10*MAX_FPS));	/* In 0.1 Hz increments */
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
-- Usage: void DCx_Start_Dialog(void *arglist);
--
-- Inputs: arglist - passed to WM_INITDIALOG as lParam (currently unused)
--
-- Output: simply launches the dialog box
--
-- Return: none
=========================================================================== */
void DCx_Start_Dialog(void *arglist) {

	DialogBoxParam(NULL, "DCX_DIALOG", HWND_DESKTOP, (DLGPROC) DCxDlgProc, (LPARAM) arglist);

	return;
}

/* ===========================================================================
-- Interface routine to accept a request to save an image as a file
--
-- Usage: int DCX_Capture_Image(char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap);
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
int DCx_Capture_Image(char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap) {
	static char *rname = "DCx_Capture_Image";
	
	wchar_t fname_w[MAX_PATH];
	IMAGE_FILE_PARAMS ImageParams;
	HCAM hCam;
	DCX_WND_INFO *dcx;

	int rc, col, line, height, width, pitch, nGamma, PID;
	unsigned char *pMem, *aptr;
	size_t ncount;

	/* If info provided, clear it in case we have a failure */
	if (info != NULL) memset(info, 0, sizeof(DCX_IMAGE_INFO));

	/* Must have been started at some point to be able to return images */
	if (main_dcx == NULL || main_dcx->hCam <= 0) return 1;
	dcx  = main_dcx;
	hCam = dcx->hCam;

	/* Capture and hold an image */
	if ( (rc = is_FreezeVideo(hCam, IS_WAIT)) != 0) {
		printf("%s: is_FreezeVideo returned failure (rc=%d)", rname, rc);
		rc = is_FreezeVideo(hCam, IS_WAIT);
		printf("  Retry gives: %d\n", rc);
		fflush(stdout);
	}

	/* Convert the filename */
	if (fname != NULL) {
		mbstowcs_s(&ncount, fname_w, MAX_PATH, fname, strlen(fname));
		ImageParams.pwchFileName = fname_w;
	} else {
		ImageParams.pwchFileName = NULL;
	}

	/* Specify the compression for PNG and JPG - BMP is uncompressed */
	if (quality <= 0)  quality = 75;
	if (quality > 100) quality = 100;
	ImageParams.nQuality = quality;

	ImageParams.pnImageID    = NULL;	
	ImageParams.ppcImageMem  = NULL;

	switch (format) {
		case IMAGE_JPG:
			ImageParams.nFileType = IS_IMG_JPG; break;
		case IMAGE_PNG:
			ImageParams.nFileType = IS_IMG_PNG; break;
		case IMAGE_BMP:
		default:
			ImageParams.nFileType = IS_IMG_BMP; break;
	}
	rc = is_ImageFile(hCam, IS_IMAGE_FILE_CMD_SAVE, &ImageParams, sizeof(ImageParams));
	rc = is_GetImageMem(hCam, &pMem);
	rc = is_GetImageMemPitch(hCam, &pitch);

	if (info != NULL) {
		height = dcx->height;
		width  = dcx->width;
		
/* Calculate the number of saturated pixels on each color plane */
		info->red_saturate = info->green_saturate = info->blue_saturate = 0;

		for (line=0; line<height; line++) {
			aptr = pMem + line*pitch;					/* Pointer to this line */
			for (col=0; col<width; col++) {
				if (dcx->SensorIsColor) {
					if (aptr[3*col+0] >= 255) info->blue_saturate++;
					if (aptr[3*col+1] >= 255) info->green_saturate++;
					if (aptr[3*col+2] >= 255) info->red_saturate++;
				} else {
					if (aptr[col] >= 255) info->blue_saturate = info->green_saturate = ++info->red_saturate;
				}
			}
		}

		info->memory_pitch = pitch;
		info->width = width;
		info->height = height;

		is_Exposure(hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &info->exposure, sizeof(info->exposure));

		is_Gamma(hCam, IS_GAMMA_CMD_GET, &nGamma, sizeof(nGamma));
		info->gamma = nGamma / 100.0;
		info->color_correction = is_SetColorCorrection(hCam, IS_GET_CCOR_MODE, &info->color_correction_factor);

		info->master_gain = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
		info->red_gain    = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(hCam, IS_GET_RED_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
		info->green_gain  = (dcx->SensorInfo.bGGain)      ? is_SetHardwareGain(hCam, IS_GET_GREEN_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
		info->blue_gain   = (dcx->SensorInfo.bBGain)      ? is_SetHardwareGain(hCam, IS_GET_BLUE_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	}

	if (IsWindow(hwndRenderBitmap) && (PID = FindImagePID(dcx, pMem, NULL)) >= 0) {
		is_RenderBitmap(dcx->hCam, PID, hwndRenderBitmap, IS_RENDER_FIT_TO_WINDOW);
		GenerateCrosshair(dcx, hwndRenderBitmap);
	}

	if (dcx->LiveVideo) {
		is_CaptureVideo(hCam, IS_DONT_WAIT);
		dcx->nLast = dcx->nValid = 0;
	}
	
	return rc;
}

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
int DCx_Acquire_Image(DCX_IMAGE_INFO *info, char **buffer) {
	static char *rname = "DCx_Acquire_Image";

	HCAM hCam;
	DCX_WND_INFO *dcx;

	int rc, col, line, height, width, pitch, nGamma;
	unsigned char *pMem, *aptr;

	/* If info provided, clear it in case we have a failure */
	if (info != NULL) memset(info, 0, sizeof(DCX_IMAGE_INFO));
	if (buffer == NULL) return -1;

	/* Must have been started at some point to be able to return images */
	if (main_dcx == NULL || main_dcx->hCam <= 0) return 1;
	dcx  = main_dcx;
	hCam = dcx->hCam;

	/* Capture and hold an image */
	if ( (rc = is_FreezeVideo(hCam, IS_WAIT)) != 0) {
		printf("%s: is_FreezeVideo returned failure (rc=%d)", rname, rc);
		rc = is_FreezeVideo(hCam, IS_WAIT);
		printf("  Retry gives: %d\n", rc);
		fflush(stdout);
	}

	rc = is_GetImageMem(hCam, &pMem);
	rc = is_GetImageMemPitch(hCam, &pitch);
	height = dcx->height;
	width  = dcx->width;

	/* Copy the image to an allocated buffer */
	*buffer = malloc(pitch*height);			/* Allocate space for new memory */
	memcpy(*buffer, pMem, pitch*height);

	if (info != NULL) {

		/* Copy over information */
		info->width = width;
		info->height = height;
		info->memory_pitch = pitch;

		is_Exposure(hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &info->exposure, sizeof(info->exposure));

		is_Gamma(hCam, IS_GAMMA_CMD_GET, &nGamma, sizeof(nGamma));
		info->gamma = nGamma / 100.0;
		info->color_correction = is_SetColorCorrection(hCam, IS_GET_CCOR_MODE, &info->color_correction_factor);

		info->master_gain = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
		info->red_gain    = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(hCam, IS_GET_RED_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
		info->green_gain  = (dcx->SensorInfo.bGGain)      ? is_SetHardwareGain(hCam, IS_GET_GREEN_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
		info->blue_gain   = (dcx->SensorInfo.bBGain)      ? is_SetHardwareGain(hCam, IS_GET_BLUE_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;

		/* Calculate the number of saturated pixels on each color plane */
		info->red_saturate = info->green_saturate = info->blue_saturate = 0;
		for (line=0; line<height; line++) {
			aptr = pMem + line*pitch;					/* Pointer to this line */
			for (col=0; col<width; col++) {
				if (dcx->SensorIsColor) {
					if (aptr[3*col+0] >= 255) info->blue_saturate++;
					if (aptr[3*col+1] >= 255) info->green_saturate++;
					if (aptr[3*col+2] >= 255) info->red_saturate++;
				} else {
					if (aptr[col] >= 255) info->blue_saturate = info->green_saturate = ++info->red_saturate;
				}
			}
		}
	}

	if (dcx->LiveVideo) {
		is_CaptureVideo(hCam, IS_DONT_WAIT);
		dcx->nLast = dcx->nValid = 0;
	}
	return 0;
}


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
int DCx_Status(DCX_STATUS *status) {

	DCX_WND_INFO *dcx;
	HCAM hCam;
	CAMINFO camInfo;
	SENSORINFO SensorInfo;
	int nGamma;
	
	/* In case of errors, return all zeros in the structure if it exists */
	if (status != NULL) memset(status, 0, sizeof(*status));

	/* Must have been started at some point to be able to return images */
	if (main_dcx == NULL || main_dcx->hCam <= 0) return 1;
	dcx  = main_dcx;
	hCam = dcx->hCam;

	/* If status is NULL, only interested in if the camera is alive and configured */
	if (status == NULL) return 0;

	/* Now maybe give information */
	is_SetFrameRate(hCam, IS_GET_FRAMERATE, &status->fps);
	is_Exposure(hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &status->exposure, sizeof(status->exposure));

	is_Gamma(hCam, IS_GAMMA_CMD_GET, &nGamma, sizeof(nGamma));
	status->gamma = nGamma / 100.0;

	status->master_gain = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	status->red_gain    = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(hCam, IS_GET_RED_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	status->green_gain  = (dcx->SensorInfo.bGGain)      ? is_SetHardwareGain(hCam, IS_GET_GREEN_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	status->blue_gain   = (dcx->SensorInfo.bBGain)      ? is_SetHardwareGain(hCam, IS_GET_BLUE_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;

	if (is_GetSensorInfo(hCam, &SensorInfo) == IS_SUCCESS) {
		status->pixel_pitch = SensorInfo.wPixelSize;
		strcpy_m(status->model, sizeof(status->model), SensorInfo.strSensorName);
		status->color_mode = SensorInfo.nColorMode == IS_COLORMODE_MONOCHROME ? IMAGE_MONOCHROME : IMAGE_COLOR ;
	}
	if (is_GetCameraInfo(hCam, &camInfo) == IS_SUCCESS) {
		strcpy_m(status->serial, sizeof(status->serial), camInfo.SerNo); 
		strcpy_m(status->manufacturer, sizeof(status->manufacturer), camInfo.ID); 
		strcpy_m(status->version, sizeof(status->version), camInfo.Version); 
		strcpy_m(status->date, sizeof(status->date), camInfo.Date); 
		status->CameraID = camInfo.Select;
	}

	status->color_correction = is_SetColorCorrection(dcx->hCam, IS_GET_CCOR_MODE, &status->color_correction_factor);

	return 0;
}


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
int DCx_WriteParameters(char *pre_text, FILE *funit) {

	DCX_STATUS status;
	int rc;

	if (pre_text == NULL) pre_text = "";
	
	if (DCx_Status(&status) == 0 && funit != NULL) {
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
int DCx_Set_Exposure(DCX_WND_INFO *dcx, double exposure, BOOL maximize_framerate, HWND hdlg) {

	struct {
		double rmin, rmax, rinc;
	} exp_range;
	double current, fps;

	/* If dcx is NULL, then use defaults for dcx and main dialog as hdlg (unless !NULL) */
	if (dcx == NULL) {
		dcx = main_dcx;
		if (hdlg == NULL) hdlg = dcx->main_hdlg;
	}

/* -------------------------------------------------------------------------------
-- Get the exposure allowed range and the current exposure value
-- Note that is_Exposure(IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE) is limited by current
-- framerate while is_GetFrameTimeRange() is not.  But deal with bug in the return 
-- values from is_GetFrameTimeRange()
--------------------------------------------------------------------------- */
//	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE, &exp_range, sizeof(exp_range));
	is_GetFrameTimeRange(dcx->hCam, &exp_range.rmin, &exp_range.rmax, &exp_range.rinc);
//	exp_range.rmin *= 1000;											/* Go from seconds to ms (but looks to already in ms so ignore) */
	exp_range.rmax *= 1000;											/* Go from seconds to ms */
	exp_range.rinc *= 1000;											/* Go from seconds to ms */
	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &current, sizeof(current));
	if (exposure < exp_range.rmin) exposure = exp_range.rmin;
	if (exposure > exp_range.rmax) exposure = exp_range.rmax;
	if (exposure > current && exposure-current < exp_range.rinc) exposure = current+1.01*exp_range.rinc;
	if (exposure < current && current-exposure < exp_range.rinc) exposure = current-1.01*exp_range.rinc;

	/* Unfortunately, while framerate will auto decrease exposure, exposure will not auto increase frame rate */
	if (maximize_framerate) {
		is_SetFrameRate(dcx->hCam, IS_GET_FRAMERATE, &fps);
		if (1000.0/fps < exposure+0.1 || fps < MAX_FPS-0.1) {	/* Change framerate to best value for this  */
			fps = (int) (10*1000.0/exposure) / 10.0;			/* Closest 0.1 value */
			if (fps > MAX_FPS) fps = MAX_FPS;
			is_SetFrameRate(dcx->hCam, fps, &fps);			/* Set and query simultaneously */
			if (hdlg != NULL && IsWindow(hdlg)) SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);	/* Make sure this is up to date */
		}
	}
	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_SET_EXPOSURE, &exposure, sizeof(exposure));
	if (hdlg != NULL && IsWindow(hdlg)) SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);

	return 0;
}				

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
int DCx_Set_Gains(DCX_WND_INFO *dcx, int master, int red, int green, int blue, HWND hdlg) {

	/* If dcx is NULL, then use defaults for dcx and main dialog as hdlg (unless !NULL) */
	if (dcx == NULL) {
		dcx = main_dcx;
		if (hdlg == NULL) hdlg = dcx->main_hdlg;
	}

	/* Set the gains immediately */
	is_SetHardwareGain(dcx->hCam, master, red, green, blue);
	if (hdlg != NULL && IsWindow(hdlg)) SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);

	return 0;
}				


/* ===========================================================================
-- Routines to allocate and release image ring buffers on either size change 
-- or when camera is released / changed
--
-- Usage: int AllocRingBuffers(DCX_WND_INFO *dcx, int nRing);
--        int ReleaseRingBuffers(DCX_WND_INFO *dcx);
--
-- Inputs: dcx   - pointer to valid structure for the camera window
--         nRing - number of ring buffers desired.
--                 if <= 1, use current ring size but reallocate (maybe new camera)
--
-- Output: Release clears the sequence for the camera and releases the memory 
--         Alloc will allocate/change memory if there is a valid camera (hCam)
--
-- Return: 0 on success; otherwise an error code
=========================================================================== */
static int ReleaseRingBuffers(DCX_WND_INFO *dcx) {

#ifndef USE_RINGS
	if (dcx->Image_Mem_Allocated) {
		if (dcx->LiveVideo) is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);
		dcx->LiveVideo = FALSE;
		is_FreeImageMem(dcx->hCam, dcx->Image_Mem, dcx->Image_PID);
		dcx->Image_Mem_Allocated = FALSE;
	}
#else
	int i;
	if (dcx->Image_Mem_Allocated) {
		if (dcx->LiveVideo) is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);
		dcx->LiveVideo = FALSE;
		is_ClearSequence(dcx->hCam);					/* Clear the sequence definitely */
		for (i=0; i<dcx->nRing; i++) is_FreeImageMem(dcx->hCam, dcx->Image_Mem[i], dcx->Image_PID[i]);
		free(dcx->Image_Mem);							/* Free the buffers */
		free(dcx->Image_PID); 
		dcx->Image_Mem_Allocated = FALSE;
		fprintf(stderr, "Release ring buffers\n"); fflush(stderr);
	}
#endif
	return 0;
}

static int AllocRingBuffers(DCX_WND_INFO *dcx, int nRing) {
	int rc;
#ifdef USE_RINGS
	int i;
#endif
	BOOL LiveVideo_Hold;

	/* Make sure valid arguments */
	if (dcx == NULL) return -1;

	/* Save video state so can restore after modifications */
	LiveVideo_Hold = dcx->LiveVideo;
	if (LiveVideo_Hold) is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);
	dcx->LiveVideo = FALSE;

#ifndef USE_RINGS
	rc = is_AllocImageMem(dcx->hCam, dcx->width, dcx->height, dcx->SensorIsColor ? 24 : 8, &dcx->Image_Mem, &dcx->Image_PID);
	rc = is_SetImageMem(dcx->hCam, dcx->Image_Mem, dcx->Image_PID); 
	printf("  Allocated Image memory: %p  PID: %d\n", dcx->Image_Mem, dcx->Image_PID); fflush(stdout);
#else
	/* Determine the new size (or size) of the ring buffer */
	if (nRing <= 1) nRing = dcx->nRing;								/* If 0, use current size */
	if (nRing >= 1000) nRing = 999;									/* Limit to reasonable */

	/* If size changing, release existing buffers */
	if (nRing != dcx->nRing) ReleaseRingBuffers(dcx);

	/* Store the new ring size and allocate if there is an active camera */
	dcx->nRing = nRing;
	dcx->Image_Mem   = calloc(dcx->nRing, sizeof(*dcx->Image_Mem));
	dcx->Image_PID   = calloc(dcx->nRing, sizeof(*dcx->Image_PID));
	for (i=0; i<dcx->nRing; i++) {
		if ( (rc = is_AllocImageMem(dcx->hCam, dcx->width, dcx->height, dcx->SensorIsColor ? 24 : 8, &dcx->Image_Mem[i], &dcx->Image_PID[i])) != IS_SUCCESS) {
			fprintf(stderr, "  Image memory allocation failed (rc=%d)\n", rc); fflush(stderr);
		} else if ( (rc = is_AddToSequence(dcx->hCam, dcx->Image_Mem[i], dcx->Image_PID[i])) != IS_SUCCESS) {
			fprintf(stderr, "  Adding image to the list failed (rc=%d)\n", rc); fflush(stderr);
		}
	}
	dcx->nLast = dcx->nValid = 0;
	printf("  Allocated %d images for ring buffer\n", dcx->nRing); fflush(stdout);
#endif

	dcx->Image_Mem_Allocated = TRUE;

	/* If image was live, restart it now */
	if (LiveVideo_Hold) {
		is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);			/* If live, need to turn off to reset */
		dcx->LiveVideo = TRUE;
		dcx->nLast = dcx->nValid = 0;
	}

	return 0;
}


/* ===========================================================================
-- Routine to try to return a high precision timer value.  The return
-- should be in units of seconds with the highest precision that is
-- reasonable for the system.  It is guarenteed to be monotonic but not
-- to be absolute.
--
-- Usage: double my_timer(BOOL reset)
--
-- Inputs: reset - should system reset the timer to zero return?
--
-- Output: none
--
-- Return: Time in seconds since first call or last reset of the counter
=========================================================================== */
static double my_timer(BOOL reset) {

	static BOOL init=FALSE;
	static LARGE_INTEGER freq, count0;
	LARGE_INTEGER counts;

	if (! init || reset) {
		init = TRUE;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&count0);
		return 0.0;
	}
	QueryPerformanceCounter(&counts);

	return (freq.QuadPart == 0) ? 0.0 : (1.0*(counts.QuadPart-count0.QuadPart))/freq.QuadPart;
}
