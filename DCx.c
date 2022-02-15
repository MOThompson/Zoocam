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
#include "dcx.h"

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
	static BOOL done=FALSE;

	/* Initialize the DCx software to pop up errors (for now so know when doing something wrong */
	if (! initialized) {
		is_SetErrorReport(0, IS_ENABLE_ERR_REP);
		initialized = TRUE;
	}

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

	/* Initialize the DCx software to pop up errors (for now so know when doing something wrong */
	if (initialized) {
		initialized = FALSE;
	}

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

	for (i=0; i<dcx->rings.nSize; i++) {
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
	for (i=0; i<dcx->rings.nSize; i++) {
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
	for (i=0; i<dcx->rings.nSize; i++) {
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
		for (i=0; i<dcx->rings.nSize; i++) {
			if (dcx->Image_Mem[i] == pMem) {
				PID = dcx->Image_PID[i];
				if (index != NULL) *index = i;
				break;
			}
		}
		if (i >= dcx->rings.nSize) { fprintf(stderr, "ERROR: Unable to find a PID corresponding to the image memory (%p)\n", pMem); fflush(stderr); }
//		fprintf(stderr, "Buffer %3.3d: PID=%3.3d  buffer=%p\n", i, PID, pMem); fflush(stderr);
	}

	return PID;
#endif
}


/* ===========================================================================
-- Capture an image and save as a file
--
-- Usage: int DCX_Capture_Image(DCX_CAMERA *dcx, char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap);
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
int DCx_Capture_Image(DCX_CAMERA *dcx, char *fname, DCX_IMAGE_FORMAT format, int quality, DCX_IMAGE_INFO *info, HWND hwndRenderBitmap) {
	static char *rname = "DCx_Capture_Image";

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
