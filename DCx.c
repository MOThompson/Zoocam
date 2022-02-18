/* DCX camera API routines (for ZooCam) */

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
	#include <wingdi.h>					/* Bitmap headers */
#endif

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "win32ex.h"						/* Needed for strcpy_m */
#include "camera.h"
#define INCLUDE_DCX_DETAIL_INFO
#include "dcx.h"							/* DCX camera specific routines */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#ifndef PATH_MAX
	#define	PATH_MAX	(260)
#endif

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
static BOOL initialized = FALSE;						/* Has the driver been initialized properly */
DCX_CAMERA *local_dcx = NULL;

/* ===========================================================================
-- Routine to initialize the DCx driver.  Safe to call multiple times.
-- 
-- Usage: int DCx_Initialize(void);
--
-- Inputs: none
--
-- Output: sets initial driver conditions
--
-- Return: 0 if successful (always)
=========================================================================== */
int DCx_Initialize(void) {
	static char *rname = "DCx_Initialize";

	/* No initialization required, so just mark and return */
	initialized = TRUE;
	return 0;
}

/* ===========================================================================
-- Routine to set debug mode within this driver.  Safe to call multiple times.
-- 
-- Usage: int DCx_SetDebug(BOOL debug);
--
-- Inputs: debug - TRUE to print error messages
--
-- Output: sets internal information
--
-- Return: 0 if successful (always)
=========================================================================== */
int DCx_SetDebug(BOOL debug) {
	static char *rname = "DCx_SetDebug";
	
	/* Initialize the DCx software to pop up errors (for now so know when doing something wrong */
	is_SetErrorReport(0, debug ? IS_ENABLE_ERR_REP : IS_DISABLE_ERR_REP);
	return 0;
}

/* ===========================================================================
-- Routine to close the DCx driver.  Safe to call multiple times.
-- 
-- Usage: int DCx_Shutdown(void);
--
-- Inputs: none
--
-- Output: terminates driver specific elements
--
-- Return: 0 if successful (always)
=========================================================================== */
int DCx_Shutdown(void) {
	static char *rname = "DCx_Shutdown";

	/* If there is a camera hanging around open, close it and then terminate */
	if (initialized && local_dcx != NULL) DCx_CloseCamera(local_dcx);

	local_dcx = NULL;
	initialized = FALSE;

	return 0;
}

/* ===========================================================================
-- Routine to close a DCx camera.  NOP if already closed or not there.
-- 
-- Usage: int DCx_CloseCamera(DCX_CAMERA *dcx);
--
-- Inputs: dcx - pointer to structure for camera
--
-- Output: closes the camera and releases associated resources
--
-- Return: 0 if successful or -1 if dcx invalid or camera not enabled
=========================================================================== */
int DCx_CloseCamera(DCX_CAMERA *dcx) {

	/* Make sure the structure is valid and there is still an opened camera */
	if (dcx == NULL || dcx->hCam <= 0) return -1;

	/* Disable events being called for rendering */
	is_DisableEvent(dcx->hCam, IS_SET_EVENT_FRAME);

#ifdef USE_RINGS
	is_DisableEvent(dcx->hCam, IS_SET_EVENT_SEQ);
	is_ExitEvent(dcx->hCam, IS_SET_EVENT_SEQ);
#endif		

	/* Relaese buffers to recover memory */
	DCx_ReleaseRingBuffers(dcx);

	is_ExitCamera(dcx->hCam);
	dcx->hCam = 0;

	return 0;
}


