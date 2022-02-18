/* Common routines that simply point to specific camera routines */

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

/* Load camera specific API for me */
#include "camera.h"
#include "dcx.h"								/* DCX API camera routines & info */
#define	INCLUDE_MINIMAL_TL
#include "tl.h"								/* TL  API camera routines & info */

#define	INCLUDE_WND_DETAIL_INFO			/* Get all of the typedefs and internal details */
#include "ZooCam.h"							/* Access to the ZooCam info */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#ifndef PATH_MAX
	#define	PATH_MAX	(260)
#endif

#define	nint(x)	(((x)>0) ? ( (int) (x+0.5)) : ( (int) (x-0.5)) )

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

/* ===========================================================================
-- Get the exposure setting for the camera
--
-- Usage: int Camera_GetExposureParms(HWND hdlg, WND_INFO *wnd, 
--												  double *ms_min, double *ms_max, double *ms_incr);
--
-- Inputs: hdlg    - calling dialog box (or NULL)
--         wnd     - pointer to valid window information
--         ms_min  - pointer to get minimum allowed exposure time
--         ms_max  - pointer to get maximum allowed exposure time
--         ms_incr - pointer to get minmum increment time
--
-- Output: for each non-NULL parameter, gets value
--
-- Return: 0 if successful, errors from each camera otherwise
--
-- Notes: Not all cameras have a minimum increment; returns 0.001 ms if unknown
=========================================================================== */
int Camera_GetExposureParms(HWND hdlg, WND_INFO *wnd, double *ms_min, double *ms_max, double *ms_incr) {
	static char *rname = "Camera_GetExposureParms";

	TL_CAMERA  *camera;
	DCX_CAMERA *dcx;
	int rc;

	if (wnd == NULL || ! wnd->bCamera) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;						/* (DCX_CAMERA *) wnd->Camera.details; */
			rc = DCx_GetExposureParms(dcx, ms_min, ms_max, ms_incr);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetExposureParms(camera, ms_min, ms_max);
			*ms_incr = 0.001;											/* Return default value */
			break;
		default:
			rc = 2;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Get the exposure setting for the camera
--
-- Usage: double Camera_GetExposure(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg  - calling dialog box (or NULL)
--         wnd   - pointer to valid window information
--
-- Output: none
--
-- Return: Current exposure time in milliseconds
=========================================================================== */
double Camera_GetExposure(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetExposure";

	double rval;
	TL_CAMERA  *camera;
	DCX_CAMERA *dcx;

	if (wnd == NULL || ! wnd->bCamera) return 0;
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;						/* (DCX_CAMERA *) wnd->Camera.details; */
			rval = DCx_GetExposure(dcx, FALSE);					/* Just query (not necessary to request) */
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rval = TL_GetExposure(camera, FALSE);				/* Just query (not necessary to request) */
			break;
		default:
			rval = 1.0;
			break;
	}

	return rval;
}

/* ===========================================================================
-- Set the exposure time for currently active camera (in ms).  Returns
-- actual exposure time
--
-- Usage: double Camera_SetExposure(HWND hdlg, WND_INFO *wnd, double ms_expose);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--         ms_exposure - desired exposure time in milliseconds
--
-- Output: sets exposure on active camera within allowable bounds
--
-- Return: 0 on error, otherwise actual exposure time instantiated in ms
=========================================================================== */
double Camera_SetExposure(HWND hdlg, WND_INFO *wnd, double ms_expose) {
	static char *rname = "Camera_SetExposure";

	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	/* Make sure we have valid structures */
	if (wnd == NULL) wnd = main_wnd;
	if (wnd == NULL || ! wnd->bCamera) return 0.0;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	/* Now just switch on the type of camera */
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			ms_expose = DCx_SetExposure(dcx, ms_expose);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			ms_expose = TL_SetExposure(camera, ms_expose);
			break;

		default:
			break;
	}

	/* Update exposure, but also deal with fact that framerate may change */
	if (hdlg != NULL && IsWindow(hdlg)) {
		SendMessage(hdlg, WMP_SHOW_EXPOSURE, 0, 0);
		SendMessage(hdlg, WMP_SHOW_FRAMERATE, 0, 0);
	}

	return ms_expose;
}				


