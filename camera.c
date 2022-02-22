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
static int GetPreferredImageFormat(HWND hdlg);

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

	if (wnd == NULL) return 1;

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

	if (wnd == NULL) return 0;
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
	if (wnd == NULL) return 0.0;
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
	if (wnd == NULL) return 1;

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
	if (wnd == NULL) return 1;

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
	if (wnd == NULL) return 1;

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

	if (wnd == NULL) return 1;

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
	if (wnd == NULL) return 1;
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
	if (wnd == NULL) return 0.0;
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
	if (wnd == NULL) return 1;
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
	if (wnd == NULL) return 0.0;

	if (wnd->Camera.driver != DCX) return 0.0;

	dcx = (DCX_CAMERA *) wnd->dcx;
	return DCx_GetGamma(dcx);
}


/* ===========================================================================
-- Alternate routine to render a specific image frame in the buffer to a window
-- Live images are processed in the threads
--
-- Usage: int Camera_RenderFrame(HWND hdlg, WND_INFO *wnd, int frame, HWND hwnd);
--
-- Inputs: hdlg  - calling dialog box (or NULL)
--         wnd   - pointer to valid window information
--         frame - index of frame to image (0 = current)
--                 will be limited to allowed range
--         hwnd  - window where image is to be rendered
=========================================================================== */
int Camera_RenderFrame(HWND hdlg, WND_INFO *wnd, int frame, HWND hwnd) {
	static char *rname = "Camera_RenderFrame";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	if (! IsWindow(hwnd)) return 0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_RenderFrame(dcx, frame, hwnd);
			GenerateCrosshair(wnd, hwnd);
			GenerateCrosshair(wnd, hwnd);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_RenderFrame(camera, frame, hwnd);
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
-- Usage: Camera_Trigger(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg    - calling dialog box (or NULL)
--         wnd     - pointer to valid window information
--
-- Output: Triggers camera immediately (last time if FREERUN)
--
-- Return: 0 if successful, !0 otherwise (see individual)
--
-- Notes: msWait < 0 will wait until there is an image captured
--               = 0 returns immediately but still triggers the capture
--        If trigger mode was FREERUN, will be set to SOFTWARE after call
=========================================================================== */
int Camera_Trigger(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_Trigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_Trigger(dcx);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_Trigger(camera);
			break;
		default:
			rc = 0;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Software arm/disarm camera (pending triggers)
--
-- Usage: int Camera_Arm(HWND hdlg, WND_INFO *wnd);
--        int Camera_Disarm(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg   - calling dialog box (or NULL)
--         wnd    - pointer to valid window information
--
-- Output: Arms or disarms camera (with expectation of pending trigger)
--
-- Return: 0 if successful
--           1 ==> wnd invalid
--           other ==> return from camera call that failed
--
-- Notes: While valid for all triggers, intended primarily for TRIG_BURST
--        In TRIG_FREERUN, after disarm, must arm AND trigger to restart
=========================================================================== */
int Camera_Arm(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_Arm";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_Arm(dcx);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_Arm(camera);
			break;
		default:
			rc = 0;
			break;
	}

	return rc;
}

int Camera_Disarm(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_Disarm";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_Disarm(dcx);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_Disarm(camera);
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
-- Usage: TRIGGER_MODE Camera_SetTriggerMode(HWND hdlg, WND_INFO *wnd, TRIGGER_MODE mode, TRIGGER_INFO *info);
--        TRIGGER_MODE Camera_GetTriggerMode(HWND hdlg, WND_INFO *wnd, TRIGGER_INFO *info);
--
-- Inputs: hdlg   - calling dialog box (or NULL)
--         wnd    - pointer to valid window information
--         mode   - one of the allowed triggering modes
--                  CAMERA_TRIG_SOFTWARE, CAMERA_TRIG_FREERUN, CAMERA_TRIG_EXTERNAL
--         info   - structure with details about triggering
--           info.msWait - time to wait before forcing change
--                         <0 ==> wait indefinitely for image
--                          0 ==> return immediately (processing continues in background)
--                         >0 ==> timeout
--
-- Output: Set ==> sets camera triggering mode;
--         Get ==> if info !NULL, returns also details about triggering mode
--
-- Return: Actual mode now set for camera or <0 on error
--
-- Notes: TRIG_BURST is really a variant of TRIG_FREERUN.  Concept within TL
--        language is that the camera is set up for continuous AUTO trigger,
--        but is not armed or triggered by this call.  A subsequent call to 
--        Camera_Arm() and then Camera_Trigger() starts acquisition, which
--        is termimated by a Camera_Disarm() call.  TRIG_BURST can be used
--        to differentiate calls to Arm, Disarm, and Trigger appropriately.
=========================================================================== */
TRIGGER_MODE Camera_SetTriggerMode(HWND hdlg, WND_INFO *wnd, TRIGGER_MODE mode, TRIGGER_INFO *info) {
	static char *rname = "Camera_SetTriggerMode";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_SetTriggerMode(dcx, mode, info);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SetTriggerMode(camera, mode, info);
			break;

		default:
			rc = -3;
			break;
	}

	return rc;
}


TRIGGER_MODE Camera_GetTriggerMode(HWND hdlg, WND_INFO *wnd, TRIGGER_INFO *info) {
	static char *rname = "Camera_GetTriggerMode";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_GetTriggerMode(dcx, info);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetTriggerMode(camera, info);
			break;

		default:
			rc = -3;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Set number of frames per trigger (allow 0 for infinite)
--
-- Usage: int Camera_SetFramesPerTrigger(HWND hdlg, WND_INFO *wnd, int frames);
--        int Camera_GetFramesPerTrigger(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg   - calling dialog box (or NULL)
--         wnd    - pointer to valid window information
--         frames - # of frames per trigger, or 0 for infinite
--
-- Output: Sets camera triggering count
--
-- Return: 0 if successful, error from calls otherwise
--
- Note: The value will only be set if in TRIG_SOFTWARE or TRIG_EXTERNAL
--       modes.  But value will be stored in the internal info in any case
=========================================================================== */
int Camera_GetFramesPerTrigger(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetFramesPerTrigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_GetFramesPerTrigger(dcx);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetFramesPerTrigger(camera);
			break;

		default:
			rc = -3;
			break;
	}

	return rc;
}

int Camera_SetFramesPerTrigger(HWND hdlg, WND_INFO *wnd, int frames) {
	static char *rname = "Camera_SetFramesPerTrigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return -1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_SetFramesPerTrigger(dcx, frames);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SetFramesPerTrigger(camera, frames);
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
	if (wnd == NULL) return -1;

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
	if (wnd == NULL) return -1;

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
	info->nBuffers = 1; info->nValid = 0; info->iLast = 0; info->iShow = 0;

	/* Have to have a camera enable to even bother asking */
	if (wnd == NULL) return 1;

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			DCx_GetRingInfo(dcx, &info->nBuffers, &info->nValid, &info->iLast, &info->iShow);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			TL_GetRingInfo(tl, &info->nBuffers, &info->nValid, &info->iLast, &info->iShow);
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
	if (wnd == NULL) return 0;

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
-- Determine formats that camera supports for writing
--
-- Usage: int Camera_GetSaveFormatFlag(HWND hdlg, WND_INFO *wnd);
--
-- Inputs: hdlg - pointer to the window handle
--         wnd  - handle to the main information structure
--
-- Output: none
--
-- Return: Bit-wise flags giving camera capabilities
--				 FL_BMP | FL_JPG | FL_RAW | FL_BURST (unique)
--         0 on errors (no capabilities)
=========================================================================== */
int Camera_GetSaveFormatFlag(HWND hdlg, WND_INFO *wnd) {
	static char *rname = "Camera_GetSaveFormatFlag";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return 0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			rc = DCx_GetSaveFormatFlag(dcx);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetSaveFormatFlag(camera);
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
-- Usage: int Camera_SaveBMPImage(HWND hdlg, WND_INFO *wnd, int dflt_format);
--
-- Inputs: hdlg   - pointer to the window handle (will use IDC_CAMERA_LIST element)
--         wnd    - handle to the main information structure
--         dflt_format - default save format (can be overwitten in dialog box)
--
-- Output: writes a file with current image (unless cancelled)
--
-- Return: 0 if successful; otherwise error code
--           1 ==> bad parameters or camera is not active
=========================================================================== */
int Camera_SaveImage(HWND hdlg, WND_INFO *wnd, int dflt_format) {
	static char *rname = "Camera_SaveImage";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;

	int i, flags;
	size_t len;
	int format_flags, format_index, nformats;
	char *aptr, *dfltExt, filter[256];

	/* information for creating filter string */
	static struct {
		int flag;
		char *text,	*wild, *ext;
	} filters[]	= {
		{FL_BMP, "Bitmap format (*.bmp)",	"*.bmp", "bmp"},
		{FL_RAW, "Raw format (*.raw)",		"*.raw", "raw"},
		{FL_PNG, "PNG format (*.png)",		"*.png", "png"},
		{FL_JPG, "JPEG format (*.jpg)",		"*.jpg", "jpg"},
		{0xFFFF,	"All files (*.*)",			"*.*",	"bmp"}				/* Will also become default extension of bmp */
	};
	static struct {
		char *ext;
		int flag;
	} exts[] = { {".bmp", FL_BMP}, {".raw", FL_RAW}, {".png", FL_PNG}, {".jpg", FL_JPG}, {".jpeg", FL_JPG} };

	/* parameters for querying a pathname */
	static char local_dir[PATH_MAX]="";		/* Directory -- keep for multiple calls */
	char pathname[PATH_MAX];
	OPENFILENAME ofn;

	/* If wnd not give, use one from global variables */
	if (wnd == NULL) wnd = main_wnd;

	/* Verify structure and that we have a camera */
	if (wnd == NULL) return 1;
	if (hdlg == NULL) hdlg = wnd->main_hdlg;

	rc = 0;

	/* Stop the video if running (SOFTWARE trigger for moment) */
	if (wnd->LiveVideo) Camera_SetTriggerMode(hdlg, wnd, TRIG_SOFTWARE, NULL);
	wnd->LiveVideo = FALSE;

	/* Generate the filter string to include allowed extensions */
	format_flags  = Camera_GetSaveFormatFlag(hdlg, wnd);
	format_index  = 0;		/* Index of dflt_format in list.  If stays 0, okay for call */

	/* Generate filter string ... looks like "bitmap image (*.bmp)\0*.bmp\0raw camera (*.raw)\0*.raw\0All files (*.*)\0*.*\0\0" */
	aptr = filter; len = sizeof(filter);
	dfltExt = NULL;
	format_flags |= 0x8000;								/* Ensure we match "all files" entry */
	nformats = 0;
	for (i=0; i<sizeof(filters)/sizeof(filters[0]); i++) {
		if (format_flags & filters[i].flag) {
			nformats++;
			strcpy_s(aptr, len, filters[i].text); len -= strlen(aptr)+1; aptr += strlen(aptr)+1;
			strcpy_s(aptr, len, filters[i].wild); len -= strlen(aptr)+1; aptr += strlen(aptr)+1;
			if (dfltExt == NULL) dfltExt = filters[i].ext;	/* Ensure we have one! */
			if (filters[i].flag == dflt_format) {
				format_index = nformats;
				dfltExt = filters[i].ext;
			}
		}
	}
	*aptr = '\0';													/* final null to terminate list */

	/* Get a save-as filename */
	strcpy_s(pathname, sizeof(pathname), "image");		/* Pathname must be initialized with a value (even if just '\0) */
	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = hdlg;
	ofn.lpstrTitle        = "Save image";
	ofn.lpstrFilter       = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter    = 0;
	ofn.nFilterIndex      = format_index;
	ofn.lpstrFile         = pathname;				/* Full path */
	ofn.nMaxFile          = sizeof(pathname);
	ofn.lpstrFileTitle    = NULL;						/* Partial path */
	ofn.nMaxFileTitle     = 0;
	ofn.lpstrDefExt       = dfltExt;
	ofn.lpstrInitialDir   = (*local_dir=='\0' ? "." : local_dir);
	ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	/* Query a filename ... if abandoned, just return now with no complaints */
	if (! GetSaveFileName(&ofn)) { rc = 1; goto ExitSave; }

	/* Save directory for subsequent calls */
	strcpy_s(local_dir, sizeof(local_dir), pathname);
	local_dir[ofn.nFileOffset-1] = '\0';

	/* Determine the file format from the extension */
	flags = FL_BMP;												/* Default format is bitmap */
	aptr = pathname+strlen(pathname)-1;
	while (*aptr != '.' && aptr != pathname) aptr--;
	if (*aptr == '.') {											/* Modify to actual file type */
		for (i=0; i<sizeof(exts)/sizeof(exts[0]); i++) {
			if (_stricmp(aptr, exts[i].ext) == 0) flags = exts[i].flag;
		}
	}

	/* Send to appropriate driver routine */
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			DCx_SaveImage(dcx, pathname, -1, flags);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SaveImage(camera, pathname, -1, flags);
			break;

		default:
			rc = 2;
			break;
	}

ExitSave:
	/* Re-enable video if halted for save */
	if (GetDlgItemCheck(hdlg, IDB_LIVE)) {
		Camera_SetTriggerMode(hdlg, wnd, TRIG_FREERUN, 0);
		wnd->LiveVideo = TRUE;
	}

	return rc;
}

/* ===========================================================================
-- Save all valid images that would have been collected in burst run
--
-- Usage: Camera_SaveBurstImages(HWND hdlg, WND_INFO *wnd, int format);
--
-- Inputs: hdlg   - pointer to the window handle
--         wnd    - pointer to current descriptor
--         format - format to save data (defaults to FL_BMP)
--
-- Output: Saves images as a series of bitmaps
--
-- Return: 0 ==> successful
--         1 ==> rings are not enabled in the code
--         2 ==> buffers not yet allocated or no data
--         3 ==> save abandoned by choice in FileOpen dialog
=========================================================================== */
#ifndef USE_RINGS

int Camera_SaveBurstImages(HWND hdlg, WND_INFO *wnd, int format) {
	static char *rname="Camera_SaveBurstImages";

	Beep(300,200);
	return 1;
}

#else

int Camera_SaveBurstImages(HWND hdlg, WND_INFO *wnd, int format) {
	static char *rname="Camera_SaveBurstImages";

	int rc;
	BOOL wasLive;
	char pattern[PATH_MAX], *aptr;
	RING_INFO rings;
	OPENFILENAME ofn;

	DCX_CAMERA *dcx;
	TL_CAMERA *tl;

	static char local_dir[PATH_MAX] = "";

	/* Verify that we have buffers to save */
	if (Camera_GetRingInfo(hdlg, wnd, &rings) != 0 || rings.nValid <= 0) return 2;

	/* Get the pattern for the save (directory and name without the extension */
	strcpy_m(pattern, sizeof(pattern), "basename");			/* Default name must be initialized with something */
	ofn.lStructSize       = sizeof(ofn);
	ofn.hwndOwner         = hdlg;
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

	/* Get filename and maybe abort */
	fprintf(stderr, "Requesting filename ... "); fflush(stderr);
	if (! GetSaveFileName(&ofn)) return 3;						/* If aborted, just skip and go back to re-enabling the image */
	fprintf(stderr, "returned\n"); fflush(stderr);

	/* Save the directory for the next time */
	strcpy_m(local_dir, sizeof(local_dir), pattern);
	local_dir[ofn.nFileOffset-1] = '\0';						/* Save for next time! */

	aptr = pattern + strlen(pattern) - 4;						/* Should be the ".csv" */
	if (_stricmp(aptr, ".csv") == 0) *aptr = '\0';

	/* Are we triggering freerun?  Stop now and restart at end */
	wasLive = Camera_GetTriggerMode(hdlg, wnd, NULL) == TRIG_FREERUN;
	if (wasLive) {																	/* Stop now */
		Camera_SetTriggerMode(hdlg, wnd, TRIG_SOFTWARE, 0);			/* Put in software mode */
		Sleep(1000);																/* Wait 1 second to end */
	}

	/* Now we have to switch based on the cameras */
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			rc = DCx_SaveBurstImages(dcx, pattern, format);
			break;

		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SaveBurstImages(tl, pattern, format);
			break;

		default:
			rc = 2;
			break;
	}

	/* If we were live before, restart freerun mode */
	if (wasLive) Camera_SetTriggerMode(hdlg, wnd, TRIG_FREERUN, 0);

	return rc;
}

#endif