/* ===========================================================================
-- Query of DCX camera status
--
-- Usage: int DCX_Status(DCX_CAMERA *dcx, DCX_STATUS *status);
--
-- Inputs: dcx    - camera structure, or if NULL use local default version
--         status - if !NULL, filled with details of the current
--                  imaging conditions
--
-- Output: *status - filled with information about current imaging
--
-- Return: 0 - camera is ready for imaging
--         1 - camera is not initialized and/or not active
=========================================================================== */
int DCx_Status(DCX_CAMERA *dcx, DCX_STATUS *status) {

	HCAM hCam;
	CAMINFO camInfo;
	SENSORINFO SensorInfo;
	int nGamma;

	/* In case of errors, return all zeros in the structure if it exists */
	if (status != NULL) memset(status, 0, sizeof(*status));

	/* Must have been started at some point to be able to return images */
	if (dcx == NULL) dcx = local_dcx;
	if (dcx == NULL || dcx->hCam <= 0) return 1;
	hCam = dcx->hCam;

	/* If status is NULL, only interested in if the camera is alive and configured */
	if (status == NULL) return 0;

	/* Now maybe give information */
	status->fps = DCx_GetFPSControl(dcx);
	status->exposure = DCx_GetExposure(dcx, FALSE);

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
-- Enumerate list of available cameras into an array structure for use with the
-- combobox dialog controls and OpenCamera control
--
-- Usage: int DCx_Enum_Camera_List(int *pcount, UC480_CAMERA_INFO **pinfo) {
--
-- Inputs: pcount - pointer to a variable to receive # of available cameras
--         pinfo  - pointer to array of structures with camera information
--
-- Output: *pcount - filled with number of cameras enumerate (some maybe in use)
--         *pinfo  - pointer to an array of entries for each camera
--                   if there are no cameras, *pinfo will be NULL
--
-- Return: # of cameras available if successful, or error code
--           -1 => failed to query the number of cameras
--           -2 => failed to enumerate the information on the cameras
=========================================================================== */
int DCx_Enum_Camera_List(int *pcount, UC480_CAMERA_INFO **pinfo) {

	int i, rc, count;
	UC480_CAMERA_LIST *list = NULL;					/* Enumerated set of cameras and information */

	/* Initial return values */
	if (pcount != NULL) *pcount = 0;
	if (pinfo  != NULL) *pinfo = NULL;

	/* Determine how many cameras are connected and build the combo box with the information */
	if ( (rc = is_GetNumberOfCameras(&count)) != IS_SUCCESS) {
		printf("is_GetNumberOfCameras() failed (rc=%d)\n", rc); fflush(stdout);
		return -1;
	}
	fprintf(stderr, "Number of DCx cameras reported: %d (rc=%d)\n", count, rc); fflush(stderr);

	if (count > 0) {
		if ( (list = calloc(1, sizeof(UC480_CAMERA_LIST) + count*sizeof(UC480_CAMERA_INFO))) == NULL) {
			count = -3;															/* Big error */
		} else {
			list->dwCount = count;
			if ( (rc = is_GetCameraList(list)) != IS_SUCCESS) {
				printf("Error getting camera list (rc=%d)\n", rc); fflush(stdout);
				count = -2;														/* Return value is an error */
			} else if (pinfo != NULL) {									/* Is the information wanted? */
				*pinfo = calloc(count, sizeof(**pinfo));
				for (i=0; i<count; i++) (*pinfo)[i] = list->uci[i];
			}
			free(list);
		}
	} 

	/* Return values */
	if (pcount != NULL) *pcount = count;

	return count;
}

/* ===========================================================================
-- Selects and intializes a specified camera
--
-- Usage: int DCx_Select_Camera(HWND hdlg, DCX_CAMERA *dcx, int CameraID, int *nBestFormat);
--
-- Inputs: dcx      - pointer to initialized DCX_CAMERA structure
--         CameraID - ID of camera to initialize
--         nBestFormat - pointer to receive recommended format to initialize
--
-- Output: *nBestFormat - if !NULL, index to highest resolution format available 
--
-- Return: 0 if successful, other error code
--           1 ==> nominally successful but no recommended format identified
--           3 ==> invalid camera requested
--           4 ==> unable to initialize the camera (busy or otherwise)
--           5 ==> unable to get sensor capabilities, no image formats
--                 found, or failed to enumerate imaging formats
--
-- Notes: Initializes the camera as much as possible, filling in DCX_CAMERA
--        structure, particularly the available formats for filling in selection box
=========================================================================== */
int DCx_Select_Camera(DCX_CAMERA *dcx, int CameraID, int *nBestFormat) {

	int i, nsize, rc, n_formats;
	HCAM hCam;

	CAMINFO camInfo;							/* Local copies - will be copied to dcx */
	SENSORINFO SensorInfo;					/* Local copies - will be copied to dcx */
	IMAGE_FORMAT_LIST *ImageFormatList;	/* Local copy - will be copied to dcx */

	IMAGE_FORMAT_INFO *ImageFormatInfo;
	int ImageFormatID;						/* What image resolution format to use */
	unsigned int width, height;

	/* In case of errors, set the best format option now */
	if (nBestFormat != NULL) *nBestFormat = 0;

	/* Disable any existing camera and free memory */
	if (dcx->hCam > 0) {
		rc = is_StopLiveVideo(dcx->hCam, IS_WAIT);
		rc = is_ExitCamera(dcx->hCam);					/* This also frees the image mem */
	}
	if (dcx->ImageFormatList != NULL) free(dcx->ImageFormatList);
	dcx->CameraID = 0;
	dcx->hCam = 0;
	dcx->ImageFormatID = 0;

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
	dcx->IsSensorColor = SensorInfo.nColorMode != IS_COLORMODE_MONOCHROME ;

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

	width = height = 0;
	ImageFormatID = 0;
	for (i=0; i<n_formats; i++) {
		ImageFormatInfo = ImageFormatList->FormatInfo+i;
		printf(" %2d:  ID: %2d  Width: %4d  Height: %4d  X0,Y0: %d,%d  Capture: 0x%4.4x  Binning: %d  SubSampling: %2d  Scaler: %g  Format: %s\n", i, 
				 ImageFormatInfo->nFormatID, ImageFormatInfo->nWidth, ImageFormatInfo->nHeight, ImageFormatInfo->nX0, ImageFormatInfo->nY0,
				 ImageFormatInfo->nSupportedCaptureModes, ImageFormatInfo->nBinningMode, ImageFormatInfo->nSubsamplingMode, ImageFormatInfo->dSensorScalerFactor,
				 ImageFormatInfo->strFormatName);
		fflush(stdout);
		/* Track the largest format possible */
		if (ImageFormatInfo->nWidth*ImageFormatInfo->nHeight > width*height) {
			width  = ImageFormatInfo->nWidth; 
			height = ImageFormatInfo->nHeight;
			ImageFormatID = ImageFormatInfo->nFormatID;
		}
	}

	/* Copy the active DCX to the local copy for client-server */
	local_dcx = dcx;

	/* Return the recommended format (if we got one).  Return 0 with it, otherwise 4 */
	if (nBestFormat != NULL) *nBestFormat = ImageFormatID;
	return (ImageFormatID > 0) ? 0 : 1 ;
}


/* ===========================================================================
-- Selects one of the DCx camera resolutions and initialize most elements of
-- the image collecion (default frame rate, exposure, etc.)
--
-- Usage: int DCx_Initialize_Resolution(DCX_CAMERA *dcx, int ImageFormatID);
--
-- Inputs: dcx - pointer to initialized DCX_CAMERA structure
--         ImageFormatID - one of the enumerated image formats
--
-- Output: selects the camera resolution and sets DCX elements
--
-- Return: 0 if successful, other error code
--           7 ==> requested ImageFormatID did not show up in the camera's list
--           8 ==> failed to initialize the requested resolution image format
=========================================================================== */
int DCx_Initialize_Resolution(DCX_CAMERA *dcx, int ImageFormatID) {

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

	/* Look up the requested ImageFormatID in the dcx list of known formats */
	for (i=0; i<dcx->NumImageFormats; i++) {
		ImageFormatInfo = dcx->ImageFormatList->FormatInfo+i;
		if (ImageFormatInfo->nFormatID == ImageFormatID) break;
	}
	if (i >= dcx->NumImageFormats) return 7;
	
	/* Set the resolution */
	if (is_ImageFormat(dcx->hCam, IMGFRMT_CMD_SET_FORMAT, &ImageFormatID, sizeof(ImageFormatID)) != IS_SUCCESS) return 8;

	/* Set the aspect ratio and confirm */
	dcx->height = ImageFormatInfo->nHeight;
	dcx->width  = ImageFormatInfo->nWidth;

	/* Save camera information details */
	dcx->ImageFormatID = ImageFormatID;
	dcx->ImageFormatInfo = ImageFormatInfo;
	printf("  Using format: %d  (%d x %d)\n", ImageFormatID, dcx->width, dcx->height); fflush(stdout);

	/* Set the color model */
	rc = is_SetColorMode(dcx->hCam, dcx->IsSensorColor ? IS_CM_BGR8_PACKED : IS_CM_MONO8); 

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

	return 0;
}


/* ===========================================================================
-- Get exposure parameters from camera
--
-- Usage: int DCx_GetExposureParms(DCX_CAMERA *dcx, double *ms_min, double *ms_max, double *ms_inc);
--
-- Inputs: dcx     - pointer to valid DCX_CAMERA structure
--         *ms_min - pointer to get minimum allowed exposure time
--         *ms_max - pointer to get maximum allowed exposure time
--         *ms_inc - pointer to get increment value on exposure time
--
-- Output: for each non-NULL parameter, gets value
--
-- Return: 0 if successful, 1 if dcx or camera invalid
=========================================================================== */
int DCx_GetExposureParms(DCX_CAMERA *dcx, double *ms_min, double *ms_max, double *ms_inc) {
	static char *rname = "DCx_GetExposureParms";
	double low, high, incr;

	/* Default return values */
	if (ms_min != NULL) *ms_min = 0.01;
	if (ms_max != NULL) *ms_max = 1000.0;
	if (ms_inc != NULL) *ms_inc = 0.01;

	if (dcx == NULL || dcx->hCam <= 0) return 1;

	/* -------------------------------------------------------------------------------
	-- Get camera information on range of exposure allowed
	-- Note that is_Exposure(IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE) is limited by current
	-- framerate while is_GetFrameTimeRange() is not.  But deal with bug in the return 
	-- values from is_GetFrameTimeRange()
	--------------------------------------------------------------------------- */
	is_GetFrameTimeRange(dcx->hCam, &low, &high, &incr);

	/* Return values */
	if (ms_min != NULL) *ms_min = low;				/* Go from seconds to ms (but looks to already in ms so ignore) */
	if (ms_max != NULL) *ms_max = high * 1000.0;	/* Go from seconds to ms */
	if (ms_inc != NULL) *ms_inc = incr * 1000.0;	/* Go from seconds to ms */

	return 0;
}

/* ===========================================================================
-- Set the exposure time on the camera
--
-- Usage: double DCx_SetExposure(DCX *dcx, double ms_expose);
--
-- Inputs: dcx       - pointer to valid DCX_CAMERA structure
--         ms_expose - desired exposure time
--
-- Output: sets the camera exposure time
--
-- Return: value of actual exposure time from camera
=========================================================================== */
double DCx_SetExposure(DCX_CAMERA *dcx, double ms_expose) {
	static char *rname = "DCx_SetExposure";

	struct {
		double rmin, rmax, rinc;
	} exp_range;
	double current, fps;

/* -------------------------------------------------------------------------------
	-- Get the exposure allowed range and the current exposure value
	-- Note that is_Exposure(IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE) is limited by current
	-- framerate while is_GetFrameTimeRange() is not.  But deal with bug in the return 
	-- values from is_GetFrameTimeRange()
	--------------------------------------------------------------------------- */
//	is_Exposure(wnd->dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE_RANGE, &exp_range, sizeof(exp_range));
	is_GetFrameTimeRange(dcx->hCam, &exp_range.rmin, &exp_range.rmax, &exp_range.rinc);
//	exp_range.rmin *= 1000;											/* Go from seconds to ms (but looks to already in ms so ignore) */
	exp_range.rmax *= 1000;											/* Go from seconds to ms */
	exp_range.rinc *= 1000;											/* Go from seconds to ms */
	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &current, sizeof(current));
	if (ms_expose < exp_range.rmin) ms_expose = exp_range.rmin;
	if (ms_expose > exp_range.rmax) ms_expose = exp_range.rmax;
	if (ms_expose > current && ms_expose-current < exp_range.rinc) ms_expose = current+1.01*exp_range.rinc;
	if (ms_expose < current && current-ms_expose < exp_range.rinc) ms_expose = current-1.01*exp_range.rinc;

	/* Unfortunately, while framerate will auto decrease exposure, exposure will not auto increase frame rate */
	/* In this routine, always maximize framerate */
	is_SetFrameRate(dcx->hCam, IS_GET_FRAMERATE, &fps);
	if (1000.0/fps < ms_expose+0.1 || fps < DCX_MAX_FPS-0.1) {	/* Change framerate to best value for this  */
		fps = ((int) (10*1000.0/ms_expose)) / 10.0;					/* Closest 0.1 value */
		if (fps > DCX_MAX_FPS) fps = DCX_MAX_FPS;
		is_SetFrameRate(dcx->hCam, fps, &fps);							/* Set and query simultaneously */
	}

	/* Now just set it, and then immediately verify to return exact value */
	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_SET_EXPOSURE, &ms_expose, sizeof(ms_expose));
	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &ms_expose, sizeof(ms_expose));

	return ms_expose;
}