/* ===========================================================================
-- Set the RGB gains ... each camera may do very differnt things
--
-- Usage: int Camera_Set_RGB_Gain(HWND hdlg, WND_INFO *wnd, 
--											 enum {R_CHAN, G_CHAN, B_CHAN} channel, 
--											 enum {IS_SLIDER, IS_VALUE} entry, 
--											 double value);
--
-- Inputs: hdlg    - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd     - handle to the main information structure
--         channel - which channel to modify
--                    M_CHAN = master, R_CHAN, G_CHAN, B_CHAN = red, reen, blue
--         entry   - type of data
--                   IS_VALUE  - direct value from an entry box
--                   IS_SLIDER - 0.0-1.0 based on slider position
--         value   - actual value either from slider fractional position [0-1] or entry box (arbitrary)
--
-- Output: sets gain of the individual channel
--
-- Return: 0 if successful; otherwise error code
--           1 ==> camera is not active
=========================================================================== */
int Camera_SetGains(HWND hdlg, WND_INFO *wnd, GAIN_CHANNEL channel, ENTRY_TYPE entry, double value) {
	static char *rname = "Camera_SetGains";

	int rc, ival;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	/* Make sure we have valid structures */
	if (wnd == NULL) wnd = main_wnd;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	/* Must have an active camera to set */
	if (wnd == NULL || ! wnd->bCamera) return 1;

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			if (IS_SLIDER == entry) value *= 100.0;										/* Rescale fraction to in on [0,100] */
			ival = (int) (value+0.5);
			ival = max(0,min(100,ival));
			if (channel == M_CHAN) DCx_SetRGBGains(dcx, ival, DCX_IGNORE_GAIN, DCX_IGNORE_GAIN, DCX_IGNORE_GAIN);
			if (channel == R_CHAN) DCx_SetRGBGains(dcx, DCX_IGNORE_GAIN, ival, DCX_IGNORE_GAIN, DCX_IGNORE_GAIN);
			if (channel == G_CHAN) DCx_SetRGBGains(dcx, DCX_IGNORE_GAIN, DCX_IGNORE_GAIN, ival, DCX_IGNORE_GAIN);
			if (channel == B_CHAN) DCx_SetRGBGains(dcx, DCX_IGNORE_GAIN, DCX_IGNORE_GAIN, DCX_IGNORE_GAIN, ival);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			if (channel == M_CHAN) {												/* Handle master channel as dB */
				double db_min, db_max;
				TL_GetMasterGainInfo(camera, NULL, NULL, &db_min, &db_max);
				if (IS_SLIDER == entry) value = db_min + value*(db_max-db_min);
				TL_SetMasterGain(camera, value);
			} else {
				if (entry == IS_SLIDER) value = 0.5*exp(value*log(20));	/* For slider, logarithmic on [0.5,10.0] */
				if (channel == R_CHAN) TL_SetRGBGains(camera, value, TL_IGNORE_GAIN, TL_IGNORE_GAIN);
				if (channel == G_CHAN) TL_SetRGBGains(camera, TL_IGNORE_GAIN, value, TL_IGNORE_GAIN);
				if (channel == B_CHAN) TL_SetRGBGains(camera, TL_IGNORE_GAIN, TL_IGNORE_GAIN, value);
			}
			break;
		default:
			break;
	}

	return rc;
}


/* ===========================================================================
-- Reset RGB gains to default values (original when camera opened)
--
-- Usage: int Camera_ResetGains(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg    - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd     - handle to the main information structure
--
-- Output: resets gains of all channels to defaults if possible
--
-- Return: 0 if successful; otherwise error code
--           1 ==> camera is not active
=========================================================================== */
int Camera_ResetGains(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_ResetGains";

	int rc;

	/* Make sure we have valid structures */
	if (wnd == NULL) wnd = main_wnd;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	/* Must have an active camera to set */
	if (wnd == NULL || ! wnd->bCamera) return 1;

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
		{
			int master, red, green, blue;
			DCX_CAMERA *dcx;

			dcx = wnd->dcx;
			DCx_GetDfltRGBGains(dcx, &master, &red, &green, &blue);
			DCx_SetRGBGains(dcx, master, red, green, blue);
		}
		break;
		case TL:
		{
			double master, red, green, blue;
			TL_CAMERA *camera;

			camera = (TL_CAMERA *) wnd->Camera.details;
			TL_GetMasterGainInfo(camera, NULL, &master, NULL, NULL);	TL_SetMasterGain(camera, master);
			TL_GetDfltRGBGains(camera, &red, &green, &blue);			TL_SetRGBGains(camera, red, green, blue);
		}
		break;
		default:
			break;
	}

	return rc;
}


