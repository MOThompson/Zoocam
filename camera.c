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
-- Get information about the camera
--
-- Usage: int Camera_GetCameraInfo(WND_INFO *wnd, CAMERA_INFO *info);
--
-- Inputs: wnd     - pointer to valid window information
--         info    - pointer to structure to receive camera information
--
-- Output: *info (if not NULL)
--
-- Return: 0 if successful, 1 if no camera initialized
=========================================================================== */
int Camera_GetCameraInfo(WND_INFO *wnd, CAMERA_INFO *info) {
	static char *rname = "Camera_GetCameraInfo";

	TL_CAMERA  *tl;
	DCX_CAMERA *dcx;
	int rc;
	BOOL bServerRequest;

	if (info != NULL) memset(info, 0, sizeof(*info));		/* Otherwise, empty out */

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;										/* Nothing initialized */

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->Camera.details;
			rc = DCx_GetCameraInfo(dcx, info);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetCameraInfo(tl, info);
			break;
		default:
			if (info != NULL) info->type = CAMERA_UNKNOWN;
			rc = 1;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Get the exposure setting for the camera
--
-- Usage: int Camera_GetExposureParms(WND_INFO *wnd, 
--												  double *ms_min, double *ms_max, double *ms_incr);
--
-- Inputs: wnd     - pointer to valid window information
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
int Camera_GetExposureParms(WND_INFO *wnd, double *ms_min, double *ms_max, double *ms_incr) {
	static char *rname = "Camera_GetExposureParms";

	TL_CAMERA  *camera;
	DCX_CAMERA *dcx;
	int rc;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: double Camera_GetExposure(WND_INFO *wnd);
--
-- Inputs: wnd   - pointer to valid window information
--
-- Output: none
--
-- Return: Current exposure time in milliseconds
=========================================================================== */
double Camera_GetExposure(WND_INFO *wnd) {
	static char *rname = "Camera_GetExposure";

	double rval;
	TL_CAMERA  *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: double Camera_SetExposure(WND_INFO *wnd, double ms_expose);
--
-- Inputs: wnd  - handle to the main information structure
--         ms_exposure - desired exposure time in milliseconds
--
-- Output: sets exposure on active camera within allowable bounds
--
-- Return: 0 on error, otherwise actual exposure time instantiated in ms
=========================================================================== */
double Camera_SetExposure(WND_INFO *wnd, double ms_expose) {
	static char *rname = "Camera_SetExposure";

	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 0.0;

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

	/* Server can't modify dialog box, so help here */
	/* Update exposure, but also deal with fact that framerate may change */
	if (wnd->hdlg != NULL && IsWindow(wnd->hdlg)) {
		SendMessage(wnd->hdlg, WMP_SHOW_EXPOSURE, 0, 0);
		SendMessage(wnd->hdlg, WMP_SHOW_FRAMERATE, 0, 0);
	}

	return ms_expose;
}				


/* ===========================================================================
-- Set the RGB gains ... each camera may do very differnt things
--
-- Usage: int Camera_Set_RGB_Gain(WND_INFO *wnd, 
--											 enum {R_CHAN, G_CHAN, B_CHAN} channel, 
--											 enum {IS_SLIDER, IS_VALUE} entry, 
--											 double value);
--
-- Inputs: wnd     - handle to the main information structure
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
int Camera_SetGains(WND_INFO *wnd, GAIN_CHANNEL channel, ENTRY_TYPE entry, double value) {
	static char *rname = "Camera_SetGains";

	int rc, ival;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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

	/* Server can't modify dialog box, so help here */
	if (bServerRequest) SendMessage(wnd->hdlg, WMP_SHOW_GAINS, 0, 0);

	return rc;
}


/* ===========================================================================
-- Reset RGB gains to either default color values (original when camera opened)
-- or values that would be most "neutral" with respect to channel gains
--
-- Usage: int Camera_ResetGains(WND_INFO *wnd, BOOL rgb);
--
-- Inputs: wnd     - handle to the main information structure
--
-- Output: resets gains of all channels to defaults if possible
--
-- Return: 0 if successful; otherwise error code
--           1 ==> camera is not active
=========================================================================== */
int Camera_ResetGains(WND_INFO *wnd, GAIN_RESET_MODE rgb) {
	static char *rname = "Camera_ResetGains";

	int rc;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

	rc = 0;
	switch (wnd->Camera.driver) {
		case DCX:
		{
			int master, red, green, blue;
			DCX_CAMERA *dcx;

			dcx = wnd->dcx;
			if (RGB_GAIN == rgb) {
				DCx_GetDfltRGBGains(dcx, &master, &red, &green, &blue);
			} else {
				master = 10; red = green = blue = 50;
			}
			DCx_SetRGBGains(dcx, master, red, green, blue);
		}
		break;
		case TL:
		{
			double master, red, green, blue;
			TL_CAMERA *camera;

			camera = (TL_CAMERA *) wnd->Camera.details;
			if (RGB_GAIN == rgb) {
				TL_GetMasterGainInfo(camera, NULL, &master, NULL, NULL);
				TL_GetDfltRGBGains(camera, &red, &green, &blue);
			} else {
				master = 0;
				red = green = blue = 1.0;
			}
			TL_SetMasterGain(camera, master);
			TL_SetRGBGains(camera, red, green, blue);
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
-- Usage: int Camera_GetGains(WND_INFO *wnd, double values[4], double slider[4]);
--
-- Inputs: wnd       - handle to the main information structure
--         values[4] - array to receive numerical values for text entry boxes
--         slider[4] - array to receive fractions [0.0,1.0] for setting sliders
--                     Order is master, red, green, blue in each array
--
-- Output: Queries gains and returns numbers to display
--
-- Return: 0 if successful; otherwise error code
--           1 ==> camera is not active
=========================================================================== */
int Camera_GetGains(WND_INFO *wnd, double values[4], double slider[4]) {
	static char *rname = "Camera_GetGains";

	int i, rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;
	double rval[4], db_min, db_max;
	int ival[4];

	/* Initial return values */
	if (values != NULL) for (i=0; i<4; i++) values[i] = 0.0;
	if (slider != NULL) for (i=0; i<4; i++) slider[i] = 0.0;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: Camera_GetFPSActual(WND_INFO *wnd);
--
-- Inputs: wnd     - pointer to valid window information
--
-- Output: none
--
-- Return: Estimate of framerate from camera
=========================================================================== */
double Camera_GetFPSActual(WND_INFO *wnd) {
	static char *rname = "Camera_GetFPSActual";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	BOOL bServerRequest;
	double fps;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: double Camera_SetFPSControl(WND_INFO *wnd, double fps);
--
-- Inputs: wnd  - handle to the main information structure
--         fps  - desired rate
--
-- Output: sets fps if possible
--
-- Return: Actual value set, or 0 if error
=========================================================================== */
double Camera_SetFPSControl(WND_INFO *wnd, double fps) {
	static char *rname = "Camera_SetFPSControl";

	DCX_CAMERA *dcx;
	TL_CAMERA *tl;
	BOOL bServerRequest;

	/* Make sure we have valid structures and an active camera */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

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

	/* Server can't modify dialog box, so help here */
	if (bServerRequest) SendMessage(wnd->hdlg, WMP_SHOW_FRAMERATE, 0, 0);

	return fps;
}

/* ===========================================================================
-- Return the framerate setting on cameras supporting
--
-- Usage: double Camera_GetFPSControl(WND_INFO *wnd);
--
-- Inputs: wnd  - handle to the main information structure
--
-- Output: none
--
-- Return: - current framerate setting in fps if supported or <= 0 on error
=========================================================================== */
double Camera_GetFPSControl(WND_INFO *wnd) {
	static char *rname = "Camera_GetFPSControl";

	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;
	double fps;

	/* Make sure we have valid structures and an active camera */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 0.0;

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
-- Query the framerate limit (if implemented)
--
-- Usage: double Camera_GetFPSLimit(WND_INFO *wnd);
--
-- Inputs: wnd - handle to the main information structure
--
-- Output: none
--
-- Return: Returns 0 if driver cannot limit FPS, or current vaiue 
--                <0 on errors
=========================================================================== */
double Camera_GetFPSLimit(WND_INFO *wnd) {
	static char *rname = "Camera_GetFPSLimit";

	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;
	double fps;

	/* Make sure we have valid structures and an active camera */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 0.0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			fps = DCx_GetFPSLimit(dcx);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			fps = TL_GetFPSLimit(camera);
			break;
		default:
			fps = 0.0;
	}

	return fps;
}

/* ===========================================================================
-- Set the framerate limit (if implemented)
--
-- Usage: double Camera_SetFPSLimit(WND_INFO *wnd, double fps);
--
-- Inputs: wnd - handle to the main information structure
--         fps - limit to saving of frames to buffers
--
-- Output: none
--
-- Return: Returns 0 if driver cannot limit FPS, or vaiue inserted
--                <0 on errors
--
-- Note: This is different from setting the framerate of the camera.
--       Image processing skipped for images arriving <1/fps since previous
=========================================================================== */
double Camera_SetFPSLimit(WND_INFO *wnd, double fps) {
	static char *rname = "Camera_SetFPSLimit";

	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures and an active camera */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 0.0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			fps = DCx_SetFPSLimit(dcx, fps);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			fps = TL_SetFPSLimit(camera, fps);
			break;
		default:
			fps = 0.0;
	}

	return fps;
}

/* ===========================================================================
-- Set the gamma factor on cameras supporting
--
-- Usage: double Camera_SetGamma(WND_INFO *wnd, double gamma);
--
-- Inputs: wnd  - handle to the main information structure
--         gamma - desired value ... with 1.0 as neutral
--
-- Output: sets gamma factor on active camera
--
-- Return: value actually set or 0.0 on error
=========================================================================== */
double Camera_SetGamma(WND_INFO *wnd, double gamma) {
	static char *rname = "Camera_SetGamma";

	double rval;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

	if (wnd->Camera.driver != DCX) return 2;
	dcx = wnd->dcx;

	rval = DCx_SetGamma(dcx, gamma);

	/* Server can't modify dialog box, so help here */
	if (bServerRequest) SendMessage(wnd->hdlg, WMP_SHOW_GAMMA, 0, 0);

	return rval;
}

/* ===========================================================================
-- Return the gamma factor on cameras supporting
--
-- Usage: double Camera_GetGamma(WND_INFO *wnd, double *gamma);
--
-- Inputs: wnd  - handle to the main information structure
--
-- Output: none
--
-- Return: gamma value from camera, or 0 on error
=========================================================================== */
double Camera_GetGamma(WND_INFO *wnd) {
	static char *rname = "Camera_GetGamma";

	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 0.0;

	if (wnd->Camera.driver != DCX) return 0.0;

	dcx = (DCX_CAMERA *) wnd->dcx;
	return DCx_GetGamma(dcx);
}


/* ===========================================================================
-- Alternate routine to render a specific image frame in the buffer to a window
-- Live images are processed in the threads
--
-- Usage: int Camera_RenderFrame(WND_INFO *wnd, int frame, HWND hwnd);
--
-- Inputs: wnd   - pointer to valid window information
--         frame - index of frame to image (-1 ==> for most recent)
--                 will be limited to allowed range
--         hwnd  - window where image is to be rendered
=========================================================================== */
int Camera_RenderFrame(WND_INFO *wnd, int frame, HWND hwnd) {
	static char *rname = "Camera_RenderFrame";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

	if (! IsWindow(hwnd)) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_RenderFrame(dcx, frame, hwnd);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_RenderFrame(camera, frame, hwnd);
			break;
		default:
			break;
	}

	GenerateCrosshair(wnd, hwnd);
	return 0;
}


/* ===========================================================================
-- Force trigger camera
--
-- Usage: Camera_Trigger(WND_INFO *wnd);
--
-- Inputs: wnd     - pointer to valid window information
--
-- Output: Triggers camera immediately (last time if FREERUN)
--
-- Return: 0 if successful, !0 otherwise (see individual)
--           3 ==> not enabled
--
-- Notes: msWait < 0 will wait until there is an image captured
--               = 0 returns immediately but still triggers the capture
--        If trigger mode was FREERUN, will be set to SOFTWARE after call
=========================================================================== */
int Camera_Trigger(WND_INFO *wnd) {
	static char *rname = "Camera_Trigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	int rc;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: TRIG_ARM_ACTION Camera_Arm(WND_INFO *wnd, TRIG_ARM_ACTION action);
--
-- Inputs: wnd    - pointer to valid window information
--         action - one of TRIG_ARM_QUERY, TRIG_ARM, TRIG_DISARM (dflt=query)
--
-- Output: Arms or disarms camera (with expectation of pending trigger)
--
-- Return: Trigger arm state
--				 TRIG_ARM_UNKNOWN on error 
--				   otherwise TRiG_ARM or TRIG_DISARM
--
-- Notes: While valid for all triggers, intended primarily for TRIG_BURST
--        In TRIG_FREERUN, after disarm, must arm AND trigger to restart
=========================================================================== */
TRIG_ARM_ACTION Camera_Arm(WND_INFO *wnd, TRIG_ARM_ACTION action) {
	static char *rname = "Camera_Arm";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->dcx;
			rc = DCx_Arm(dcx, action);
			break;
		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_Arm(camera, action);
			break;
		default:
			rc = 0;
			break;
	}

	/* Server can't modify dialog box, so help here */
	if (bServerRequest) SendMessage(wnd->hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);			/* Too many interdependencies */

	return rc;
}

/* ===========================================================================
-- Set/Query the triggering mode for the camera
--
-- Usage: TRIGGER_MODE Camera_SetTriggerMode(WND_INFO *wnd, TRIGGER_MODE mode, TRIGGER_INFO *info);
--        TRIGGER_MODE Camera_GetTriggerMode(WND_INFO *wnd, TRIGGER_INFO *info);
--
-- Inputs: wnd    - pointer to valid window information
--         mode   - one of the allowed triggering modes
--                  TRIG_SOFTWARE, TRIG_FREERUN, TRIG_EXTERNAL, TRIG_SS or TRIG_BURST
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
TRIGGER_MODE Camera_SetTriggerMode(WND_INFO *wnd, TRIGGER_MODE mode, TRIGGER_INFO *info) {
	static char *rname = "Camera_SetTriggerMode";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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

	/* Server can't modify dialog box, so help here */
	if (bServerRequest) SendMessage(wnd->hdlg, WMP_UPDATE_TRIGGER_BUTTONS, 0, 0);

	return rc;
}


TRIGGER_MODE Camera_GetTriggerMode(WND_INFO *wnd, TRIGGER_INFO *info) {
	static char *rname = "Camera_GetTriggerMode";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: int Camera_SetFramesPerTrigger(WND_INFO *wnd, int frames);
--        int Camera_GetFramesPerTrigger(WND_INFO *wnd);
--
-- Inputs: wnd    - pointer to valid window information
--         frames - # of frames per trigger, or 0 for infinite
--
-- Output: Sets camera triggering count
--
-- Return: 0 if successful, error from calls otherwise
--
-- Note: Value set internally always, but only passed to camera in 
--       TRIG_SOFTWARE, TRIG_EXTERNAL, and TRIG_SS modes.
=========================================================================== */
int Camera_GetFramesPerTrigger(WND_INFO *wnd) {
	static char *rname = "Camera_GetFramesPerTrigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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

int Camera_SetFramesPerTrigger(WND_INFO *wnd, int frames) {
	static char *rname = "Camera_SetFramesPerTrigger";

	DCX_CAMERA *dcx;
	TL_CAMERA  *camera;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: COLOR_CORRECT Camera_SetColorCorrection(WND_INFO *wnd, COLOR_CORRECT mode, double rval);
--        COLOR_CORRECT Camera_SetColorCorrection(WND_INFO *wnd, double &rval);
--
-- Inputs: wnd    - pointer to valid window information
--         mode   - one of the allowed color correction modes
--         rval   - strength of color corrections (camera dependent)
--
-- Output: Sets color correction mode
--
-- Return: Actual color correction mode set for camera or <0 on error
=========================================================================== */
COLOR_CORRECT Camera_SetColorCorrection(WND_INFO *wnd, COLOR_CORRECT mode, double rval) {
	static char *rname = "Camera_SetColorCorrection";

	DCX_CAMERA *dcx;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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

COLOR_CORRECT Camera_GetColorCorrection(WND_INFO *wnd, double *rval) {
	static char *rname = "Camera_GetColorCorrection";

	DCX_CAMERA *dcx;
	BOOL bServerRequest;
	int rc;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Usage: int Camera_GetRingInfo(WND_INFO *wnd, RING_INFO *info);
--
-- Inputs: wnd     - handle to the main information structure
--
-- Output: writes a file unless cancelled
--
-- Return: 0 if successful; otherwise error code
--           1 ==> bad parameters or camera is not active
--           2 ==> info was NULL
=========================================================================== */
int Camera_GetRingInfo(WND_INFO *wnd, RING_INFO *info) {
	static char *rname = "Camera_GetRingInfo";

	int rc;
	DCX_CAMERA *dcx;
	TL_CAMERA *tl;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

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
-- Usage: int Camera_SetBufferSize(WND_INFO *wnd, int nBuf);
--
-- Inputs: wnd  - handle to the main information structure
--         nBuf - number of buffers to allocate in the ring (DCX_MAX_RING_SIZE)
--                constrained within limits [1, DCX_MAX_RING_SIZE]
--
-- Output: Stops processing for a moment, changes buffers, and restarts
--
-- Return: Number of buffers or 0 on fatal errors; minimum number is 1
--
-- Note: A request can be ignored and will return previous size
=========================================================================== */
int Camera_SetRingBufferSize(WND_INFO *wnd, int nBuf) {
	static char *rname = "Camera_SetRingBufferSize";

	int rc;
	DCX_CAMERA *dcx;
	TL_CAMERA *tl;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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

	/* Server can't modify dialog box, so help here */
	if (bServerRequest) SetDlgItemInt(wnd->hdlg, IDV_RING_SIZE, nBuf, FALSE);

	return rc;
}

/* ===========================================================================
-- Reset the ring buffer counters so the next image will be in location 0
-- Primarily a Client/Server call for burst mode operation.  While other
-- commands may also reset these counters, this routine ensures a reset.
--
-- Usage: int Camera_ResetRingCounters(WND_INFO *wnd);
--
-- Inputs: wnd - handle to the main information structure
--
-- Output: Resets buffers so next image will be 0
--
-- Return: 0 on success
=========================================================================== */
int Camera_ResetRingCounters(WND_INFO *wnd) {
	static char *rname = "Camera_ResetRingCounters";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 0;

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			rc = DCx_ResetRingCounters(dcx);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_ResetRingCounters(camera);
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
-- Usage: int Camera_GetSaveFormatFlag(WND_INFO *wnd);
--
-- Inputs: wnd  - handle to the main information structure
--
-- Output: none
--
-- Return: Bit-wise flags giving camera capabilities
--				 FILE_BMP | FILE_JPG | FILE_RAW | FILE_BURST (unique)
--         0 on errors (no capabilities)
=========================================================================== */
int Camera_GetSaveFormatFlag(WND_INFO *wnd) {
	static char *rname = "Camera_GetSaveFormatFlag";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
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
-- Get information about a specific image
--
-- Usage: int Camera_GetImageInfo(WND_INFO *wnd, int frame, IMAGE_INFO *info);
--
-- Inputs: wnd   - pointer to valid window information
--         frame - index of frame to image (-1 = current)
--                    invalid frame return error (rc = 2)
--         info  - pointer to structure to receive image information
--
-- Output: *info (if not NULL)
--
-- Return: 0 if successful, 
--           1 => no camera initialized
--           2 => frame invalid
=========================================================================== */
int Camera_GetImageInfo(WND_INFO *wnd, int frame, IMAGE_INFO *info) {
	static char *rname = "Camera_GetImageInfo";

	TL_CAMERA  *tl;
	DCX_CAMERA *dcx;
	int rc;
	BOOL bServerRequest;

	if (info != NULL) memset(info, 0, sizeof(*info));		/* Otherwise, empty out */

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;										/* Nothing initialized */

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->Camera.details;
			rc = DCx_GetImageInfo(dcx, frame, info);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetImageInfo(tl, frame, info);
			break;
		default:
			if (info != NULL) info->type = CAMERA_UNKNOWN;
			rc = 1;
			break;
	}

	return rc;
}

/* ===========================================================================
-- Get pointer to raw data for a specific image, and length of that data
--
-- Usage: int Camera_GetImageData(WND_INFO *wnd, int frame, void **image_data, int *length);
--
-- Inputs: wnd        - pointer to valid window information
--         frame      - index of frame to image (-1 = current)
--                        invalid frame return error (rc = 2)
--         image_data - pointer to get a pointer to actual memory location (shared)
--         length     - pointer to get count to # of bytes in the image data
--
-- Output: *image_data - a pointer (UNSIGNED SHORT *) to actual data
--         *length     - number of bytes in the memory buffer
--
-- Return: 0 if successful, 
--           1 => no camera initialized
--           2 => frame invalid
=========================================================================== */
int Camera_GetImageData(WND_INFO *wnd, int frame, void **image_data, int *length) {
	static char *rname = "Camera_GetImageData";

	TL_CAMERA  *tl;
	DCX_CAMERA *dcx;
	int rc;
	BOOL bServerRequest;

	/* Default return values */
	if (image_data != NULL) *image_data = NULL;
	if (length     != NULL) *length = 0;

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;										/* Nothing initialized */

	switch (wnd->Camera.driver) {
		case DCX:
			dcx = (DCX_CAMERA *) wnd->Camera.details;
			rc = DCx_GetImageData(dcx, frame, image_data, length);
			break;
		case TL:
			tl = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_GetImageData(tl, frame, image_data, length);
			break;
		default:
			rc = 1;
			break;
	}

	return rc;
}


/* ===========================================================================
-- Guess file format from extension of a given filename
--
-- Usage: FILE_FORMAT GuessFileFormat(char *path);
--
-- Inputs: path   - a pathname
--
-- Output: none
--
-- Return: Matching format, or FILE_BMP if none match 
=========================================================================== */
FILE_FORMAT GuessFileFormat(char *path) {

	FILE_FORMAT format;
	char *aptr;
	int i;

	static struct {
		char *ext;
		FILE_FORMAT format;
	} exts[] = { {".bmp", FILE_BMP}, {".raw", FILE_RAW}, {".png", FILE_PNG}, {".jpg", FILE_JPG}, {".jpeg", FILE_JPG} };

	/* Set the default return value */
	format = FILE_BMP;

	/* Look for the extension and compare to known types */
	aptr = path + strlen(path)-1;
	while (*aptr != '.' && aptr != path) aptr--;
	if (*aptr == '.') {											/* Modify to actual file type */
		for (i=0; i<sizeof(exts)/sizeof(exts[0]); i++) {
			if (_stricmp(aptr, exts[i].ext) == 0) format = exts[i].format;
		}
	}

	return format;
}


/* ===========================================================================
-- Query for a filename to save image
--
-- Usage: int GetFilename(WND_INFO *wnd, char *path, size_t length, FILE_FORMAT dflt_format, FILE_FORMAT *format);
--
-- Inputs: wnd      - handle to the main information structure
--         path     - variable to receive new pathname
--         length   - number of characters available in path
--         dlft_fmt - default format for the open dialog box
--         format   - variable to receive actual format
--
-- Output: writes file with image (unless cancelled)
--
-- Return: 0 if successful; otherwise error code
--           1 ==> bad parameters or camera is not active
--           2 ==> invalid frame
=========================================================================== */
static int GetFilename(WND_INFO *wnd, char *path, size_t length, FILE_FORMAT dflt_format, FILE_FORMAT *format) {
	static char *rname = "GetFilename";

	int i;
	size_t len;
	int format_flags, format_index, nformats;
	char *aptr, *dfltExt, filter[256];

	/* information for creating filter string */
	static struct {
		int flag;
		FILE_FORMAT format;
		char *text,	*wild, *ext;
	} filters[]	= {
		{FL_BMP, FILE_BMP,  "Bitmap format (*.bmp)",	"*.bmp", "bmp"},
		{FL_RAW, FILE_RAW,  "Raw format (*.raw)",		"*.raw", "raw"},
		{FL_PNG, FILE_PNG,  "PNG format (*.png)",		"*.png", "png"},
		{FL_JPG, FILE_JPG,  "JPEG format (*.jpg)",	"*.jpg", "jpg"},
		{0xFFFF, FILE_DFLT, "All files (*.*)",			"*.*",	"bmp"}				/* Will also become default extension of bmp */
	};

	/* parameters for querying a pathname */
	static char local_dir[PATH_MAX]="";		/* Directory -- keep for multiple calls */
	OPENFILENAME ofn;

	/* Default return values */
	*path   = '\0';
	*format = FILE_BMP;							/* Default format is bitmap */

	/* Generate the filter string to include allowed extensions */
	format_flags  = Camera_GetSaveFormatFlag(wnd);
	format_index  = 0;							/* Index of dflt_format in list.  If stays 0, okay for call */

	/* Generate filter string ... looks like "bitmap image (*.bmp)\0*.bmp\0raw camera (*.raw)\0*.raw\0All files (*.*)\0*.*\0\0" */
	aptr = filter; len = sizeof(filter);
	dfltExt = NULL;
	format_flags |= 0x8000;						/* Ensure we match "all files" entry */
	nformats = 0;
	for (i=0; i<sizeof(filters)/sizeof(filters[0]); i++) {
		if (format_flags & filters[i].flag) {
			nformats++;
			strcpy_s(aptr, len, filters[i].text); len -= strlen(aptr)+1; aptr += strlen(aptr)+1;
			strcpy_s(aptr, len, filters[i].wild); len -= strlen(aptr)+1; aptr += strlen(aptr)+1;
			if (dfltExt == NULL) dfltExt = filters[i].ext;	/* Ensure we have one! */
			if (filters[i].format == dflt_format) {
				format_index = nformats;
				dfltExt = filters[i].ext;
			}
		}
	}
	*aptr = '\0';													/* final null to terminate list */

	/* Get a save-as filename */
	strcpy_s(path, length, "image");					/* Pathname must be initialized (even if just '\0) */
	memset(%ofn, 0, sizeof(ofn));						/* Not static, must be set to zeros */
	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = wnd->hdlg;
	ofn.lpstrTitle        = "Save image";
	ofn.lpstrFilter       = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter    = 0;
	ofn.nFilterIndex      = format_index;
	ofn.lpstrFile         = path;						/* Full path */
	ofn.nMaxFile          = length;
	ofn.lpstrFileTitle    = NULL;						/* Partial path */
	ofn.nMaxFileTitle     = 0;
	ofn.lpstrDefExt       = dfltExt;
	ofn.lpstrInitialDir   = (*local_dir=='\0' ? "." : local_dir);
	ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	/* Query a filename ... if abandoned, just return now with no complaints */
	if (! GetSaveFileName(&ofn)) return 2;

	/* Save directory for subsequent calls */
	strcpy_s(local_dir, sizeof(local_dir), path);
	local_dir[ofn.nFileOffset-1] = '\0';

	/* Determine the file format from the extension */
	*format = GuessFileFormat(path);

	return 0;
}

/* ===========================================================================
-- Save the most recent image as an image file
--
-- Usage: int Camera_SaveImage(WND_INFO *wnd, int frame, char *path, FILE_FORMAT format);
--
-- Inputs: wnd    - handle to the main information structure
--         frame  - frame to save (-1 => last)
--         path   - filename (NULL to query)
--         format - format (if path given) or preferred format if querying
--
-- Output: writes file with image (unless cancelled)
--
-- Return: 0 if successful; otherwise error code
--           1 ==> bad parameters or camera is not active
--           2 ==> invalid frame
=========================================================================== */
int Camera_SaveImage(WND_INFO *wnd, int frame, char *pathname, FILE_FORMAT format) {
	static char *rname = "Camera_SaveImage";

	int rc;
	TL_CAMERA *camera;
	DCX_CAMERA *dcx;
	BOOL bServerRequest;

	char path[PATH_MAX];								/* Local storage for pathname if NULL */

	/* If wnd not give, use one from global variables */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 1;

	rc = 0;

#if 0
	/* Stop the video if running (SOFTWARE trigger for moment) */
	if (wnd->LiveVideo) Camera_SetTriggerMode(wnd, TRIG_SOFTWARE, NULL);
	wnd->LiveVideo = FALSE;
#endif

	/* If path is NULL, query a pathname using local storage */
	if (pathname == NULL || *pathname == '\0') {
		if (GetFilename(wnd, path, sizeof(path), format, &format) != 0) return 3;			/* Aborted */
		pathname = path;
	}

	/* Guess format for the drivers ... though they will ultimately choose a default */
	if (format == FILE_DFLT) format = GuessFileFormat(pathname);

	/* Send to appropriate driver routine */
	switch (wnd->Camera.driver) {
		case DCX:
			dcx = wnd->dcx;
			rc = DCx_SaveImage(dcx, pathname, frame, format);
			break;

		case TL:
			camera = (TL_CAMERA *) wnd->Camera.details;
			rc = TL_SaveImage(camera, pathname, frame, format);
			break;

		default:
			rc = 2;
			break;
	}

#if 0
ExitSave:
	/* Re-enable video if halted for save */
	if (GetDlgItemCheck(wnd->hdlg, IDB_LIVE)) {
		Camera_SetTriggerMode(wnd, TRIG_FREERUN, 0);
		wnd->LiveVideo = TRUE;
	}
#endif

	return rc;
}


/* ===========================================================================
-- Save all valid images that would have been collected in burst run
--
-- Usage: Camera_SaveAll(WND_INFO *wnd, char *pattern, FILE_FORMAT format);
--
-- Inputs: wnd     - pointer to current descriptor
--         pattern - root of filenames or NULL to query
--         format  - format to save data (defaults to FILE_BMP)
--
-- Output: Saves images as a series of bitmaps
--
-- Return: 0 ==> successful
--         1 ==> rings are not enabled in the code
--         2 ==> buffers not yet allocated or no data
--         3 ==> save abandoned by choice in FileOpen dialog
=========================================================================== */
#ifndef USE_RINGS

int Camera_SaveAll(WND_INFO *wnd, char *pattern, FILE_FORMAT format) {
	static char *rname="Camera_SaveAll";

	Beep(300,200);
	return 1;
}

#else

int Camera_SaveAll(WND_INFO *wnd, char *pattern, FILE_FORMAT format) {
	static char *rname="Camera_SaveAll";

	int rc;
	RING_INFO rings;
	OPENFILENAME ofn;

	DCX_CAMERA *dcx;
	TL_CAMERA *tl;
	BOOL bServerRequest;

	char path[PATH_MAX], *aptr;
	static char local_dir[PATH_MAX] = "";

	/* Make sure we have valid structures */
	if (bServerRequest = (wnd == NULL)) wnd = main_wnd;
	if (wnd == NULL) return 2;

	/* Verify that we have buffers to save */
	if (Camera_GetRingInfo(wnd, &rings) != 0 || rings.nValid <= 0) return 2;

	/* Do we want to query? */
	if (pattern == NULL || *pattern == '\0') {

		/* Get the pattern for the save (directory and name without the extension */
		strcpy_m(path, sizeof(path), "basename");		/* Default name must be initialized with something */
		memset(%ofn, 0, sizeof(ofn));						/* Not static, must be set to zeros */
		ofn.lStructSize       = sizeof(ofn);
		ofn.hwndOwner         = wnd->hdlg;
		ofn.lpstrTitle        = "Burst image database save";
		ofn.lpstrFilter       = "Excel csv file (*.csv)\0*.csv\0\0";
		ofn.lpstrCustomFilter = NULL;
		ofn.nMaxCustFilter    = 0;
		ofn.nFilterIndex      = 1;
		ofn.lpstrFile         = path;						/* Full path */
		ofn.nMaxFile          = sizeof(path);
		ofn.lpstrFileTitle    = NULL;						/* Partial path */
		ofn.nMaxFileTitle     = 0;
		ofn.lpstrDefExt       = "csv";
		ofn.lpstrInitialDir   = (*local_dir=='\0' ? NULL : local_dir);
		ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

		/* Get filename and maybe abort */
		if (! GetSaveFileName(&ofn) != 0) return 3;				/* If aborted, just skip and go back to re-enabling the image */

		/* Scan and remove a .csv extension if it exists */
		if (strlen(path) > 4) {
			aptr = path + strlen(path) - 4;				/* Should be the ".csv" */
			if (_stricmp(aptr, ".csv") == 0) *aptr = '\0';
		}

		/* Save the directory for the next time */
		strcpy_m(local_dir, sizeof(local_dir), path);
		local_dir[ofn.nFileOffset-1] = '\0';						/* Save for next time! */

		/* And now point to local storage as the template */
		pattern = path;
	}

#if 0
	/* Are we triggering freerun?  Stop now and restart at end */
	wasLive = Camera_GetTriggerMode(wnd, NULL) == TRIG_FREERUN;
	if (wasLive) {																	/* Stop now */
		Camera_SetTriggerMode(wnd, TRIG_SOFTWARE, 0);			/* Put in software mode */
		Sleep(1000);																/* Wait 1 second to end */
	}
#endif

	/* Pass a valid format ... default is BMP */
	if (format == FILE_DFLT) format = FILE_BMP;

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

#if 0
	/* If we were live before, restart freerun mode */
	if (wasLive) Camera_SetTriggerMode(wnd, TRIG_FREERUN, 0);
#endif

	return rc;
}

#endif