/* ===========================================================================
-- Query the current exposure time of the camera
--
-- Usage: double DCx_GetExposure(DCX *dcx, BOOL bForceQuery);
--
-- Inputs: dcx - pointer to valid DCX_CAMERA structure
--         bForceQuery - if !TRUE, may return last set value (not currently)
--
-- Output: none
--
-- Return: 0 on error; otherwise exposure time in ms
=========================================================================== */
double DCx_GetExposure(DCX_CAMERA *dcx, BOOL bForceQuery) {
	static char *rname = "DCx_GetExposure";

	double ms;

	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &ms, sizeof(ms));

	return ms;
}


/* ===========================================================================
-- Set the gamma value in conversion
--
-- Usage: double DCx_SetGamma(DCX *dcx, double gamma);
--
-- Inputs: dcx - pointer to valid DCX_CAMERA structure
--         gamma - gamma parameter requested on [0.0, 10.0]
--
-- Output: sets camera parameter
--
-- Return: value read back from camera or 0.0 on error
=========================================================================== */
double DCx_SetGamma(DCX_CAMERA *dcx, double gamma) {
	static char *rname = "DCx_SetGamma";

	int ival;

	if (dcx == NULL || dcx->hCam <= 0) return 0.0;

	/* Set value is on [0,1000] for range [0.0,10.0] */
	ival = (int) (100.0*gamma+0.5);
	ival = max(0, min(1000, ival));
	is_Gamma(dcx->hCam, IS_GAMMA_CMD_SET, &ival, sizeof(ival));

	return DCx_GetGamma(dcx);
}

/* ===========================================================================
-- Set the gamma value in conversion
--
-- Usage: double DCx_GetGamma(DCX *dcx);
--
-- Inputs: dcx - pointer to valid DCX_CAMERA structure
--
-- Output: none
--
-- Return: value of gamma on [0.0, 10.0] from camera, or 0.0 on error
=========================================================================== */
double DCx_GetGamma(DCX_CAMERA *dcx) {
	static char *rname = "DCx_GetGamma";

	int gamma = 0;

	if (dcx == NULL || dcx->hCam <= 0) return 0.0;

	/* Returned value is on [0,1000] */
	is_Gamma(dcx->hCam, IS_GAMMA_CMD_GET, &gamma, sizeof(gamma));
	return 0.01*gamma;
}

/* ===========================================================================
-- Set the frame rate for the camera
--
-- Usage: double DCx_SetFPSControl(DCX_CAMERA *dcx, double fps);
--
-- Inputs: dcx - an opened DCx camera
--         fps - requested frame rate
--
-- Output: attempts to set camera framerate
--
-- Return: value actually set or 0 if any type of error
=========================================================================== */
double DCx_SetFPSControl(DCX_CAMERA *dcx, double fps) {
	static char *rname = "DCx_SetFPSControl";

	/* Make sure we are alive and the camera is connected (open) */
	if (dcx == NULL || dcx->hCam <= 0) return 0.0;

	fps = max(DCX_MIN_FPS, max(DCX_MAX_FPS, fps));
	is_SetFrameRate(dcx->hCam, fps, &fps);				/* Set and query simultaneously */

	return fps;
}

/* ===========================================================================
-- Query estimated frame rate based on image acquisition timestamps
--
-- Usage: double DCx_GetFPSActual(DCX_CAMERA *dcx);
--
-- Inputs: camera - an opened DCx camera
--
-- Output: none
--
-- Return: Measured frame rate from the camera
=========================================================================== */
double DCx_GetFPSControl(DCX_CAMERA *dcx) {
	static char *rname = "DCx_GetFPSControl";
	double fps;

	/* Make sure we are alive and the camera is connected (open) */
	if (dcx == NULL || dcx->hCam <= 0) return 0.0;

	/* Query the parameter that is set, not actual */
	is_SetFrameRate(dcx->hCam, IS_GET_FRAMERATE, &fps);
	return fps;
}

/* ===========================================================================
-- Query actual frame rate based on image acquisition timestamps
--
-- Usage: double DCx_GetFPSActual(DCX_CAMERA *dcx);
--
-- Inputs: camera - an opened DCx camera
--
-- Output: none
--
-- Return: Measured frame rate from the camera
=========================================================================== */
double DCx_GetFPSActual(DCX_CAMERA *dcx) {
	static char *rname = "DCx_GetFPSActual";
	double fps;

	/* Make sure we are alive and the camera is connected (open) */
	if (dcx == NULL || dcx->hCam <= 0) return 0.0;

	is_GetFramesPerSecond(dcx->hCam, &fps);
	return fps;
}