/* ===========================================================================
-- Set the RGB gains ... each camera may do very differnt things
--
-- Usage: int Camera_GetGains(HWND hdlg, WND_INFO *wnd, double values[4], double slider[4]);
--
-- Inputs: hdlg      - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd       - handle to the main information structure
--         values[4] - array to receive numerical values for text entry boxes
--         slider[4] - array to receive fractions [0.0,1.0] for setting sliders
--                     Order is master, red, green, blue in each array
--
-- Output: Queries gains and returns numbers to display
--
-- Return: 0 if successful; otherwise error code
--           1 ==> camera is not active
=========================================================================== */
int Camera_GetGains(HWND hdlg, WND_INFO *wnd, double values[4], double slider[4]) {
	static char *rname = "Camera_GetGains";

	int i, rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	double rval[4], db_min, db_max;
	int ival[4];

	/* Initial return values */
	if (values != NULL) for (i=0; i<4; i++) values[i] = 0.0;
	if (slider != NULL) for (i=0; i<4; i++) slider[i] = 0.0;

	/* Make sure we have valid structures */
	if (wnd == NULL) wnd = main_wnd;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	/* Must have an active camera to set */
	if (wnd == NULL || ! wnd->bCamera) return 1;

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			DCx_GetRGBGains(dcx, &ival[0], &ival[1], &ival[2], &ival[3]);
			if (values != NULL) for (i=0; i<4; i++) values[i] = ival[i];
			if (slider != NULL) {
				for (i=0; i<4; i++) slider[i] = ival[i] / 100.0 ;
				for (i=0; i<4; i++) slider[i] = max(0.0, min(1.0, slider[i]));
			}
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			TL_GetMasterGain(camera, &rval[0]);
			TL_GetMasterGainInfo(camera, NULL, NULL, &db_min, &db_max);
			TL_GetRGBGains(camera, &rval[1], &rval[2], &rval[3]);

			if (values != NULL) for (i=0; i<4; i++) values[i] = rval[i];
			if (slider != NULL) {
				slider[0] = (rval[0]-db_min)/(db_max-db_min+1E-10);
				for (i=1; i<4; i++) slider[i] = log(2*max(0.5,rval[i]))/log(20.0);
				for (i=0; i<4; i++) slider[i] = max(0.0, min(1.0, slider[i]));
			}
			break;

		default:
			break;
	}

	return rc;
}

/* ===========================================================================
-- Get the exposure setting for the camera
--
-- Usage: Camera_GetFPSActual(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg    - calling dialog box (or NULL)
--         wnd     - pointer to valid window information
--
-- Output: none
--
-- Return: Estimate of framerate from camera
=========================================================================== */
double Camera_GetFPSActual(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetFPSActual";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	double fps;

	if (wnd == NULL || ! wnd->bCamera) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			fps = DCx_GetFPSActual(dcx);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			fps = TL_GetFPSActual(camera);
			break;
		default:
			fps = 0.0;
			break;
	}

	return fps;
}

/* ===========================================================================
-- Set the framerate on cameras supporting
--
-- Usage: double Camera_SetFPSControl(HWND hdlg, WND_INFO *wnd, double fps);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--         fps  - desired rate
--
-- Output: sets fps if possible
--
-- Return: Actual value set, or 0 if error
=========================================================================== */
double Camera_SetFPSControl(HWND hdlg, WND_INFO *wnd, double fps) {
	static char *rname = "Camera_SetFPSControl";

	DCX_CAMERA *dcx;
	TL_CAMERA *tl;

	/* Make sure we have valid structures and an active camera */
	if (wnd == NULL) wnd = main_wnd;
	if (wnd == NULL || ! wnd->bCamera) return 1;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			fps = DCx_SetFPSControl(dcx, fps);
			break;

		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			fps = TL_SetFPSControl(tl, fps);
			break;

		default:
			fps = 0.0;
			break;
	}

	return fps;
}

/* ===========================================================================
-- Return the framerate setting on cameras supporting
--
-- Usage: double Camera_GetFPSControl(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--
-- Output: none
--
-- Return: - current framerate setting in fps if supported or <= 0 on error
=========================================================================== */
double Camera_GetFPSControl(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetFPSControl";

	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	double fps;

	/* Make sure we have valid structures and an active camera */
	if (wnd == NULL) wnd = main_wnd;
	if (wnd == NULL || ! wnd->bCamera) return 0.0;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			fps = DCx_GetFPSControl(dcx);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			fps = TL_GetFPSControl(camera);
			break;
		default:
			fps = 0.0;
	}

	return fps;
}	

/* ===========================================================================
-- Set the gamma factor on cameras supporting
--
-- Usage: double Camera_SetGamma(HWND hdlg, WND_INFO *wnd, double gamma);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--         gamma - desired value ... with 1.0 as neutral
--
-- Output: sets gamma factor on active camera
--
-- Return: value actually set or 0.0 on error
=========================================================================== */
double Camera_SetGamma(HWND hdlg, WND_INFO *wnd, double gamma) {
	static char *rname = "Camera_SetGamma";

	DCX_CAMERA *dcx;

	/* Verify structure and that we have a camera */
	if (wnd == NULL || ! wnd->bCamera) return 1;
	if (wnd->Camera.driver != DCX) return 2;
	dcx = wnd->dcx;

	return DCx_SetGamma(dcx, gamma);
}

/* ===========================================================================
-- Return the gamma factor on cameras supporting
--
-- Usage: double Camera_GetGamma(HWND hdlg, WND_INFO *wnd, double *gamma);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--
-- Output: none
--
-- Return: gamma value from camera, or 0 on error
=========================================================================== */
double Camera_GetGamma(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetGamma";

	DCX_CAMERA *dcx;

	/* Verify structure and that we have a camera */
	if (wnd == NULL || ! wnd->bCamera) return 0.0;

	if (wnd->Camera.driver != DCX) return 0.0;

	dcx = (DCX_CAMERA *) wnd->dcx;
	return DCx_GetGamma(dcx);
}


/* ===========================================================================
-- Alternate routine to render a specific image frame in the buffer to a window
-- Live images are processed in the threads
--
-- Usage: int Camera_RenderImage(HWND hdlg, WND_INFO *wnd, int frame, HWND hwnd);
--
-- Inputs: hdlg  - calling dialog box (or NULL)
--         wnd   - pointer to valid window information
--         frame - index of frame to image (0 = current)
--                 will be limited to allowed range
--         hwnd  - window where image is to be rendered
=========================================================================== */
int Camera_RenderImage(HWND hdlg, WND_INFO *wnd, int frame, HWND hwnd) {
	static char *rname = "Camera_RenderImage";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	if (! wnd->bCamera || ! IsWindow(hwnd)) return 0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_RenderImage(dcx, frame, hwnd);
			GenerateCrosshair(wnd, hwnd);
			GenerateCrosshair(wnd, hwnd);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_RenderImage(camera, 0, hwnd);
			GenerateCrosshair(wnd, hwnd);
			break;
		default:
			break;
	}
	return 0;
}