/* ===========================================================================
-- Routine to set the gains on the camera (if enabled)
--
-- Usage: int DCx_SetRGBGains(DCX_CAMERA *dcx, int master, int red, int green, int blue);
--
-- Inputs: dcx    - pointer to info about the camera
--         master - value in range [0,100] for hardware gain of the overall image
--         red    - value in range [0,100] for hardware gain of the red channel
--         green  - value in range [0,100] for hardware gain of the green channel
--         blue   - value in range [0,100] for hardware gain of the blue channel
--
-- Output: Sets the hardware gain values to desired value
--
-- Return: 0 if successful
=========================================================================== */
int DCx_SetRGBGains(DCX_CAMERA *dcx, int master, int red, int green, int blue) {

	/* Limit to [0,100] or set value to be ignored */
	master = (master != DCX_IGNORE_GAIN) ? min(100,max(0,master)) : IS_IGNORE_PARAMETER;
	red    = (red    != DCX_IGNORE_GAIN) ? min(100,max(0,red   )) : IS_IGNORE_PARAMETER;
	green  = (green  != DCX_IGNORE_GAIN) ? min(100,max(0,green )) : IS_IGNORE_PARAMETER;
	blue   = (blue   != DCX_IGNORE_GAIN) ? min(100,max(0,blue  )) : IS_IGNORE_PARAMETER;

	is_SetHardwareGain(dcx->hCam, master, red, green, blue);

	return 0;
}