/* ===========================================================================
-- Force trigger camera
--
-- Usage: Camera_Trigger(HWND hdlg, WND_INFO *wnd, int msWait);
--
-- Inputs: hdlg    - calling dialog box (or NULL)
--         wnd     - pointer to valid window information
--         msWait - time to wait before forcing change
--                  <0 ==> wait indefinitely for image
--                   0 ==> return immediately (processing continues in background)
--                  >0 ==> timeout
--
-- Output: Triggers camera immediately (last time if FREERUN)
--
-- Return: 0 if successful, !0 otherwise (see individual)
--
-- Notes: msWait < 0 will wait until there is an image captured
--               = 0 returns immediately but still triggers the capture
--        If trigger mode was FREERUN, will be set to SOFTWARE after call
=========================================================================== */
int Camera_Trigger(HWND hdlg, WND_INFO *wnd, int msWait) {
	static char *rname = "Camera_Trigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL || ! wnd->bCamera) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_Trigger(dcx, msWait);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_Trigger(camera, msWait);
			break;
		default:
			rc = 0;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Set/Query the triggering mode for the camera
--
-- Usage: TRIG_MODE Camera_SetTrigMode(HWND hdlg, WND_INFO *wnd, TRIG_MODE mode, int msWait);
--        TRIG_MODE Camera_GetTrigMode(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg   - calling dialog box (or NULL)
--         wnd    - pointer to valid window information
--         mode   - one of the allowed triggering modes
--                  CAMERA_TRIG_SOFTWARE, CAMERA_TRIG_FREERUN, CAMERA_TRIG_EXTERNAL
--         msWait - time to wait before forcing change
--                  <0 ==> wait indefinitely for image
--                   0 ==> return immediately (processing continues in background)
--                  >0 ==> timeout
--
-- Output: Sets camera triggering mode
--
-- Return: Actual mode now set for camera or <0 on error
=========================================================================== */
TRIG_MODE Camera_SetTrigMode(HWND hdlg, WND_INFO *wnd, TRIG_MODE mode, int msWait) {
	static char *rname = "Camera_SetTrigMode";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL || ! wnd->bCamera) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_SetTrigMode(dcx, mode, msWait);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SetTrigMode(camera, mode, msWait);
			break;

		default:
			rc = -3;
			break;
	}

	return rc;
}


TRIG_MODE Camera_GetTrigMode(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetTrigMode";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL || ! wnd->bCamera) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_GetTrigMode(dcx);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetTrigMode(camera);
			break;

		default:
			rc = -3;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Set/Query the color correction mode for the camera
--
-- Usage: COLOR_CORRECT Camera_SetColorCorrection(HWND hdlg, WND_INFO *wnd, COLOR_CORRECT mode, double rval);
--        COLOR_CORRECT Camera_SetColorCorrection(HWND hdlg, WND_INFO *wnd, double &rval);
--
-- Inputs: hdlg   - calling dialog box (or NULL)
--         wnd    - pointer to valid window information
--         mode   - one of the allowed color correction modes
--         rval   - strength of color corrections (camera dependent)
--
-- Output: Sets color correction mode
--
-- Return: Actual color correction mode set for camera or <0 on error
=========================================================================== */
COLOR_CORRECT Camera_SetColorCorrection(HWND hdlg, WND_INFO *wnd, COLOR_CORRECT mode, double rval) {
	static char *rname = "Camera_SetColorCorrection";

	DCX_CAMERA *dcx;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL /* || ! wnd->bCamera */) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_SetColorCorrection(dcx, mode, rval);
			break;

		case TL:
		default:
			rc = -3;
			break;
	}

	return rc;
}

COLOR_CORRECT Camera_GetColorCorrection(HWND hdlg, WND_INFO *wnd, double *rval) {
	static char *rname = "Camera_GetColorCorrection";

	DCX_CAMERA *dcx;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL /* || ! wnd->bCamera */) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_GetColorCorrection(dcx, rval);
			break;

		case TL:
		default:
			rc = -3;
			break;
	}

	return rc;
}


/* ===========================================================================
-- Returns statistics on the ring buffers
--
-- Usage: int Camera_GetRingInfo(HWND hdlg, WND_INFO *wnd, RING_INFO *info);
--
-- Inputs: hdlg    - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd     - handle to the main information structure
--
-- Output: writes a file unless cancelled
--
-- Return: 0 if successful; otherwise error code
--           1 ==> bad parameters or camera is not active
--           2 ==> info was NULL
=========================================================================== */
int Camera_GetRingInfo(HWND hdlg, WND_INFO *wnd, RING_INFO *info) {
	static char *rname = "Camera_GetRingInfo";

	int rc;
	DCX_CAMERA *dcx;
	TL_CAMERA *tl;

	/* Make sure we have valid structures and an active camera */
	if (wnd == NULL) wnd = main_wnd;

	/* Useless call if no info ... return with error or set default return values */
	if (info == NULL) return 2;
	info->nSize = 1; info->nValid = 0; info->iLast = 0; info->iShow = 0;

	/* Have to have a camera enable to even bother asking */
	if (wnd == NULL || ! wnd->bCamera) return 1;

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			DCx_GetRingInfo(dcx, &info->nSize, &info->nValid, &info->iLast, &info->iShow);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			TL_GetRingInfo(tl, &info->nSize, &info->nValid, &info->iLast, &info->iShow);
			break;
		default:
			break;
	}

	return rc;
}	


/* ===========================================================================
-- Allocate (or deallocate) ring buffer for images
--
-- Usage: int Camera_SetBufferSize(HWND hdlg, WND_INFO *wnd, int nBuf);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--         nBuf - number of buffers to allocate in the ring (DCX_MAX_RING_SIZE)
--                constrained within limits [1, DCX_MAX_RING_SIZE]
--
-- Output: Stops processing for a moment, changes buffers, and restarts
--
-- Return: Number of buffers or 0 on fatal errors; minimum number is 1
--
-- Note: A request can be ignored and will return previous size
=========================================================================== */
int Camera_SetRingBufferSize(HWND hdlg, WND_INFO *wnd, int nBuf) {
	static char *rname = "Camera_SetRingBufferSize";

	int rc;
	DCX_CAMERA *dcx;
	TL_CAMERA *tl;

	/* Have to have a camera enable to even bother asking */
	if (wnd == NULL || ! wnd->bCamera) return 0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_SetRingBufferSize(dcx, nBuf);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SetRingBufferSize(tl, nBuf);
			break;
		default:
			rc = 0;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Save the most recent image as an image file
--
-- Usage: int Camera_SaveImage(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd  - handle to the main information structure
--
-- Output: writes a file with current image (unless cancelled)
--
-- Return: 0 if successful; otherwise error code
--           1 ==> bad parameters or camera is not active
=========================================================================== */
int Camera_SaveImage(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_SaveCurrentFrame";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	/* parameters for querying a pathname */
	static char local_dir[PATH_MAX]="";		/* Directory -- keep for multiple calls */
	char pathname[PATH_MAX];
	OPENFILENAME ofn;

	/* If wnd not give, use one from global variables */
	if (wnd == NULL) wnd = main_wnd;

	/* Verify structure and that we have a camera */
	if (wnd == NULL || ! wnd->bCamera) return 1;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	rc = 0;

	/* Stop the video if running (SOFTWARE trigger for moment) */
	if (wnd->LiveVideo) Camera_SetTrigMode(hdlg, wnd, TRIG_SOFTWARE, -1);
	wnd->LiveVideo = FALSE;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			DCx_SaveImage(dcx, NULL, IMAGE_BMP);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;

			/* Get a save-as filename */
			strcpy_s(pathname, sizeof(pathname), "image");		/* Pathname must be initialized with a value (even if just '\0) */
			ofn.lStructSize       = sizeof(OPENFILENAME);
			ofn.hwndOwner         = hdlg;
			ofn.lpstrTitle        = "Save bitmap image";
			ofn.lpstrFilter       = "bitmap data (*.bmp)\0*.bmp\0All files (*.*)\0*.*\0\0";
			ofn.lpstrCustomFilter = NULL;
			ofn.nMaxCustFilter    = 0;
			ofn.nFilterIndex      = 1;
			ofn.lpstrFile         = pathname;				/* Full path */
			ofn.nMaxFile          = sizeof(pathname);
			ofn.lpstrFileTitle    = NULL;						/* Partial path */
			ofn.nMaxFileTitle     = 0;
			ofn.lpstrDefExt       = "bmp";
			ofn.lpstrInitialDir   = (*local_dir=='\0' ? "." : local_dir);
			ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

			/* Query a filename ... if abandoned, just return now with no complaints */
			if (! GetSaveFileName(&ofn)) return 1;

			/* Save the directory for the next time */
			strcpy_s(local_dir, sizeof(local_dir), pathname);
			local_dir[ofn.nFileOffset-1] = '\0';					/* Save for next time! */

			rc = TL_SaveBMPFile(camera, pathname, 0);
			break;

		default:
			rc = 2;
			break;
	}

	/* Re-enable video if halted for save */
	if (GetDlgItemCheck(hdlg, IDB_LIVE)) {
		Camera_SetTrigMode(hdlg, wnd, TRIG_FREERUN, 0);
		wnd->LiveVideo = TRUE;
	}

	return rc;
}