/* ===========================================================================
-- Routine to query the gains on the camera (if enabled)
--
-- Usage: int DCx_GetRGBGains(DCX_CAMERA *dcx, int *master, int *red, int *green, int *blue);
--
-- Inputs: dcx    - pointer to info about the camera
--         master - pointer for return of value (or NULL if unneeded)
--         red    - pointer for return of value (or NULL if unneeded)
--         green  - pointer for return of value (or NULL if unneeded)
--         blue   - pointer for return of value (or NULL if unneeded)
--
-- Output: For each non-null value, return gain as value on [0,100];
--         If the gain does not exist, returns 0
--
-- Return: 0 if successful
=========================================================================== */
int DCx_GetRGBGains(DCX_CAMERA *dcx, int *master, int *red, int *green, int *blue) {
	
	/* Limit to [0,100] or set value to be ignored */
	if (master != NULL)
		*master = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	if (red    != NULL)
		*red    = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_RED_GAIN,    IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	if (green  != NULL)
		*green  = (dcx->SensorInfo.bGGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_GREEN_GAIN,  IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	if (blue   != NULL)
		*blue   = (dcx->SensorInfo.bBGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_BLUE_GAIN,   IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;

	return 0;
}


/* ===========================================================================
-- Routine to query the default gains on the camera (if enabled)
--
-- Usage: int DCx_GetDfltRGBGains(DCX_CAMERA *dcx, int *master, int *red, int *green, int *blue);
--
-- Inputs: dcx    - pointer to info about the camera
--         master - pointer for return of value (or NULL if unneeded)
--         red    - pointer for return of value (or NULL if unneeded)
--         green  - pointer for return of value (or NULL if unneeded)
--         blue   - pointer for return of value (or NULL if unneeded)
--
-- Output: For each non-null value, return the camera's default gain as value on [0,100];
--         If the gain does not exist, returns 0
--
-- Return: 0 if successful
=========================================================================== */
int DCx_GetDfltRGBGains(DCX_CAMERA *dcx, int *master, int *red, int *green, int *blue) {

	/* Limit to [0,100] or set value to be ignored */
	if (master != NULL)
		*master = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_DEFAULT_MASTER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	if (red    != NULL)
		*red    = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_DEFAULT_RED,    IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	if (green  != NULL)
		*green  = (dcx->SensorInfo.bGGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_DEFAULT_GREEN,  IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	if (blue   != NULL)
		*blue   = (dcx->SensorInfo.bBGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_DEFAULT_BLUE,   IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;

	return 0;
}


/* ===========================================================================
-- Set/Query the color correction mode for the camera
--
-- Usage: COLOR_CORRECT DCx_SetColorCorrection(DCX_CAMERA dcx, COLOR_CORRECT mode, double rval);
--        COLOR_CORRECT DCx_GetColorCorrection(DCX_CAMERA dcx, COLOR_CORRECT mode, double *rval);
--
-- Inputs: dcx  - pointer to initialized 
--         mode - one of the allowed color correction modes
--                  IS_CCOR_DISABLE, IS_CCOR_ENABLE_NORMAL, 
--						  IS_CCOR_ENABLE_BG40_ENHANCED, IS_CCOR_ENABLE_HQ_ENHANCED,
--						  IS_CCOR_SET_IR_AUTOMATIC
--         rval - strength of color corrections (camera dependent)
--
-- Output: Sets color correction mode
--
-- Return: Actual color correction mode set for camera or <0 on error
=========================================================================== */
typedef struct _TRANSLATE_COLOR {
	COLOR_CORRECT camera;
	int dcx;
} TRANSLATE_COLOR;

static TRANSLATE_COLOR color_modes[] = {
	{COLOR_DISABLE,	IS_CCOR_DISABLE},
	{COLOR_ENABLE,		IS_CCOR_ENABLE_NORMAL},
	{COLOR_BG40,		IS_CCOR_ENABLE_BG40_ENHANCED},
	{COLOR_HQ,			IS_CCOR_ENABLE_HQ_ENHANCED},
	{COLOR_AUTO_IR,	IS_CCOR_SET_IR_AUTOMATIC}
};

COLOR_CORRECT DCx_SetColorCorrection(DCX_CAMERA *dcx, COLOR_CORRECT mode, double rval) {
	static char *rname = "DCx_SetColorCorrection";

	TRANSLATE_COLOR *entry;
	int i, rc;

	/* Make sure we are alive and the camera is connected (open) */
	if (dcx == NULL || dcx->hCam <= 0) return -1;

	/* Find this entry in the list to translate ti DCX call value */
	entry = NULL;
	for (i=0; i<sizeof(color_modes)/sizeof(color_modes[0]); i++) {
		if (color_modes[i].camera == mode) { entry = color_modes+i; break; }
	}
	if (entry == NULL) return -2;

	/* Set the value and then read back parameters */
	rc = is_SetColorCorrection(dcx->hCam, entry->dcx, &rval);

	/* Query the value and return */
	return DCx_GetColorCorrection(dcx, NULL);
}

COLOR_CORRECT DCx_GetColorCorrection(DCX_CAMERA *dcx, double *rval) {
	static char *rname = "DCx_GetColorCorrection";

	TRANSLATE_COLOR *entry;
	int i, rc;
	double tmp;

	/* Make sure we are alive and the camera is connected (open) */
	if (dcx == NULL || dcx->hCam <= 0) return -1;
	if (rval == NULL) rval = &tmp;

	/* Query the value and return */
	rc = is_SetColorCorrection(dcx->hCam, IS_GET_CCOR_MODE, rval);
	entry = color_modes;							/* Default return of first entry */
	for (i=0; i<sizeof(color_modes)/sizeof(color_modes[0]); i++) {
		if (color_modes[i].dcx == rc) { entry = color_modes+i; break; }
	}

	return entry->camera;
}


/* ===========================================================================
-- Routine to read/save camera parameters to a file
--
-- Usage: DCx_LoadParameters(DCX_CAMERA *dcx, char *path);
--
-- Inputs: dcx    - pointer to info about the camera
--         path   - pathname to load, or NULL for dialog box
--
-- Output: Loads and implements parameters saved in the file
--         Saves current camera parameters to the file
--
-- Return: 0 if successful
--           1 ==> invalid camera structure or no camera initialized
--           2 ==> file does not match the current camera (on load)
--           3 ==> general failure
=========================================================================== */
int DCx_LoadParameterFile(DCX_CAMERA *dcx, char *path) {
	static char *rname = "DCx_LoadParameterFile";
	int rc;

	if (dcx == NULL || dcx->hCam <= 0) return 1;
	
	switch (is_ParameterSet(dcx->hCam, IS_PARAMETERSET_CMD_LOAD_FILE, path, 0)) {
		case IS_SUCCESS:
			rc = 0; break;
		case IS_INVALID_CAMERA_TYPE:
			rc = 2; break;
		case IS_NO_SUCCESS:
		default:
			rc = 3; break;
	}
	return rc;
}

int DCx_SaveParameterFile(DCX_CAMERA *dcx, char *path) {
	static char *rname = "DCx_SaveParameterFile";
	int rc;

	if (dcx == NULL || dcx->hCam <= 0) return 1;

	switch (is_ParameterSet(dcx->hCam, IS_PARAMETERSET_CMD_SAVE_FILE, path, 0)) {
		case IS_SUCCESS:
			rc = 0; break;
		case IS_INVALID_CAMERA_TYPE:
			rc = 2; break;
		case IS_NO_SUCCESS:
		default:
			rc = 3; break;
	}
	return rc;
}

/* ===========================================================================
-- Software trigger of camera (single frame)
--
-- Usage: int DCx_Trigger(DCX_CAMERA *dcx, int msWait);
--
-- Inputs: dcx    - structure associated with a camera
--         msWait - time to wait before forcing change
--                  <0 ==> wait indefinitely (IS_WAIT)
--                   0 ==> stop immediately (IS_DONT_WAIT)
--                  >0 ==> timeout; 10 ms resolution from 40 ms to 1193 hrs
--
-- Output: Triggers camera immediately (last time if FREERUN)
--
-- Return: 0 if successful
--           1 ==> dcx or dcx->hCam invalid
--           2 ==> not in SOFTWARE or EXTERNAL triggering modes
--
-- Notes: msWait < 0 will wait until there is an image captured
--               = 0 returns immediately but still triggers the capture
--        If trigger mode was FREERUN, will be set to SOFTWARE after call
=========================================================================== */
static int ms_to_wait(int msWait) {						/* Returns wait parameter for is_xxx calls */
	if      (msWait  < 0) msWait = IS_WAIT;
	else if (msWait == 0) msWait = IS_DONT_WAIT;
	else if (msWait < 40) msWait = 4;
	else                  msWait = (msWait+9)/10;	/* 10 ms increments */
	return msWait;
}

int DCx_Trigger(DCX_CAMERA *dcx, int msWait) {
	static char *rname = "DCx_Trigger";

	/* Validate parameters and that we are in a soft-trigger mode */
	if (dcx == NULL || dcx->hCam <= 0) return 1;

	is_FreezeVideo(dcx->hCam, ms_to_wait(msWait));		/* Switches out of FREERUN */
	if (TRIG_FREERUN == dcx->Trig_Mode) dcx->Trig_Mode = TRIG_SOFTWARE;

	return 0;
}

/* ===========================================================================
-- Set/Query the triggering mode for the camera
--
-- Usage: TRIG_MODE DCx_SetTrigMode(DCX_CAMERA *dcx, TRIG_MODE mode, int msWait);
--        TRIG_MODE DCx_GetTrigMode(DCX_CAMERA *dcx);
--
-- Inputs: dcx    - structure associated with a camera
--         mode   - one of the allowed triggering modes
--                  TRIG_SOFTWARE, TRIG_FREERUN, TRIG_EXTERNAL
--         msWait - time to wait before forcing change
--                  <0 ==> wait indefinitely (IS_WAIT)
--                   0 ==> stop immediately (IS_DONT_WAIT)
--                  >0 ==> timeout; 10 ms resolution from 40 ms to 1193 hrs
--
-- Output: Sets camera triggering mode
--
-- Return: Mode in camera or -1 on error
=========================================================================== */
TRIG_MODE DCx_GetTrigMode(DCX_CAMERA *dcx) {
	static char *rname = "DCx_GetTrigMode";
	return dcx->Trig_Mode;
}

TRIG_MODE DCx_SetTrigMode(DCX_CAMERA *dcx, TRIG_MODE mode, int msWait) {
	static char *rname = "DCx_SetTrigMode";

	/* Validate call parameters */
	if (dcx == NULL || dcx->hCam <= 0) return -1;
	if (mode != TRIG_SOFTWARE && mode != TRIG_FREERUN && mode != TRIG_EXTERNAL) mode = TRIG_SOFTWARE;

	if (dcx->Trig_Mode != mode) {						/* Do we need to change? */

		/* Move to TRIG_SOFTWARE (especially pause video if in FREERUN) */
		if (dcx->Trig_Mode == TRIG_FREERUN) {
			int rc;
			if ( (rc = is_FreezeVideo(dcx->hCam, ms_to_wait(msWait))) != 0) {
				switch (rc) {
					case IS_TIMED_OUT:
						fprintf(stderr, "%s: is_FreezeVideo() timed out [rc=%d]\n", rname, rc); fflush(stderr);
						break;
					default:
						fprintf(stderr, "%s: is_FreezeVideo() unknown error [rc=%d]\n", rname, rc); fflush(stderr);
						break;
				}
				is_FreezeVideo(dcx->hCam, IS_DONT_WAIT);		/* Try forced stop */
			}
		}

		/* Now, do what's necessary to get to desired mode */
		dcx->Trig_Mode = mode;
		if (dcx->Trig_Mode == TRIG_FREERUN) {
			is_CaptureVideo(dcx->hCam, IS_DONT_WAIT);
			dcx->iLast = dcx->iShow = dcx->nValid = 0;			/* All reset these values */
		}
	}

	/* Return current mode */
	return dcx->Trig_Mode;
}


/* ===========================================================================
-- Allocate (or deallocate) ring buffer for images
--
-- Usage: int DCx_SetBufferSize(DCX_CAMERA *dcx, int nBuf);
--
-- Inputs: dcx  - structure associated with a camera
--         nBuf - number of buffers to allocate in the ring (DCX_MAX_RING_SIZE)
--                constrained within limits [1, DCX_MAX_RING_SIZE]
--
-- Output: Stops processing for a moment, changes buffers, and restarts
--
-- Return: Number of buffers or 0 on fatal errors; minimum number is 1
--
-- Note: A request can be ignored and will return previous size
=========================================================================== */
int DCx_SetRingBufferSize(DCX_CAMERA *dcx, int nRequest) {
	static char *rname = "DCx_SetRingBufferSize";

	int i, rc, rval;
	TRIG_MODE trig_hold;

	/* Must be valid structure */
	if (dcx == NULL || dcx->hCam <= 0) return 0;

	/* Save video trigger mode and stop captures */
	trig_hold = dcx->Trig_Mode;
	DCx_SetTrigMode(dcx, TRIG_SOFTWARE, 0);							/* Stop the video */

#ifndef USE_RINGS

	rc = is_AllocImageMem(dcx->hCam, wnd->width, wnd->height, dcx->IsSensorColor ? 24 : 8, &dcx->Image_Mem, &dcx->Image_PID);
	rc = is_SetImageMem(dcx->hCam, dcx->Image_Mem, dcx->Image_PID); 
	fprintf(stderr, "  Allocated Image memory: %p  PID: %d\n", dcx->Image_Mem, dcx->Image_PID); fflush(stderr);
	dcx->Image_Mem_Allocated = TRUE;
	rval = 1;																		/* Report 1 ring allocated */

#else

	/* Determine the new size (or size) of the ring buffer */
	if (nRequest == 0) nRequest = DCX_DFLT_RING_SIZE;
	if (nRequest <= 1) nRequest = dcx->nSize;						/* If 0 or negative, reuse current size (query) */
	nRequest = min(nRequest, DCX_MAX_RING_SIZE);							/* Limit to reasonable */
	if (nRequest == dcx->nSize) return nRequest;					/* If no change, do nothing */

	/* Release existing buffers if present */
	DCx_ReleaseRingBuffers(dcx);												/* Okay since video is definitely stopped */

	/* Store the new ring size and allocate memory buffers */
	dcx->nSize = nRequest;
	dcx->Image_Mem   = calloc(dcx->nSize, sizeof(dcx->Image_Mem[0]));
	dcx->Image_PID   = calloc(dcx->nSize, sizeof(dcx->Image_PID[0]));
	fprintf(stderr, "Allocating memory for [%d] ring frames ... ", dcx->nSize); fflush(stderr);
	for (i=0; i<dcx->nSize; i++) {
		if (i%20 == 19) fprintf(stderr, "\n   ");
		fprintf(stderr, "[%d", i+1); fflush(stderr);
		rc = is_AllocImageMem(dcx->hCam, dcx->width, dcx->height, dcx->IsSensorColor ? 24 : 8, &dcx->Image_Mem[i], &dcx->Image_PID[i]);
		fprintf(stderr, "m"); fflush(stderr);
		if (rc != IS_SUCCESS) {
			fprintf(stderr, "  Image memory allocation failed (rc=%d)\n", rc); fflush(stderr);
			continue;
		}
		rc = is_AddToSequence(dcx->hCam, dcx->Image_Mem[i], dcx->Image_PID[i]);
		fprintf(stderr, "s]"); fflush(stderr);
		if (rc != IS_SUCCESS) {
			fprintf(stderr, "  Adding image to the list failed (rc=%d)\n", rc); fflush(stderr);
			continue;
		}
	}
	dcx->iLast = dcx->iShow = dcx->nValid = 0;
	dcx->Image_Mem_Allocated = TRUE;							/* Memory now allocated */
	rval = dcx->nSize;									/* Report number of rings allocated */

	fprintf(stderr, " ... done\n"); fflush(stderr);
#endif

	/* Restore trigger mode and start video if in freerun mode */
	DCx_SetTrigMode(dcx, trig_hold, 0);

	/* Return number of image buffers that exist */
	return rval;
}


/* ===========================================================================
-- Release ring buffers / memory associated with a camera.
--
-- Usage: int DCx_ReleaseRingBuffers(DCX_CAMERA *dcx);
--
-- Inputs: dcx  - structure associated with a camera
--
-- Output: Clears ring information in camera and releases image memory
--
-- Return: 0 on success
--
-- WARNING: Camera must be stopped before calling
=========================================================================== */
int DCx_ReleaseRingBuffers(DCX_CAMERA *dcx) {
	static char *rname = "DCx_ReleaseRingBuffers";
	

#ifndef USE_RINGS

	if (dcx->Image_Mem_Allocated) {
		DCx_SetTrigMode(dcx, TRIG_SOFTWARE, 0);				/* Make sure we can't trigger unexpectedly */
		is_FreeImageMem(dcx->hCam, wnd->Image_Mem, wnd->Image_PID);
	}
	dcx->Image_Mem_Allocated = FALSE;

#else
	int i, rc;

	/* If already released, nothing to do */
	if (dcx->Image_Mem_Allocated) {
		DCx_SetTrigMode(dcx, TRIG_SOFTWARE, 0);				/* Make sure we can't trigger unexpectedly */

		if ( (rc = is_ClearSequence(dcx->hCam)) != IS_SUCCESS) {	fprintf(stderr, "is_ClearSequence failed [rc=%d]\n", rc); fflush(stderr); }
		for (i=0; i<dcx->nSize; i++) {
			if ( (rc = is_FreeImageMem(dcx->hCam, dcx->Image_Mem[i], dcx->Image_PID[i])) != IS_SUCCESS) { fprintf(stderr, "is_FreeImageMem failed [i=%d rc=%d]\n", i, rc); fflush(stderr); }
		}
		free(dcx->Image_Mem); dcx->Image_Mem = NULL;
		free(dcx->Image_PID); dcx->Image_PID = NULL;
		dcx->nSize = 0;
	}
	dcx->Image_Mem_Allocated = FALSE;
#endif

	return 0;
}

/* ===========================================================================
-- Render an image in a specified window
-- 
-- Usage: int DCx_RenderImage(DCX_CAMERA *dcx, int frame, HWND hwnd);
--
-- Inputs: dcx    - an opened DCX camera
--         frame  - frame to process from buffers (0 = most recent)
--         hwnd   - window to render the bitmap to
--
-- Output: converts image to RGB, generates bitmap, and displays in window
--
-- Return: 0 if successful, otherwise an error code
--           1 ==> dcx invalid, no camera, or not a window
--           2 ==> no valid image frames
=========================================================================== */
int DCx_RenderImage(DCX_CAMERA *dcx, int frame, HWND hwnd) {
	static char *rname = "DCx_RenderImage";

	int rc;

	/* Verify parameters */
	if (dcx == NULL || ! dcx->hCam <= 0 || ! IsWindow(hwnd)) return 1;

	rc = 0;							/* Assume success */
#ifndef USE_RINGS
	is_RenderBitmap(dcx->hCam, dcx->Image_PID, hwnd, IS_RENDER_FIT_TO_WINDOW);
#else
	if (dcx->nValid <= 0) {						/* No valid images to display */
		rc = 2;
	} else {
		frame = max(0, min(frame, dcx->nValid-1));
		is_RenderBitmap(dcx->hCam, dcx->Image_PID[frame], hwnd, IS_RENDER_FIT_TO_WINDOW);
	}
#endif

	return rc;
}


/* ===========================================================================
-- Routine to return the PID, index, or pMem corresponding to an active pMem
-- or PID retrieved via functions such as is_GetImageMem() routine.  For single
--  image mode, just return the PID is from the initial memory allocation.
-- For ring buffering, search through the list and return the matching one.
--
-- The PID and index appear to differ by 1 ... index is 0 based while PID is 
-- 1 based.  But safer just to accept that they are different beasts.
--
-- Usage: int    FindImagePIDFrompMem(WND_INFO *wnd, void *pMem, int *index);
--        uchar *FindImagepMemFromPID(WND_INFO *wnd, int PID, int *index);
--        int    FindImageIndexFromPID(WND_INFO *wnd, int PID);
--
-- Inputs: dcx - pointer to valid structure for the window
--         pMem - memory buffer returned from is_GetImageMem
--         index - if not NULL, variable to receive the index into the
--                 ring buffer table
--
-- Output: *index - actual index (0 based) into wnd->Image_PID, _Mem, and _Time
--
-- Returns: PID or index corresponding to the pMem/PID if it exists. Otherwise -1.
--          pMem corresponding to PID, or NULL if invalid.  Sets index if wanted.
=========================================================================== */
int FindImageIndexFromPID(DCX_CAMERA *dcx, int PID) {

#ifndef USE_RINGS
	return 0;
#else
	int i;

	for (i=0; i<dcx->nSize; i++) {
		if (dcx->Image_PID[i] == PID) return i;
	}
	return -1;
#endif
}	

int FindImageIndexFrompMem(DCX_CAMERA *dcx, char *pMem) {

#ifndef USE_RINGS
	return 0;
#else
	int i;
	for (i=0; i<dcx->nSize; i++) {
		if (dcx->Image_Mem[i] == pMem) return i;
	}
	return -1;
#endif
}	

unsigned char *FindImagepMemFromPID(DCX_CAMERA *dcx, int PID, int *index) {

#ifndef USE_RINGS
	if (index != NULL) *index = 0;
	return dcx->Image_pMem;
#else
	int i;
	if (index != NULL) *index = -1;
	for (i=0; i<dcx->nSize; i++) {
		if (dcx->Image_PID[i] == PID) {
			if (index != NULL) *index = i;
			return dcx->Image_Mem[i];
		}
	}
	return NULL;
#endif
}	

int FindImagePIDFrompMem(DCX_CAMERA *dcx, unsigned char *pMem, int *index) {

#ifndef USE_RINGS
	return dcx->Image_PID;
#else
	int i, PID;

	/* Set the default return on errors */
	PID = -1;
	if (index != NULL) *index = -1;

	/* Scan through the list */
	if (dcx != NULL && dcx->Image_Mem_Allocated) {
		for (i=0; i<dcx->nSize; i++) {
			if (dcx->Image_Mem[i] == pMem) {
				PID = dcx->Image_PID[i];
				if (index != NULL) *index = i;
				break;
			}
		}
		if (i >= dcx->nSize) { fprintf(stderr, "ERROR: Unable to find a PID corresponding to the image memory (%p)\n", pMem); fflush(stderr); }
//		fprintf(stderr, "Buffer %3.3d: PID=%3.3d  buffer=%p\n", i, PID, pMem); fflush(stderr);
	}

	return PID;
#endif
}


/* ===========================================================================
   ===========================================================================
   ========================  SERVERS SIDE FUNCTIONS  =========================
   ===========================================================================
=========================================================================== */

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

	DCX_CAMERA *dcx;
	HCAM hCam;
	int trig_hold;

	int rc, col, line, height, width, pitch, nGamma;
	unsigned char *pMem, *aptr;

	/* If info provided, clear it in case we have a failure */
	if (info != NULL) memset(info, 0, sizeof(DCX_IMAGE_INFO));
	if (buffer == NULL) return -1;

	/* Must have been started at some point to be able to do anything */
	if ( (dcx = local_dcx) == NULL) return 1;					/* No camera active */
	if (dcx->hCam <= 0) return 1;									/* No camera active */
	hCam = dcx->hCam;

	/* Capture and hold an image */
	trig_hold = dcx->Trig_Mode;
	if ( (rc = is_FreezeVideo(hCam, IS_WAIT)) != 0) {		/* Forces image capture and resets to software trigger mode */
		printf("[%s:] is_FreezeVideo returned failure (rc=%d)", rname, rc);
		rc = is_FreezeVideo(hCam, IS_WAIT);
		printf("  Retry gives: %d\n", rc);
		fflush(stdout);
	}
	dcx->Trig_Mode = TRIG_SOFTWARE;

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

		DCx_GetRGBGains(dcx, &info->master_gain, &info->red_gain, &info->green_gain, &info->blue_gain);

		/* Calculate the number of saturated pixels on each color plane */
		info->red_saturate = info->green_saturate = info->blue_saturate = 0;
		for (line=0; line<height; line++) {
			aptr = pMem + line*pitch;					/* Pointer to this line */
			for (col=0; col<width; col++) {
				if (dcx->IsSensorColor) {
					if (aptr[3*col+0] >= 255) info->blue_saturate++;
					if (aptr[3*col+1] >= 255) info->green_saturate++;
					if (aptr[3*col+2] >= 255) info->red_saturate++;
				} else {
					if (aptr[col] >= 255) info->blue_saturate = info->green_saturate = ++info->red_saturate;
				}
			}
		}
	}

	trig_hold = dcx->Trig_Mode;
	DCx_SetTrigMode(dcx, trig_hold, 0);

	return 0;
}

/* ===========================================================================
-- DCx_Set_Exposure_Parms
--
-- Usage: int DCx_Set_Exposure_Parms(int options, DCX_EXPOSURE_PARMS *request, DCX_EXPOSURE_PARMS *actual);
--
-- Inputs: options - OR'd bitwise flag indicating parameters that will be modified
--         request - pointer to structure with values for the selected conditions
--         actual  - pointer to variable to receive the actual settings (all updated)
--
-- Output: *actual - if not NULL, values of all parameters after modification
--
-- Return: 0 ==> successful
--         1 ==> no camera initialized
--
-- Notes:
--    1) Parameters are validated but out-of-bound will not generate failure
--    2) exposure is prioritized if both DCX_MODIFY_EXPOSURE and DCX_MODIFY_FPS
--       are specified.  FPS will be modified only if lower than max possible
--    3) If DCX_MODIFY_EXPOSURE is given without DCX_MODIFY_FPS,
--       maximum FPS will be set
--    4) Trying DCXF_MODIFY_BLUE_GAIN on a monochrome camera is a NOP
=========================================================================== */
int DCx_Set_Exposure_Parms(int options, DCX_EXPOSURE_PARMS *request, DCX_EXPOSURE_PARMS *actual) {
	static char *rname = "DCx_Set_Exposure_Parms";

	DCX_EXPOSURE_PARMS mine;
	DCX_CAMERA *dcx;
	
	int gamma;
	int master,red,green,blue;
	double fps;

	/* Set response code to -1 to indicate major error */
	if (actual == NULL) actual = &mine;			/* So don't have to check */
	memset(actual, 0, sizeof(*actual));

	/* Must have been started at some point to be able to do anything */
	if ( (dcx = local_dcx) == NULL) return 1;					/* No camera active */
	if (dcx->hCam <= 0) return 1;									/* No camera active */
	
#if 0																		/* No ability to reset the display ... belongs at camera equivalent routine */
	hdlg = wnd->main_hdlg;
	if (hdlg != NULL && ! IsWindow(hdlg)) hdlg = NULL;		/* Mark hdlg if not window */
#endif

	/* If we don't have data, we can't have any options for setting */
	if (request == NULL) options = 0;

	if (options & DCXF_MODIFY_GAMMA) {
		gamma = min(100,max(0,request->gamma));
		is_Gamma(dcx->hCam, IS_GAMMA_CMD_SET, &gamma, sizeof(gamma));
#if 0
		if (hdlg != NULL) SendMessage(hdlg, WMP_SHOW_GAMMA, 0, 0);
#endif
	}

	if (options & (DCXF_MODIFY_MASTER_GAIN | DCXF_MODIFY_RED_GAIN | DCXF_MODIFY_GREEN_GAIN | DCXF_MODIFY_BLUE_GAIN)) {
		master = (options & DCXF_MODIFY_MASTER_GAIN) ? min(100,max(0,request->master_gain)) : IS_IGNORE_PARAMETER;
		red    = (options & DCXF_MODIFY_RED_GAIN)    ? min(100,max(0,request->red_gain))    : IS_IGNORE_PARAMETER;
		green  = (options & DCXF_MODIFY_GREEN_GAIN)  ? min(100,max(0,request->green_gain))  : IS_IGNORE_PARAMETER;
		blue   = (options & DCXF_MODIFY_BLUE_GAIN)   ? min(100,max(0,request->blue_gain))   : IS_IGNORE_PARAMETER;
		is_SetHardwareGain(dcx->hCam, master, red, green, blue);
#if 0
		if (hdlg != NULL) SendMessage(hdlg, WMP_SHOW_GAINS, 0, 0);
#endif
	}

	/* Do the exposure first maximizing, and then maybe frame rate */
	if (options & DCXF_MODIFY_EXPOSURE) {
		DCx_SetExposure(dcx, request->exposure);
		if (options & DCXF_MODIFY_FPS) {
			is_SetFrameRate(dcx->hCam, IS_GET_FRAMERATE, &fps);		/* Determine maximized framerate */
			if (request->fps < fps) is_SetFrameRate(dcx->hCam, request->fps, &fps);
		}
	} else if (options & DCXF_MODIFY_FPS) {
		is_SetFrameRate(dcx->hCam, request->fps, &fps);
	}

	/* Retrieve the current values now */
	is_Exposure(dcx->hCam, IS_EXPOSURE_CMD_GET_EXPOSURE, &actual->exposure, sizeof(actual->exposure));
	is_SetFrameRate(dcx->hCam, IS_GET_FRAMERATE, &actual->fps);
	is_Gamma(dcx->hCam, IS_GAMMA_CMD_GET, &actual->gamma, sizeof(actual->gamma));
	actual->master_gain = (dcx->SensorInfo.bMasterGain) ? is_SetHardwareGain(dcx->hCam, IS_GET_MASTER_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	actual->red_gain    = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_RED_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	actual->green_gain  = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_GREEN_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;
	actual->blue_gain   = (dcx->SensorInfo.bRGain)      ? is_SetHardwareGain(dcx->hCam, IS_GET_BLUE_GAIN, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER, IS_IGNORE_PARAMETER) : 0 ;

	return 0;
}


/* ===========================================================================
-- Query the current ring buffer values
--
-- Usage: int DCx_GetRingInfo(DCX_CAMERA *dcx, int *nSize, int *nValid, int *iLast, int *iShow);
--
-- Inputs: dcx    - structure associated with a camera
--         nSize  - pointer for # of buffers in the ring
--			  nValid - pointer for # of buffers in the ring current with valid images
--			  iLast  - pointer for index of buffer with last image from the camera
--			  iShow  - pointer for index of buffer currently being shown (rgb values)
--
-- Output: For all parameter !NULL, copies appropriate value from internals
--
-- Return: 0 if successful, 1 if dcx invalid
=========================================================================== */
int DCx_GetRingInfo(DCX_CAMERA *dcx, int *nSize, int *nValid, int *iLast, int *iShow) {
	static char *rname = "DCx_GetRingInfo";
	BOOL valid;

	/* Is structure valid ... return default values or values from the structure */
	valid = dcx != NULL;

	if (nSize  != NULL) *nSize  = valid ? dcx->nSize   : 1 ;
	if (nValid != NULL) *nValid = valid ? dcx->nValid  : 0 ;
	if (iLast  != NULL) *iLast  = valid ? dcx->iLast   : 0 ;
	if (iShow  != NULL) *iShow  = valid ? dcx->iShow   : 0 ;

	return valid ? 0 : 1 ;
}


/* ===========================================================================
-- Save image as file
--
-- Usage: int DCX_SaveImage(DCX_CAMERA *dcx);
--
-- Inputs: dcx     - pointer to structure with information about the DCx camera
--
-- Output: Saves the file as specified.
--         if info != NULL, *info filled with details of capture and basic image stats
--
-- Return: 0 - all okay
--         1 - camera is not initialized and active
--         2 - file save failed for some other reason
=========================================================================== */
int DCx_SaveImage(DCX_CAMERA *dcx, char *fname, int format) {
	static char *rname = "DCx_SaveImage";

	wchar_t fname_w[MAX_PATH];
	IMAGE_FILE_PARAMS ImageParams;
	size_t ncount;
	int rc;

	/* Convert the filename or leave NULL for dialog box */
	if (fname != NULL) {
		mbstowcs_s(&ncount, fname_w, MAX_PATH, fname, strlen(fname));
		ImageParams.pwchFileName = fname_w;
	} else {
		ImageParams.pwchFileName = NULL;
	}

	/* Choose a format */
	switch (format) {
		case IMAGE_JPG:
			ImageParams.nFileType = IS_IMG_JPG; break;
		case IMAGE_PNG:
			ImageParams.nFileType = IS_IMG_PNG; break;
		case IMAGE_BMP:
		default:
			ImageParams.nFileType = IS_IMG_BMP; break;
	}

	/* Specify the compression for PNG and JPG - BMP is uncompressed */
	ImageParams.nQuality = 75;

	/* Default values for remaining parameters */
	ImageParams.pnImageID    = NULL;	
	ImageParams.ppcImageMem  = NULL;

	/* Let the driver do all the work */
	rc = is_ImageFile(dcx->hCam, IS_IMAGE_FILE_CMD_SAVE, &ImageParams, sizeof(ImageParams));

	return rc;
}


/* ===========================================================================
-- Capture an image and save as a file
--
-- Usage: int DCX_CaptureImage(DCX_CAMERA *dcx, char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap);
--
-- Inputs: dcx     - pointer to structure with information about the DCx camera
--         fname   - if not NULL, pointer to name of file to be saved
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
int DCx_CaptureImage(DCX_CAMERA *dcx, char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap) {
	static char *rname = "DCx_CaptureImage";

	wchar_t fname_w[MAX_PATH];
	IMAGE_FILE_PARAMS ImageParams;
	HCAM hCam;

	int rc, col, line, height, width, pitch, nGamma, PID;
	unsigned char *pMem, *aptr;
	size_t ncount;

	/* If info provided, clear it in case we have a failure */
	if (info != NULL) memset(info, 0, sizeof(DCX_IMAGE_INFO));

	/* Must have been started at some point to be able to return images */
	if (dcx == NULL || dcx->hCam <= 0) return 1;
	hCam = dcx->hCam;

	/* Capture and hold an image */
	if ( (rc = is_FreezeVideo(hCam, IS_WAIT)) != 0) {
		printf("[%s:] is_FreezeVideo returned failure (rc=%d)", rname, rc);
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
				if (dcx->IsSensorColor) {
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

	/* If the window exists, render this bitmap to that location */
	if (IsWindow(hwndRenderBitmap) && (PID = FindImagePIDFrompMem(dcx, pMem, NULL)) >= 0) {
		is_RenderBitmap(dcx->hCam, PID, hwndRenderBitmap, IS_RENDER_FIT_TO_WINDOW);
	}

	return rc;
}
