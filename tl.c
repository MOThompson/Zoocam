/* tl camera function utility */

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
#include <stdint.h>		            /* C99 extension to get known width integers */

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
#include "timer.h"
#include "win32ex.h"
#include "camera.h"
#define INCLUDE_TL_DETAIL_INFO
#include "tl.h"

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
static void camera_connect_callback(char* camera_ID, enum TL_CAMERA_USB_PORT_TYPE usb_bus_speed, void* context);
static void camera_disconnect_callback(char* camera_ID, void* context);
static void frame_available_callback(void* sender, unsigned short* image_buffer, int frame_count, unsigned char* metadata, int metadata_size_in_bytes, void* context);

static int TL_CameraErrMsg(int rc, char *msg, char *routine);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global variables    */
/* ------------------------------- */
BOOL TL_is_initialized = FALSE;
int tl_camera_count = 0;
TL_CAMERA *tl_camera_list[TL_MAX_CAMERAS] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
															NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static BOOL is_camera_sdk_open        = FALSE,		/* Have we opened the SDK */
				is_camera_dll_open        = FALSE,		/* Have we linked to the DLLs */
				is_mono_to_color_sdk_open = FALSE;		/* Has the color processing sdk been opened */
			   

/* ===========================================================================
-- Routine to initialize the TL interface (load SDK, initialize)
-- Should be followed up with a TL_Release() call at end of program
--
-- Usage: int TL_Initialize();
--
-- Inputs: none
--
-- Output: Internal structures initialized
--
-- Return: 0 if successful, otherwise bit-wise indicator of errors encountered
--          0x01 - failed to initialize camera sdk dll's
--          0x02 - failed to open camera sdk
--				0x04 - failed to open color processing sdk
=========================================================================== */
int TL_Initialize(void) {
	static char *rname = "TL_Initilize";

	int rc, rcode;											/* return code */

	/* Allow multiple calls as NOP */
	if (TL_is_initialized) return 0;

	/* Clear the camera list and count */
	memset(tl_camera_list, 0, TL_MAX_CAMERAS*sizeof(*tl_camera_list));
	tl_camera_count = 0;

	/* Assume success */
	rcode = 0;

	/* Build the DLL link points for the sdk (painful) */
	is_camera_dll_open = tl_camera_sdk_dll_initialize() == 0;
	if (! is_camera_dll_open) { 
		fprintf(stderr, "[%s] Failed to initialize the TL camera DLLs\n", rname); fflush(stderr);
		rcode |= 0x01;
	}

	/* Open the SDK */
	is_camera_sdk_open = tl_camera_open_sdk() == 0;
	if (! is_camera_sdk_open) { 
		fprintf(stderr, "[%s] Failed to open the tl camera SDK\n", rname); fflush(stderr);
		rcode |= 0x02;
	}

	/* Open the SDK for the mono to color processing (painful) */
	is_mono_to_color_sdk_open = tl_mono_to_color_processing_initialize() == 0;
	if (! is_mono_to_color_sdk_open) { 
		fprintf(stderr, "[%s] Failed to initialize mono to color processing sdk\n", rname); fflush(stderr);
		rcode |= 0x04;
	}

	/* Register the camera connect/disconnect event callbacks. */
	if ( (rc = tl_camera_set_camera_connect_callback(camera_connect_callback, 0)) != 0)       TL_CameraErrMsg(rc, "Unable to register camera connect callback", rname);
	if ( (rc = tl_camera_set_camera_disconnect_callback(camera_disconnect_callback, 0)) != 0) TL_CameraErrMsg(rc, "Unable to register camera disconnect callback", rname);

	/* Mark that we have initialized and are open for business */
	TL_is_initialized = TRUE;
	return rcode;
}

/* ===========================================================================
-- Routine to set debug mode within this driver.  Safe to call multiple times.
-- 
-- Usage: int TL_SetDebug(BOOL debug);
--
-- Inputs: debug - TRUE to print error messages
--
-- Output: sets internal information
--
-- Return: 0 if successful (always)
=========================================================================== */
int TL_SetDebug(BOOL debug) {
	static char *rname = "TL_SetDebug";

	/* Not sure how to implement ... really have to track the rc messages */
	return 0;
}

/* ===========================================================================
-- Shutdown the TL interface (close and free all cameras, close SDK, etc.)
--
-- Usage: int TL_Shutdown();
--
-- Inputs: none
--
-- Output: Internal structures and memory released
--
-- Return: 0 if successful, otherwise bit-wise indicator of errors encountered
--          0x01 - failed to shutdown camera sdk dll's
--          0x02 - failed to shutdown camera sdk
--				0x04 - failed to shutdown color processing sdk
=========================================================================== */
int TL_Shutdown(void) {
	static char *rname = "TL_Shutdown";
	
	int rc = 0;

	if (! TL_is_initialized) return 0;

	/* Assume success */
	rc = 0;

	/* Close and forget all cameras */
	TL_ForgetAllCameras();

	/* Unregister the camera connect/disconnect event callbacks. */
	tl_camera_set_camera_connect_callback(NULL, 0);
	tl_camera_set_camera_disconnect_callback(NULL, 0);

	/* Close the color processing SDK */
	if (is_mono_to_color_sdk_open) {
		if (tl_mono_to_color_processing_terminate() != 0) {
			fprintf(stderr, "Failed to close mono to color SDK!\n"); fflush(stderr);
			rc |= 0x04;
		}
		is_mono_to_color_sdk_open = FALSE;
	}

	/* Close the camera SDK */
	if (is_camera_sdk_open) {
		if (tl_camera_close_sdk() != 0) {
			fprintf(stderr, "Failed to close camera SDK!\n"); fflush(stderr);
			rc |= 0x02;
		}
		is_camera_sdk_open = FALSE;
	}

	/* Close the camera DLL's */
	if (is_camera_dll_open) {
		if (tl_camera_sdk_dll_terminate() != 0) {
			fprintf(stderr, "Failed to close camera dll!\n"); fflush(stderr);
			rc |= 0x01;
		}
		is_camera_dll_open = FALSE;
	}

	/* Mark we are closed for business and return informational errors */
	TL_is_initialized = FALSE;
	return rc;
}


/* ===========================================================================
-- Routine to return a list (and load info) on all available TL cameras connected
--
-- Usage: int TL_FindAllCameras(TL_CAMERA **pcamera_list[]);
--
-- Inputs: camera_list - pointer to variable to recieve address of array
--                       of TL_CAMERA structures
--
-- Output: *camera_list - pointer to an array with list of TL_CAMERA * pointers
--                        this is a malloc'd space and caller responsible for
--                        releasing when done with the information
--
-- Return: number of cameras detected or negative on errors
--          -1 => TL interface not (and cannot be) initialized
--          -2 => Unable to allocate memory for the list
--
-- Notes: Opens and queries properties of all cameras but does not instantiate
--        any of the memory arrays.  TL_OpenCamera must be called before the
--        camera can actually be used.
=========================================================================== */
int TL_FindAllCameras(TL_CAMERA **plist[]) {
	static char *rname = "TL_FindAllCameras";

	char *aptr, *bptr, szBuf[256];
	int rc;
	TL_CAMERA *tl;

	/* Initialize the return values */
	if (plist != NULL) *plist = NULL;

	/* Initialize now if not done before */
	if ( TL_Initialize() != 0) {
		fprintf(stderr, "[%s] TL not (and cannot be) initialized\n", rname); fflush(stderr);
		return -1;
	}

	/* Query the list of known camera ID's */
	*szBuf = 0;
	if ( (rc = tl_camera_discover_available_cameras(szBuf, sizeof(szBuf))) != 0) { 
		TL_CameraErrMsg(rc, "Call to discover cameras failed", rname);
		return 0;
	} else if (*szBuf == '\0') {
		fprintf(stderr, "[%s] No cameras identified\n", rname); fflush(stderr);
		return 0;
	} 
	fprintf(stderr, "[%s] TL Cameras (full list): %s\n", rname, szBuf);  fflush(stderr);
	szBuf[strlen(szBuf)] = '\0';				/* Double NULL terminate */

	/* Code maintains list of open cameras and will not double open */
	for (aptr=szBuf; *aptr!='\0'; aptr+=strlen(aptr)) {
		if ( (bptr = strchr(aptr, ' ')) != NULL) *bptr = '\0';
		if ( (tl = TL_FindCamera(aptr, &rc)) == NULL) {
			fprintf(stderr, "[%s] Failed to open camera with ID %s (rc = %d)\n", rname, aptr, rc);
			fflush(stderr);
		} else {
			fprintf(stderr, "Camera %s:   handle: 0x%p\n", tl->ID, tl->handle);
			fprintf(stderr, "   Model: %s\n", tl->model);
			fprintf(stderr, "   Serial Number: %s\n", tl->serial);
			fprintf(stderr, "   Bit depth: %d\n", tl->bit_depth);
			fprintf(stderr, "   Size %d x %d\n", tl->width, tl->height); 
			fprintf(stderr, "   Exposure (us):  %lld (%lld < exp < %lld)\n", tl->us_expose, tl->us_expose_min, tl->us_expose_max);
			fprintf(stderr, "   Framerate: %.2f < fps < %.2f\n", tl->fps_min, tl->fps_max);
			fprintf(stderr, "   Pixel size (um): %.3f x %.3f\n", tl->pixel_width_um, tl->pixel_height_um);
			fprintf(stderr, "   Bytes per pixel: %d\n", tl->pixel_bytes);
			fprintf(stderr, "   Clock rate (Hz): %d\n", tl->clock_Hz);
			fflush(stderr);
		}
	}

	/* Return pointer to full list and count */
	if (plist != NULL && tl_camera_count > 0) {
		*plist = calloc(tl_camera_count, sizeof(**plist));
		memcpy(*plist, tl_camera_list, tl_camera_count*sizeof(**plist));
	}
	return tl_camera_count;
}

/* ===========================================================================
-- Routine to find a camera associated with a given ID
--
-- Usage: TL_CAMERA *TL_FindCamera(char *ID, int *rcode);
--
-- Inputs: ID - character string ID of camera (serial number) - generally from camera enumeration
--         rc - pointer to variable receive error codes (or NULL if unneeded)
--
-- Output: Creates basic structures and parameters for a TL camera
--         *rcode - if rcode != NULL, error code with 0 if successful
--            1 - too many cameras already found
--            2 - camera failed to be identified
--
-- Return: pointer to the camera, or NULL on error
--
-- Note: The TL_CAMERA structure returned by this function is guarenteed to
--       information concerning capabilities.  In addition, IT MAY HAVE
--       already been initialized with buffers, but this is not guarenteed.
--       Use TL_OpenCamera to ensure that buffers for the camera are
--       initialized.
--
--       Before use, must call TL_OpenCamera to initialize properly
--				tl->handle
--				tl->color_processor
--				tl->image_mutex
--				tl->raw,red,green,blue,rgb24
=========================================================================== */
TL_CAMERA *TL_FindCamera(char *ID, int *rcode) {
	static char *rname = "TL_FindCamera";

	TL_CAMERA *tl;
	FILE *handle;
 	int rc, myrcode, ilow, ihigh;

	/* Just have something where *rc works */
	if (rcode == NULL) rcode = &myrcode;

	/* Assume we will be successful */
	*rcode = 0;										

	/* First make sure we don't already have it opened */
	if ( (tl = TL_FindCameraByID(ID)) != NULL) return tl;

	/* Make sure there is an open slot for the camera */
	if (tl_camera_count >= TL_MAX_CAMERAS) {
		fprintf(stderr, "[%s] Error opening camera %s: Too many cameras already open\n", rname, ID); fflush(stderr);
		*rcode = 1;
		return NULL;
	}

	/* Open the camera, get handle, and make sure all seems okay */
	if ( (rc = tl_camera_open_camera(ID, &handle)) != 0) {
		char szBuf[64];
		sprintf_s(szBuf, sizeof(szBuf), "Error opening camera %s", ID);
		TL_CameraErrMsg(rc, szBuf, rname);
		*rcode = 2;
		return NULL;
	}

	/* Create a structure for the camera now */
	tl = calloc(1, sizeof(*tl));
	tl->magic = TL_CAMERA_MAGIC;
	strcpy_s(tl->ID, sizeof(tl->ID), ID);
	tl->handle = handle;

	/* And get lots of information about the camera */
	if ( (rc = tl_camera_get_model                       (handle,  tl->model, sizeof(tl->model)))       != 0) TL_CameraErrMsg(rc, "Error determining model for camera", rname);
	if ( (rc = tl_camera_get_serial_number               (handle,  tl->serial, sizeof(tl->serial)))     != 0) TL_CameraErrMsg(rc, "Error determining serial number for camera", rname);
	if ( (rc = tl_camera_get_name								  (handle,  tl->name, sizeof(tl->name)))         != 0) TL_CameraErrMsg(rc, "Error determining name for camera", rname);
	if ( (rc = tl_camera_get_firmware_version				  (handle,  tl->firmware, sizeof(tl->firmware))) != 0) TL_CameraErrMsg(rc, "Error determining firmware for camera", rname);

	if ( (rc = tl_camera_get_camera_sensor_type          (handle, &tl->sensor_type))      != 0) TL_CameraErrMsg(rc, "Error determining sensor type for camera", rname);
	if ( (rc = tl_camera_get_color_filter_array_phase    (handle, &tl->color_filter))     != 0) TL_CameraErrMsg(rc, "Error determining color filter array for camera", rname);
	if ( (rc = tl_camera_get_color_correction_matrix     (handle,  tl->color_correction)) != 0) TL_CameraErrMsg(rc, "Error determining color correction for camera", rname);
	if ( (rc = tl_camera_get_default_white_balance_matrix(handle,  tl->white_balance))    != 0) TL_CameraErrMsg(rc, "Error determining white balance for camera", rname);

	if ( (rc = tl_camera_get_image_width                 (handle, &tl->width))            != 0) TL_CameraErrMsg(rc, "Unable to get image width", rname);
	if ( (rc = tl_camera_get_image_height                (handle, &tl->height))           != 0) TL_CameraErrMsg(rc, "Unable to get image height", rname);
	if ( (rc = tl_camera_get_bit_depth                   (handle, &tl->bit_depth))        != 0) TL_CameraErrMsg(rc, "Error determining bit depth for camera", rname);

	if ( (rc = tl_camera_get_sensor_pixel_height         (handle, &tl->pixel_height_um))  != 0) TL_CameraErrMsg(rc, "Error determining pixel height for camera", rname);
	if ( (rc = tl_camera_get_sensor_pixel_width          (handle, &tl->pixel_width_um))   != 0) TL_CameraErrMsg(rc, "Error determining pixel width for camera", rname);
	if ( (rc = tl_camera_get_sensor_pixel_size_bytes     (handle, &tl->pixel_bytes))      != 0) TL_CameraErrMsg(rc, "Error determining pixel bytes for camera", rname);
	tl->image_bytes = tl->pixel_bytes * tl->width * tl->height;

	if ( (rc = tl_camera_get_exposure_time_range         (handle, &tl->us_expose_min, &tl->us_expose_max)) != 0) TL_CameraErrMsg(rc, "Error determining min/max exposure time for camera", rname);
	if ( (rc = tl_camera_get_exposure_time               (handle, &tl->us_expose))        != 0) TL_CameraErrMsg(rc, "Unable to set exposure time for camera", rname);

	if ( (rc = tl_camera_get_timestamp_clock_frequency	  (handle, &tl->clock_Hz))         != 0) TL_CameraErrMsg(rc, "Unable to get camera clock frequency", rname);

	if ( (rc = tl_camera_get_gain_range                  (handle, &ilow, &ihigh))         != 0) TL_CameraErrMsg(rc, "Unable to get gain range for camera", rname);
	tl->bGainControl = ihigh > 0;		
	tl->db_min = 0.1*ilow; tl->db_max = 0.1*ihigh;

	/* Try to enable framerate control */
	tl->bFrameRateControl = FALSE;					/* Default unless everything succeeds */
	tl->fps_min = 1; tl->fps_max = 40;
	if ( (rc = tl_camera_set_is_frame_rate_control_enabled(handle, TRUE)) != 0) {
		TL_CameraErrMsg(rc, "Unable to enable frame rate control", rname);
	} else if ( (rc = tl_camera_get_is_frame_rate_control_enabled(handle, &tl->bFrameRateControl)) != 0)  {
		TL_CameraErrMsg(rc, "Enabled frame rate, but then failed to verify", rname);
	} else if ( (rc = tl_camera_get_frame_rate_control_value_range(handle, &tl->fps_min, &tl->fps_max)) != 0) {
		TL_CameraErrMsg(rc, "Error determining min/max frame rate for camera", rname);
	} 

	if ( (rc = tl_camera_close_camera(tl->handle)) != 0) TL_CameraErrMsg(rc, "Failed to close camera", rname);
	tl->handle = NULL;

	/* Register in my list of known cameras and return with no error */
	tl_camera_list[tl_camera_count++] = tl;

	/* And return camera (*rcode already set at start) */
	return tl;
}


/* ===========================================================================
-- Routine to open a camera for use (active)
--
-- Usage: int TL_OpenCamera(TL_CAMERA *tl, int nBuf);
--
-- Inputs: tl - a partially completed structure from TL_FindCamera()
--         nBuf   - number of buffers to allocate in the ring (TL_MAX_RING_SIZE)
--
-- Output: Completes opening the Creates structures and processors for a TL camera
--         *rc - if rc != NULL, error code with 0 if successful
--            1 - too many cameras already open
--            2 - camera failed to open
--
-- Return: 0 if successful, otherwise a bit-wise error code (maybe single bit)
--            0x8000 - fatal error (no use even trying to use)
--            0x01   - Invalid camera structure (or released) (fatal)
--            0x02   - Unable to open a handle to the camera (fatal)
--            0x04   - Unable to allocate the color processor
--            0x10   - Unable to register the frame handling callback
--
-- Additional calls that might be used for more information
--      TL_CAMERA_GET_IS_OPERATION_MODE_SUPPORTED
=========================================================================== */
int TL_OpenCamera(TL_CAMERA *tl, int nBuf) {
	static char *rname = "TL_OpenCamera";

	FILE *handle;
	int rc, rcode;

	/* Verify that the structure is valid and hasn't already been closed */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 0x8001;

	/* Are we already open and initialized? */
	if (tl->handle != NULL) return 0;	

	/* Open the camera first and make sure it exists */
	if ( (rc = tl_camera_open_camera(tl->ID, &tl->handle)) != 0) {
		TL_CameraErrMsg(rc, "Error opening camera", rname);
		tl->handle = NULL;
		return 0x8002;
	}
	handle = tl->handle;
	
	/* Flash the camera's light to show which one is being initialized */
	tl_camera_set_is_led_on(handle, FALSE); Sleep(100); tl_camera_set_is_led_on(handle, TRUE); 

	/* Open the color image processor if camera is a color ... and set default linear RGB */
	rcode = 0;
	if (tl->sensor_type == TL_CAMERA_SENSOR_TYPE_BAYER) {
		if ( (rc = tl_mono_to_color_create_mono_to_color_processor(tl->sensor_type, tl->color_filter, tl->color_correction, tl->white_balance, tl->bit_depth, &tl->color_processor)) != 0) {
			TL_CameraErrMsg(rc, "Failed to create a color to mono processor for camera", rname);
			tl->color_processor = NULL;
			rcode |= 0x04;
		} else {
			/* Use TL_MONO_TO_COLOR_SPACE_SRGB if really interested in photographs */
			if ( (rc = tl_mono_to_color_set_color_space(tl->color_processor, TL_MONO_TO_COLOR_SPACE_LINEAR_SRGB)) != 0) { 
				TL_CameraErrMsg(rc, "Failed to set linear RGB", rname);
				rcode |= 0x08;			/* Failed to set linear RGB model */
			}
		}
	}

	/* Simple flag to indicate if we are color or B/W */
	tl->IsSensorColor = tl->sensor_type == TL_CAMERA_SENSOR_TYPE_BAYER;

	/* Create a semaphore for access to the data itself */
	tl->image_mutex = CreateMutex(NULL, FALSE, NULL);

	/* Figure out how big image buffer needs to be (assume 16-bit values) */
	tl->npixels = tl->width * tl->height;
	tl->nbytes_raw = sizeof(unsigned short) * tl->npixels;

	/* Allocate buffers using common code (same as change) */
	TL_SetRingBufferSize(tl, nBuf);			/* mutex has to be defined first */
	
	/* If color sensor, create RGB channels for a separation of a raw frame */
	if (tl->IsSensorColor) {
		tl->nbytes_red   = tl->nbytes_raw / 4;
		tl->nbytes_green = tl->nbytes_raw / 2;
		tl->nbytes_blue  = tl->nbytes_raw / 4;
		tl->red   = realloc(tl->red,   tl->nbytes_red);
		tl->green = realloc(tl->green, tl->nbytes_green);
		tl->blue  = realloc(tl->blue,  tl->nbytes_blue);
	}
	/* If color sensor, create RGB combined buffer */
	if (tl->IsSensorColor) {
		tl->rgb24_nbytes = 3 * tl->width * tl->height;
		tl->rgb24 = realloc(tl->rgb24, tl->rgb24_nbytes);
	}

	/* Query the initial gains so could be reset later */
	TL_GetMasterGain(tl, &tl->db_dflt);
	TL_GetRGBGains(tl, &tl->red_dflt, &tl->green_dflt, &tl->blue_dflt);

	/* Register the routine that will process images */
	if ( (rc = tl_camera_set_frame_available_callback(handle, frame_available_callback, 0)) != 0) { 
		TL_CameraErrMsg(rc, "Unable to set frame_available_callback", rname);
		rcode |= 0x10;
	}

	/* Clear the timestamps on separations just in case */
	tl->rgb24_imageID = tl->separations_imageID = -1;

	/* Return either 0 or one/more of errors 0x04 and/or 0x08 */
	return rcode;
}

/* ===========================================================================
-- Get information about the camera
--
-- Usage: int TL_GetCameraInfo(TL_CAMERA *tl, CAMERA_INFO *info);
--
-- Inputs: tl   - pointer to camera structure
--         info - pointer to structure to receive camera information
--
-- Output: *info (if not NULL)
--
-- Return: 0 if successful, 1 if no camera initialized
=========================================================================== */
int TL_GetCameraInfo(TL_CAMERA *tl, CAMERA_INFO *info) {
	static char *rname = "TL_GetCameraInfo";

	/* Must be valid structure */
	if (info != NULL) { memset(info, 0, sizeof(*info)); info->type = CAMERA_TL; }

	/* Validate structure and camera active */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC || tl->handle == NULL) return 1;

	/* If info NULL, just state we are alive and return */
	if (info == NULL) return 0;

	/* Return sensor and camera information from DCX_CAMERA structure */
	strcpy_m(info->name,				sizeof(info->name),			 tl->name);
	strcpy_m(info->model,			sizeof(info->model),			 tl->model);
	strcpy_m(info->serial,			sizeof(info->serial),		 tl->serial);
	strcpy_m(info->manufacturer,	sizeof(info->manufacturer), "ThorLabs");
	strcpy_m(info->version,			sizeof(info->version),		 "<unknown>");
	strcpy_m(info->date,				sizeof(info->date),			 "<unknown>");

	info->x_pixel_um = tl->pixel_height_um;
	info->y_pixel_um = tl->pixel_width_um;
	info->bColor     = tl->IsSensorColor;
	info->height     = tl->height;
	info->width      = tl->width;

	return 0;
}

/* ===========================================================================
-- Allocate (or deallocate) ring buffer for images
--
-- Usage: int TL_SetRingBufferSize(TL_CAMERA *tl, int nBuf);
--
-- Inputs: camera - a partially completed structure from TL_FindCamera()
--         nBuf   - number of buffers to allocate in the ring (TL_MAX_RING_SIZE)
--
-- Output: Stops processing for a moment, changes buffers, and restarts
--
-- Return: Number of buffers or 0 on fatal errors
--
-- Note: A request can be ignored and will return previous size
=========================================================================== */
int TL_SetRingBufferSize(TL_CAMERA *tl, int nBuf) {
	static char *rname = "TL_SetRingBufferSize";

	int i;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC || tl->handle == NULL) return 0;

	/* Get access to the memory structures */
	if (WAIT_OBJECT_0 != WaitForSingleObject(tl->image_mutex, 5*TL_IMAGE_ACCESS_TIMEOUT)) {
		fprintf(stderr, "[%s] Unable to get image memory semaphore to modify ring buffer structures\n", rname); fflush(stderr);
		return 0;
	}

	/* Have the semaphore, proceed to change size */
	nBuf = max(1, min(TL_MAX_RING_SIZE, nBuf));

	/* If shrinking, release image memory but TL_IMAGE too small to bother releasing unused image[]	*/
	/* If expanding, assume getting nBuf but may end up having unneeded space in tl->images[] also	*/
	if (nBuf < tl->nBuffers) {
		for (i=nBuf; i<tl->nBuffers; i++) free(tl->images[i].raw);							/* Release image buffer (big)	*/

	} else if (nBuf > tl->nBuffers) {																/* Increase number of buffers	*/
		tl->images = realloc(tl->images, nBuf*sizeof(*tl->images));							/* Space for full request		*/
		memset(tl->images+tl->nBuffers, 0, (nBuf-tl->nBuffers)*sizeof(*tl->images));	/* Clear new TL_IMAGE blocks	*/
		for (i=tl->nBuffers; i<nBuf; i++) {															/* Start adding image buffers	*/
			tl->images[i].tl    = tl;																	/* Access to other info			*/
			tl->images[i].index = i;																	/* Immediately index				*/
			if ( (tl->images[i].raw = malloc(tl->nbytes_raw)) == NULL) {
				fprintf(stderr, "[%s] Only able to increase buffer size to %d\n", rname, i); fflush(stderr);
				nBuf = i;
				break;
			}
		}
	}
	tl->nBuffers  = nBuf;
	tl->nValid = tl->iLast = tl->iShow = 0;

	ReleaseMutex(tl->image_mutex);
	return tl->nBuffers;
}

/* ===========================================================================
-- Reset the ring buffer counters so the next image will be in location 0
-- Primarily a Client/Server call for burst mode operation.  While other
-- commands may also reset these counters, this routine ensures a reset.
--
-- Usage: int TL_ResetRingCounters(TL_CAMERA *tl);
--
-- Inputs: camera - pointer to valid opened camera
--
-- Output: Resets buffers so next image will be 0
--
-- Return: 0 on success, !0 if errors (not initialized?)
=========================================================================== */
int TL_ResetRingCounters(TL_CAMERA *tl) {
	static char *rname = "TL_ResetRingCounters";
	
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Simple since this code manages where the data goes */
	tl->nValid = tl->iLast = tl->iShow = 0;
	return 0;
}

/* ===========================================================================
-- Routine to close a camera and release associated resources
--
-- Usage: int TL_CloseCamera(TL_CAMERA *tl);
--
-- Inputs: camera - pointer to valid opened camera
--                  (if NULL, is a nop returning rc=1)
--
-- Output: Closes camera and releases resources
--         Camera remains in the list of known cameras however
--
-- Return: 0 if successful, otherwise error code
--           0x01 -> camera is invalid (NULL, invalid, or already closed)
--           0x02 -> failed to disarm the camera handle
--           0x04 -> failed to close the camera handle
--           0x08 -> failed to release the color processor
=========================================================================== */
int TL_CloseCamera(TL_CAMERA *tl) {
	static char *rname = "TL_CloseCamera";

	int rc, rcode;
	
	/* Assume successful return */
	rcode = 0;

	/* Verify that the structure is valid and hasn't already been closed */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;
	if (tl->handle == NULL) return 0;						/* Already closed ... can call multiple */
	
	/* Disarm the camera */
	if ( (rc = tl_camera_disarm(tl->handle)) != 0) {
		TL_CameraErrMsg(rc, "Unable to disarm camera", rname);
		rcode |= 0x02;
	}

	/* Close the camera handle */
	if ( (rc = tl_camera_close_camera(tl->handle)) != 0) {
		TL_CameraErrMsg(rc, "Failed to close camera", rname);
		rcode |= 0x04;
	}
	tl->handle = NULL;

	/* Destroy the color processor */
	if (tl->color_processor != NULL) {
		if ( (rc = tl_mono_to_color_destroy_mono_to_color_processor(tl->color_processor)) != 0) {
			TL_CameraErrMsg(rc, "Failed to destory color processor for camera", rname);
			rcode |= 0x08;
		}
		tl->color_processor = NULL;
	}

	/* Release allocated memory for the image */
	if (tl->images != NULL) {
		int i;
		for (i=0; i<tl->nBuffers; i++) free(tl->images[i].raw);
		free(tl->images); tl->images = NULL;
	}
	if (tl->red       != NULL) { free(tl->red);       tl->red       = NULL; }
	if (tl->green     != NULL) { free(tl->green);     tl->green     = NULL; }
	if (tl->blue      != NULL) { free(tl->blue);      tl->blue      = NULL; }
	if (tl->rgb24     != NULL) { free(tl->rgb24);     tl->rgb24     = NULL; }

	/* Release semaphores */
	CloseHandle(tl->image_mutex);

	return rcode;
}


/* ===========================================================================
-- Routine to completely remove a camera from the list.  Does an implicit
-- TL_CloseCamera before releasing the entire structure.  After this call,
-- the camera structure will be invalid and not in the mast list.
--
-- Usage: int TL_ForgetCamera(TL_CAMERA *tl);
--
-- Inputs: camera - pointer to valid opened camera
--                  (if NULL, is a nop returning rc=1)
--
-- Output: Closes camera, releases resources, and release the structure
--         Camera no longer will be in the list of known cameras
--
-- Return: 0 if successful, otherwise error code
--           1 -> camera is invalid (NULL, invalid, or already closed)
=========================================================================== */
int TL_ForgetCamera(TL_CAMERA *tl) {
	static char *rname = "TL_ForgetCamera";

	int i,j;

	/* Verify that the structure is valid and hasn't already been closed */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Close if not already done */
	TL_CloseCamera(tl);

	/* Remove from the list of known cameras */
	for (i=0; i<tl_camera_count; i++) {
		if (tl_camera_list[i] == tl) {
			for (j=i; j<tl_camera_count-1; j++) tl_camera_list[j] = tl_camera_list[j+1];
			tl_camera_list[j] = NULL;
			tl_camera_count--;
			break;
		}
	}

	/* Finally mark the structure invalid and release the structure itself */
	tl->magic = 0;
	free(tl);
	return 0;
}

/* ===========================================================================
-- Routine to close all cameras that have been opened in one call
--
-- Usage: int TL_CloseAllCameras(void);
--
-- Inputs: none
--
-- Output: Closes all cameras and releases allocated resources
--
-- Return: 0
=========================================================================== */
int TL_CloseAllCameras(void) {
	static char *rname = "TL_CloseAllCameras";

	int i;
	for (i=0; i<TL_MAX_CAMERAS; i++) TL_CloseCamera(tl_camera_list[i]);
	return 0;
}

/* ===========================================================================
-- Routine to foget all cameras in the current known list
--
-- Usage: int TL_ForgetAllCameras(void);
--
-- Inputs: none
--
-- Output: Closes and forgets all cameras, releasing allocated resources
--
-- Return: 0
=========================================================================== */
int TL_ForgetAllCameras(void) {
	static char *rname = "TL_ForgetAllCameras";

	while (tl_camera_list[0] != NULL) TL_ForgetCamera(tl_camera_list[0]);
	return 0;
}

/* ===========================================================================
-- Routine to locate a camera within the list of initialzed cameras based on
-- the ID of the camera or on the handle associated with the camera.
--
-- Usage: TL_CAMERA *TL_FindCameraByID(char *ID);
--        TL_CAMERA *TL_FindCameraByHandle(void *handle);
--        TL_CAMERA *TL_FindCameraByIndex(int index);
--
-- Inputs: ID     - string with ID of camera (serial number)
--         handle - handle of the opened camera ("sender" frame callback)
--
-- Output: none
--
-- Return: pointer to the TL_CAMERA * structure or NULL if not found
=========================================================================== */
TL_CAMERA *TL_FindCameraByIndex(int index) {
	static char *rname = "TL_FindCameraByIndex";

	return (index >= 0 && index < TL_MAX_CAMERAS) ? tl_camera_list[index] : NULL ;
}

TL_CAMERA *TL_FindCameraByID(char *ID) {
	static char *rname = "TL_FindCameraByID";

	int i;
	for (i=0; i<TL_MAX_CAMERAS; i++) {
		if (tl_camera_list[i] != NULL && _stricmp(tl_camera_list[i]->ID, ID) == 0) return tl_camera_list[i];
	}
	return NULL;
}

TL_CAMERA *TL_FindCameraByHandle(void *handle) {
	static char *rname = "TL_FindCameraByHandle";

	int i;
	for (i=0; i<TL_MAX_CAMERAS; i++) {
		if (tl_camera_list[i] != NULL && tl_camera_list[i]->handle == handle) return tl_camera_list[i];
	}
	return NULL;
}


/* ===========================================================================
-- Enumerate list of available cameras into an array structure for use with the
-- combobox dialog controls and OpenCamera control
--
-- Usage: int TL_EnumCameraList(int *pcount, TL_CAMERA_INFO **pinfo) {
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
int TL_EnumCameraList(int *pcount, TL_CAMERA **pinfo[]) {
	static char *rname = "TL_EnumCameraList";

	int i, count;
	TL_CAMERA **camera_list;

	/* Initial return values */
	if (pcount != NULL) *pcount = 0;
	if (pinfo  != NULL) *pinfo = NULL;

	/* Determine how many cameras are connected and build the combo box with the information */
	if ( (count = TL_FindAllCameras(&camera_list)) < 0) {
		fprintf(stderr, "TL_FindAllCameras() returned error (%d)\n", count);
		fflush(stderr);
		count = 0;
	}
	fprintf(stderr, "Number of TL cameras reported: %d\n", count); fflush(stderr);

	/* Make a local copy that the calling routine will free */
	if (count > 0 && pinfo != NULL) {
		*pinfo = calloc(count, sizeof(**pinfo));
		for (i=0; i<count; i++) (*pinfo)[i] = camera_list[i];
	} 
	if (pcount != NULL) *pcount = count;
	return count;
}


/* ===========================================================================
-- Test if a given camera pointer is active and live
--
-- Usage: BOOL TL_IsValidCamera(TL_CAMERA *tl);
--
-- Inputs: camera - pointer to a TL_CAMERA structure ... hopefully opened
--
-- Output: none
--
-- Return: TRUE if pointer appears to be to a valid active camera structure
=========================================================================== */
BOOL TL_IsValidCamera(TL_CAMERA *tl) {
	static char *rname = "TL_IsValidCamera";

	return tl != NULL && tl->magic == TL_CAMERA_MAGIC;
}

/* ===========================================================================
-- Routine to request a signal when new image arrives for a camera
--
-- Usage: int TL_AddImageSignal(TL_CAMERA *tl, HANDLE signal);
--
-- Inputs: camera - valid pointer to an existing opened camera
--         signal - valid handle to an event semaphore
--
-- Output: Adds the signal to a list of signals that will be triggered
--         when a new image is valid in the camera buffers.  The signal
--         should be removed when it is no longer needed (limited #)
--
-- Return: 0 if successful
--
-- Notes: When a new image is available in the camera buffers, all registered
--        signals will be pulsed via "SetEvent(signal)". The owner of the
--        event semaphore should probably create the semaphore with auto
--        reset corresponding to a single release; but up to you.  The data
--        in the buffer will be valid and you may want to use the
--        "tl->image_mutex" to ensure data is not changed before all
--        processing is complete.
=========================================================================== */
int TL_AddImageSignal(TL_CAMERA *tl, HANDLE signal) {
	static char *rname = "TL_AddImageSignal";

	int i;

	/* Verify that the structure is valid and hasn't already been closed */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	for (i=0; i<TL_MAX_SIGNALS; i++) {
		if (tl->new_image_signals[i] == NULL) {
			tl->new_image_signals[i] = signal;
			return 0;
		}
	}
	
	/* No space for another signal */
	return 2;
}

/* ===========================================================================
-- Routine to remove an event semaphore signal from the active list
--
-- Usage: int TL_RemoveImageSignal(TL_CAMERA *tl, HANDLE signal);
--
-- Inputs: camera - valid pointer to an existing opened camera
--         signal - existing event semaphore signal to be removed
--
-- Output: Removes the signal from the list if it is there
--
-- Return: 0 if found, 1 if camera invalid, or 2 if signal wasn't in list
=========================================================================== */
int TL_RemoveImageSignal(TL_CAMERA *tl, HANDLE signal) {
	static char *rname = "TL_RemoveImageSignal";

	int i;

	/* Verify that the structure is valid and hasn't already been closed */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	for (i=0; i<TL_MAX_SIGNALS; i++) {
		if (tl->new_image_signals[i] == signal) {
			tl->new_image_signals[i] = NULL;
			return 0;
		}
	}

	/* Did not exist in the list */
	return 2;
}


/* ===========================================================================
-- Routines to be called when a camera connects / disconnects 
--
-- Usage: static void camera_connect_callback(char *camera_ID, enum TL_CAMERA_USB_PORT_TYPE usb_bus_speed, void *context);
--        static void camera_disconnect_callback(char *camera_ID, void *context);
--
-- Inputs: camera_ID     - character string of the camera connecting or disconnecting 
--         usb_bus_speed - usb bus speed
--         context       - unused
--
-- Output: none
--
-- Return: none
=========================================================================== */
static void camera_connect_callback(char *camera_ID, enum TL_CAMERA_USB_PORT_TYPE usb_bus_speed, void *context) {
	static char *rname = "camera_connect_callback";
	fprintf(stderr, "TL camera ID %s connected with bus speed = %d!\n", camera_ID, usb_bus_speed); fflush(stderr);
}

static void camera_disconnect_callback(char *camera_ID, void* context) {
	static char *rname = "camera_disconnect_callback";
	fprintf(stderr, "TL camera ID %s disconnected!\n", camera_ID); fflush(stderr);
}

/* ===========================================================================
-- Routine invoked when there is a new image to process from some camera.
-- Determines the appropriate camera and processes the image before signalling 
-- threads that are waiting for new images.
--
-- Usage: static void frame_available_callback(void* sender, 
--                                             unsigned short* image_buffer, 
--															  int frame_count, 
--															  unsigned char* metadata, 
--															  int metadata_size_in_bytes, 
--															  void* context);
--
-- Inputs: sender      - handle to the camera that generated the request
--         image_data  - raw image data
--         frame_count - frame number from the camera
--         metadata    - metadata in form of 4-char string 4-byte values
--         metadata_size_in_bytes - number of metadata blocks
--         context     - unused
--
-- Output: processes the new image and stores information in the camera
--         structure.  Note that all use of "raw_data" must be completed
--         before returning.
--
-- Return: none
--
-- Note: data organized as first row, second row, ...
--        Bayer is even rows: grgrgrgr .... 
--                  odd rows: bgbgbgbf .... 
=========================================================================== */
static void frame_available_callback(void* sender, unsigned short* image_buffer, int frame_count, unsigned char* metadata, int metadata_size_in_bytes, void* context) {
	static char *rname = "frame_available_callback";
	
	TL_CAMERA *tl;
	int i;

	/* As there may be other cameras, have global common image counter */
	int imageID;									/* ID for this invokation */
	static sig_atomic_t image_count = 0;	/* Every call increments */

	/* Variables to process the metadata */
	UINT32 dval;									/* 4-byte integer */
	union {
		UINT64 value;								/* 8-byte timestamp (in ns) */
		UINT32 word[2];
	} timestamp;
	char tag[5];

	static HIRES_TIMER *timer = NULL;

/* Immediately increment to have this call's image ID */
	imageID = image_count++;
	
/* Now start rest of the process */
	if (timer == NULL) timer = HiResTimerCreate();

	timestamp.value = 0;
	for (i=0; i<metadata_size_in_bytes; i+=8) {
		memcpy(&dval, metadata+i+4, 4);					/* Make a dval so can handle */
		memcpy(&tag,  metadata+i, 4); tag[4] = '\0';
		if (strcmp(tag, "TSI") == 0) {					/* Start of metadata tags*/
		} else if (strcmp(tag, "FCNT") == 0) {			/* Frame count */
		} else if (strcmp(tag, "IFMT") == 0) {			/* Image Data Format tag */
		} else if (strcmp(tag, "IOFF") == 0) {			/* Offset to data in 8-byte blocks tag */
		} else if (strcmp(tag, "PCKH") == 0) {			/* Pixel clock - upper 32 bits */
			timestamp.word[1] = dval;
		} else if (strcmp(tag, "PCKL") == 0) {			/* Pixel clock - lower 32 bits */
			timestamp.word[0] = dval;
		} else if (strcmp(tag, "ENDT") == 0) {			/* End tag (quit processing) */
			break;
		} else {
			fprintf(stderr, "Unrecognized metadata tag %s: 0x%8.8x\n", tag, dval); fflush(stderr);
		}
	}

	/* Find the camera generating the call */
	if ( (tl = TL_FindCameraByHandle(sender)) == NULL) {
		fprintf(stderr, "ERROR: Unable to identify the camera for this callback\n"); fflush(stderr);
		return;
	}

	/* Take control of the memory buffers */
	if (WAIT_OBJECT_0 == WaitForSingleObject(tl->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {
		int ibuf;							/* Which buffer gets the data */

		/* Put into the next available position */
		ibuf = (tl->nValid == 0) ? 0 : (tl->iLast+1) % tl->nBuffers;
		while (tl->images[ibuf].locks != 0) {				/* Find one that is unlocked */
			if (ibuf == tl->iLast) break;						/* Continuously use the last one if all locked */
			tl->nValid = max(tl->nValid, ibuf+1);			/* Increment valid for locked images */
			ibuf = (ibuf+1) % tl->nBuffers;
		}

		/* Save where we are and increment the number of valid (up to nBuffers) */
		tl->iLast = ibuf;
		tl->nValid = max(tl->nValid, ibuf+1);				/* Number now valid */
//		fprintf(stderr, "Reseting iLast and nValid: %d %d\n", tl->iLast, tl->nValid); fflush(stderr);

		/* Copy raw data from sensor (<0.45 ms) and generate metadata */
		/* Image timestamp documentation (page 42) incorrect ... clock seems to be exactly 99 MHz, not reported value */
		tl->images[ibuf].imageID = imageID;
		GetLocalTime(&tl->images[ibuf].system_time);
		tl->images[ibuf].timestamp = time(NULL);
		tl->images[ibuf].camera_time = timestamp.value/99000000.0;
		memcpy(tl->images[ibuf].raw, image_buffer, tl->nbytes_raw);

		/* Copy imaging conditions now */
		tl->images[ibuf].dB_gain   = tl->dB_gain;
		tl->images[ibuf].ms_expose = tl->ms_expose;
		tl->images[ibuf].valid = TRUE;

		/* Copy framecount and mark raw data valid, other datas "not done" */
		tl->frame_count = frame_count;

		/* These are optional ... no reason to do unless they are used later */
//		TL_ProcessRawSeparation(camera);
//		TL_ProcessRGB(camera);

		/* We are done needing exclusive access to the memory buffers */
		ReleaseMutex(tl->image_mutex);

//		fprintf(stderr, "[%4.4d] %10.6f:  %10.6f  sender: 0x%p  buffer: 0x%p  meta_buffer: 0x%p  size: %d\n", frame_count, HiResTimerDelta(timer), tl->timestamp, sender, image_buffer, metadata, metadata_size_in_bytes);

		/* Set all event semaphores that have been registered (want to process images) */
		for (i=0; i<TL_MAX_SIGNALS; i++) {
			if (tl->new_image_signals[i] != NULL) SetEvent(tl->new_image_signals[i]);
		}
	}

	return;
}

/* ===========================================================================
-- Convert the raw buffer in camera structure to separated red, green and blue
--
-- Usage: int TL_ProcessRawSeparation(TL_CAMERA *tl, int frame);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         frame  - frame to process from buffers (-1 ==> for most recent)
--
-- Output: fills in the ->red, ->green, ->blue buffers in camera
--
-- Return: 0 if successful.  
--            1 => not valid camera
--	           2 => no image yet valid in the camera structure
--            3 => one of the buffers needed has not been allocated
--            4 => unable to get the semaphore for image data access
--
-- Note: Calling thread is assumed to have tl->image_mutex semaphore
=========================================================================== */
int TL_ProcessRawSeparation(TL_CAMERA *tl, int frame) {
	static char *rname = "TL_ProcessRawSeparation";

	int rc, row,col;
	unsigned short *red, *green, *blue, *raw;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* If frame <0, implies want most recent image */
	if (frame < 0) frame = tl->iLast;

	/* Make sure we have valid data */
	if (! tl->images[frame].valid) return 2;

	/* Once done for a raw image, don't ever need to repeat */
	if (tl->separations_imageID == tl->images[frame].imageID) return 0;		/* Already done */

	/* And must have the structures defined */
	if (tl->images == NULL || tl->red == NULL || tl->green == NULL || tl->blue == NULL) return 3;

	/* Get control of the memory buffers */
	if (WAIT_OBJECT_0 == WaitForSingleObject(tl->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {

		/* Separate the raw signal into raw red/green/blue buffers */
		raw = tl->images[frame].raw;			/* Point to the requested buffer */
		red = tl->red; green = tl->green; blue = tl->blue;
		for (row=0; row<tl->height; row++) {
			for (col=0; col<tl->width; col+=2) {
				if (row%2 == 0) {																/* Even rows */
					*(green++) = *(raw++); *(red++) = *(raw++); 
				} else {
					*(blue++) = *(raw++); *(green++) = *(raw++);
				}
			}
		}
		tl->separations_imageID = tl->images[frame].imageID;
		ReleaseMutex(tl->image_mutex);												/* Done with the mutex */
		rc = 0;

	} else {
		rc = 4;
	}

	return rc;
}

/* ===========================================================================
-- Convert the raw buffer in camera structure to full RGB24 buffer
--
-- Usage: int TL_ProcessRGB(TL_CAMERA *tl, int frame);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         frame  - frame to process from buffers (-1 ==> for most recent)
--
-- Output: fills in the ->rgb24 buffer in camera
--
-- Return: 0 if successful.  
--            1 => not valid camera
--	           2 => no image yet valid in the camera structure
--            3 => no rgb24 buffer allocated (may not be color image)
--            4 => unable to get the semaphore for image data access
--            5 => no color processor loaded
--
-- Note: Calling thread is assumed to have tl->image_mutex semaphore
=========================================================================== */
int TL_ProcessRGB(TL_CAMERA *tl, int frame) {
	static char *rname = "TL_ProcessRGB";

	int rc;
	unsigned short *raw;
	
	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Make sure we have valid data */
	if (tl->images == NULL) return 2;

	/* If frame <0, implies want most recent image */
	if (frame < 0) frame = tl->iLast;

	/* Once done for a raw image, don't ever need to repeat */
	if (tl->rgb24_imageID == tl->images[frame].imageID) return 0;

	/* And must have the structures defined */
	if (tl->rgb24 == NULL) return 3;
	if (tl->color_processor == NULL) return 4;

	/* Get control of the memory buffers */
	if (WAIT_OBJECT_0 != WaitForSingleObject(tl->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {
		rc = 5;										/* Failed to get the semaphore error */
	} else {
		/* Convert to true RGB format */
		raw = tl->images[frame].raw;			/* Point to the requested buffer */
		if ( (rc = tl_mono_to_color_transform_to_24(tl->color_processor, raw, tl->width, tl->height, tl->rgb24)) != 0) {
			TL_CameraErrMsg(rc, "Unable to transform to rgb24", rname);
		} else {
			int i, itmp;
			for (i=0; i<3*tl->width*tl->height; i+=3) {					/* Reverse order or rgb to bgr to match DCX pattern */
				itmp = tl->rgb24[i+0];
				tl->rgb24[i+0] = tl->rgb24[i+2];
				tl->rgb24[i+2] = itmp;
			}
			tl->rgb24_imageID = tl->images[frame].imageID;
			rc = 0;
		}
		ReleaseMutex(tl->image_mutex);												/* Done with the mutex */
	}

	return rc;
}


/* ===========================================================================
-- Convert from internal structure to device independent bitmap (DIB) for display
-- 
-- Usage: BITMAPINFOHEADER *TL_GenerateDIB(TL_CAMERA *tl, int frame, int *rc);
--
-- Inputs: camera - an opened TL camera
--         frame  - frame to process from buffers (-1 ==> for most recent)
--         rc     - optional pointer to variable to retrieve specific error codes
--
-- Output: if rc != NULL, *rc has error code (or 0 if successful)
--				 0 ==> successful
--           1 ==> camera pointer not valid
--           2 ==> path invalid
--           3 ==> no RGB24 image data in camera structure
--           4 ==> unable to allocate memory for new RGB data
--           5 ==> unable to get the mutex semaphore
--
-- Return: pointer to bitmap or NULL on any error
=========================================================================== */
BITMAPINFOHEADER *TL_CreateDIB(TL_CAMERA *tl, int frame, int *rc) {
	static char *rname = "TL_CreateDIB";

	BITMAPINFOHEADER *bmih;
	unsigned char *data;
	int irow, icol, ineed;
	int my_rc;

	/* Make life easy if user doesn't want error codes */
	if (rc == NULL) rc = &my_rc;
	*rc = 0;

	/* Verify that the structure is valid and hasn't already been closed */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) { *rc = 1; return NULL; }
	if (tl->rgb24 == NULL) { *rc = 3; return NULL; }

	/* Make sure we have data and color information loaded */
	if (0 != (*rc = TL_ProcessRGB(tl, frame))) return NULL;

	/* Allocate the structure */
	ineed = sizeof(*bmih)+3*tl->width*tl->height;
	if ( (bmih = calloc(1, ineed)) == NULL) { *rc = 4; return NULL; }

	/* Fill in the bitmap information */
	bmih->biSize = sizeof(*bmih);					/* Only size of the header itself */
	bmih->biWidth         = tl->width;
	bmih->biHeight        = tl->height;	/* Make the image upright when saved */
	bmih->biPlanes        = 1;
	bmih->biBitCount      = 24;					/* Value for RGB24 color images */
	bmih->biCompression   = BI_RGB;
	bmih->biSizeImage     = 3*tl->width*tl->height;
	bmih->biXPelsPerMeter = 3780;					/* Just make it 96 ppi (doesn't matter) */
	bmih->biYPelsPerMeter = 3780;					/* Same value ThorCam reports */
	bmih->biClrUsed       = 0;
	bmih->biClrImportant  = 0;

	/* Get access to the mutex semaphore for the data processing */
	/* and save data reversing row sequence and byte sequence (colors / rotation) */
	/* Note, the reversal of bits in the rgb24 can be here or in ProcessRGB24 */
	if (WAIT_OBJECT_0 != WaitForSingleObject(tl->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {
		*rc = 5;
		return NULL;
	} else {

		data = ((unsigned char *) bmih) + sizeof(*bmih);		/* Where RGB in the bitmap really starts */
		for (irow=0; irow<tl->height; irow++) {
			for (icol=0; icol<tl->width; icol++) {
#if 0			/* Switch bgr to rgb here */
				data[3*(irow*tl->width+icol)+0] = tl->rgb24[3*((tl->height-1-irow)*tl->width+icol)+2];
				data[3*(irow*tl->width+icol)+1] = tl->rgb24[3*((tl->height-1-irow)*tl->width+icol)+1];
				data[3*(irow*tl->width+icol)+2] = tl->rgb24[3*((tl->height-1-irow)*tl->width+icol)+0];
#else			/* Already done in TL_ProcessRGB */
				data[3*(irow*tl->width+icol)+0] = tl->rgb24[3*((tl->height-1-irow)*tl->width+icol)+0];
				data[3*(irow*tl->width+icol)+1] = tl->rgb24[3*((tl->height-1-irow)*tl->width+icol)+1];
				data[3*(irow*tl->width+icol)+2] = tl->rgb24[3*((tl->height-1-irow)*tl->width+icol)+2];
#endif
			}
		}
	}

	ReleaseMutex(tl->image_mutex);									/* Done with the mutex */
	*rc = 0;
	return bmih;
}

/* ===========================================================================
-- Get information about a specific image
--
-- Usage: int TL_GetImageInfo(TL_CAMERA *tl, int frame, IMAGE_INFO *info);
--
-- Inputs: tl    - an opened TL camera
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
int TL_GetImageInfo(TL_CAMERA *tl, int frame, IMAGE_INFO *info) {
	static char *rname = "TL_GetImageInfo";

	TL_IMAGE *image;
	float R,G,B;

/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;
	
	if (frame == -1) frame = tl->iLast;								/* Last image */
	if (frame < 0 || frame > tl->nValid) return 2;
	if (info == NULL) return 0;
	memset(info, 0, sizeof(*info));

	/* Point to the appropriate image */
	image = &tl->images[frame];
	image->locks++;														/* Lock (or more lock) iamge */

	info->type         = CAMERA_TL;
	info->frame        = frame;
	info->timestamp    = image->timestamp;							/* When image acquired */
	info->camera_time  = image->camera_time;						/* Higher resolution time */
	info->width        = tl->width;
	info->height       = tl->height;
	info->memory_pitch = 2*tl->height;								/* 2 bytes, and no padding */
	info->exposure     = image->ms_expose;
	info->gamma        = 1.0;
	info->master_gain  = image->dB_gain;

	tl_mono_to_color_get_red_gain(tl->color_processor, &R);
	tl_mono_to_color_get_green_gain(tl->color_processor, &G);
	tl_mono_to_color_get_blue_gain(tl->color_processor, &B);
	info->red_gain = R;	info->green_gain = G;	info->blue_gain = B;

	info->color_correct_mode     = 0;
	info->color_correct_strength = 1.0;

	image->locks--;
	return 0;
}

/* ===========================================================================
-- Get pointer to raw data for a specific image, and length of that data
--
-- Usage: int TL_GetImageData(TL_CAMERA *tl, int frame, void **image_data, int *length);
--
-- Inputs: tl         - an opened TL camera
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
int TL_GetImageData(TL_CAMERA *tl, int frame, void **image_data, size_t *length) {
	static char *rname = "TL_GetImageData";

	TL_IMAGE *image;

	/* Default returns */
	if (image_data != NULL) *image_data = NULL;
	if (length     != NULL) *length = 0;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;
	
	if (frame == -1) frame = tl->iLast;								/* Last image */
	if (frame < 0 || frame > tl->nValid) return 2;

	/* Point to the appropriate image and copy pointers / length */
	image = &tl->images[frame];
	if (image_data != NULL) *image_data = (void *) image->raw;
	if (length     != NULL) *length = image->tl->image_bytes;

	return 0;
}


/* ===========================================================================
-- Determine formats that camera supports for writing
--
-- Usage: int TL_GetSaveFormatFlag(TL_CAMERA *tl);
--
-- Inputs: tl - an opened TL camera
--
-- Output: none
--
-- Return: Bit-wise flags giving camera capabilities
--				 FL_BMP | FL_JPEG | FL_RAW
--         0 on errors (no capabilities)
=========================================================================== */
int TL_GetSaveFormatFlag(TL_CAMERA *tl) {
	static char *rname = "TL_GetSaveFormatFlag";

	return FL_BMP | FL_RAW ;
}


/* ===========================================================================
-- Get a reasoanble filename for save if one isn't passed in the call (NULL)
--
-- Usage: int TL_GetSaveName(char *path, size_t length, FILE_FORMAT *format);
--
-- Inputs: path    - pointer to character string buffer for returned filename
--         length  - length of the path buffer
--         format  - pointer to get "format" implied by file extension
--
-- Output: *path   - filled with selected filename
--         *format - one of the flags FILE_BMP, FILE_RAW, ... from file extension
--
-- Return: 0 if successful, !0 on cancel of the file open dialog
=========================================================================== */
int TL_GetSaveName(char *path, size_t length, FILE_FORMAT *format) {
	static char *rname = "TL_GetSaveName";

	int i;
	char *aptr;
	static struct {
		char *ext;
		int flag;
	} exts[] = { {".bmp", FILE_BMP}, {".raw", FILE_RAW}, {".png", FILE_PNG}, {".jpg", FILE_JPG}, {".jpeg", FILE_JPG} };

	/* parameters for querying a pathname */
	static char local_dir[PATH_MAX]="";		/* Directory -- keep for multiple calls */
	OPENFILENAME ofn;

	/* Get a save-as filename */
	strcpy_s(path, length, "image");					/* Pathname must be initialized with a value (even if just '\0) */
	ofn.lStructSize       = sizeof(OPENFILENAME);
	ofn.hwndOwner         = HWND_DESKTOP;
	ofn.lpstrTitle        = "Save image";
	ofn.lpstrFilter       = "Bitmap format (*.bmp)\0*.bmp\0Raw camera format (*.raw)\0*.raw\0All files (*.*)\0*.*\0\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter    = 0;
	ofn.nFilterIndex      = 1;
	ofn.lpstrFile         = path;						/* Full path */
	ofn.nMaxFile          = (DWORD) length;
	ofn.lpstrFileTitle    = NULL;						/* Partial path */
	ofn.nMaxFileTitle     = 0;
	ofn.lpstrDefExt       = "bmp";
	ofn.lpstrInitialDir   = (*local_dir=='\0' ? "." : local_dir);
	ofn.Flags = OFN_LONGNAMES | OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;

	/* Query a filename ... if abandoned, just return now with no complaints */
	if (! GetSaveFileName(&ofn)) return 1;

	/* Save directory for subsequent calls */
	strcpy_s(local_dir, sizeof(local_dir), path);
	local_dir[ofn.nFileOffset-1] = '\0';

	/* Determine the file format from extension */
	if (format != NULL) {
		*format = FILE_BMP;												/* Default format is bitmap */
		aptr = path+strlen(path)-1;
		while (*aptr != '.' && aptr != path) aptr--;
		if (*aptr == '.') {											/* Modify to actual file type */
			for (i=0; i<sizeof(exts)/sizeof(exts[0]); i++) {
				if (_stricmp(aptr, exts[i].ext) == 0) *format = exts[i].flag;
			}
		}
	}

	return 0;
}


/* ===========================================================================
-- Save data from TL camera as a bitmap (.bmp) file
-- 
-- Usage: int TL_SaveImage(TL_CAMERA *tl, char *path, int frame, FILE_FORMAT format);
--
-- Inputs: tl    - an opened TL camera
--         path  - pointer to name of a file to save data (or NULL for query)
--         frame - frame to process from buffers (-1 ==> for most recent)
--         format - One of the FILE_XXX file formats (from camera.h)
--                  defaults to FILE_BMP if invalid format
--
-- Output: saves the data as an RGB uncompressed bitmap
--
-- Return: 0 if successful, otherwise an error code
--           1 ==> camera pointer not valid
--           2 ==> path invalid
--           3 ==> no RGB24 image data in camera structure
--           4 ==> unable to allocate memory for new RGB data
--           5 ==> unable to get the mutex semaphore
--           6 ==> file failed to open
=========================================================================== */
int TL_SaveImage(TL_CAMERA *tl, char *path, int frame, FILE_FORMAT format) {
	static char *rname = "TL_SaveImage";

	int rc;
	char pathname[MAX_PATH];
	
	/* If no name is given, then get one ourselves */
	if (path == NULL) {
		if ( (rc = TL_GetSaveName(pathname, sizeof(pathname), &format)) != 0) return rc;
		path = pathname;
	}
	
	/* Have a filename, now just save the data */
	switch (format) {
		case FILE_RAW:
			rc = TL_SaveRawImage(tl, path, frame);
			break;
		case FILE_BMP:
		default:
			rc = TL_SaveBMPImage(tl, path, frame);
			break;
	}
	return rc;
}


int TL_SaveBMPImage(TL_CAMERA *tl, char *path, int frame) {
	static char *rname = "TL_SaveBMPImage";

	BITMAPINFOHEADER *bmih=NULL;
	BITMAPFILEHEADER  bmfh;
	int rc, isize;
	FILE *funit;

	/* If frame <0, implies want most recent image */
	if (frame < 0) frame = tl->iLast;

	/* Verify all is okay and get a bitmap corresponding to current image */
	/* CreateDIB locks the semaphore while creating the DIB, but once
	 * the bmih is created, we no longer need access to the raw data */
	if ( (bmih = TL_CreateDIB(tl, frame, &rc)) == NULL) return rc;
	isize = sizeof(*bmih)+3*tl->width*tl->height;

	/* Create the file header */
	memset(&bmfh, 0, sizeof(bmfh));	
	bmfh.bfType = 19778;
	bmfh.bfSize = sizeof(bmfh)+isize;
	bmfh.bfOffBits = sizeof(bmfh)+sizeof(*bmih);

	if ( (fopen_s(&funit, path, "wb")) != 0) {
		fprintf(stderr, "[%s] Failed to open \"%s\"\n", rname, path); fflush(stderr);
		rc = 5;
	} else {
		fwrite(&bmfh, 1, sizeof(bmfh), funit);
		fwrite(bmih, 1, isize, funit);
		fclose(funit);
		rc = 0;
	}

	/* Free our data block and return error code (or 0 if successful) */
	free(bmih);
	return rc;
}

/* ===========================================================================
-- Save data from TL camera as a raw (.raw) file
-- 
-- Usage: int TL_SaveRawImage(TL_CAMERA *tl, char *path, int frame);
--
-- Inputs: camera - an opened TL camera
--         path   - pointer to name of a file to save data (or NULL for query)
--         frame  - frame to process from buffers (-1 ==> for most recent)
--
-- Output: saves the data in binary raw data format
--
-- Return: 0 if successful, otherwise an error code
--           1 ==> camera pointer not valid
--           2 ==> path invalid
--           3 ==> no RGB24 image data in camera structure
--           4 ==> unable to allocate memory for new RGB data
--           5 ==> unable to get the mutex semaphore
--           6 ==> file failed to open
=========================================================================== */
int TL_SaveRawImage(TL_CAMERA *tl, char *path, int frame) {
	static char *rname = "TL_SaveRawImage";

	FILE *funit;
	TL_RAW_FILE_HEADER header;
	TL_IMAGE *image;
	int dummy_zeros = 0;

	/* If frame <0, implies want most recent image */
	if (frame < 0) frame = tl->iLast;
	image = tl->images+frame;

	/* Create the file header */
	memset(&header, 0, sizeof(header));
	header.magic  = TL_RAW_FILE_MAGIC;
	header.header_size = sizeof(TL_RAW_FILE_HEADER);
	header.major_version = 1;		header.minor_version = 0;

	header.ms_expose = image->ms_expose;
	header.dB_gain   = image->dB_gain;			

	header.timestamp = image->timestamp;
	header.camera_time = image->camera_time;
	header.year = image->system_time.wYear; header.month = image->system_time.wMonth;	header.day = image->system_time.wDay;
	header.hour = image->system_time.wHour; header.min   = image->system_time.wMinute;	header.sec = image->system_time.wSecond;
	header.ms   = image->system_time.wMilliseconds;

	strcpy_s(header.camera_model,  sizeof(header.camera_model),  tl->model);
	strcpy_s(header.camera_serial, sizeof(header.camera_serial), tl->serial);
	header.sensor_type  = tl->sensor_type;		header.color_filter = tl->color_filter;

	header.width		  = tl->width;				header.height		  = tl->height;
	header.bit_depth    = tl->bit_depth;
	header.pixel_bytes  = tl->pixel_bytes;		header.image_bytes  = tl->image_bytes;
	header.pixel_width  = tl->pixel_width_um;	header.pixel_height = tl->pixel_height_um;

	if ( (fopen_s(&funit, path, "wb")) != 0) {
		fprintf(stderr, "[%s] Failed to open \"%s\"\n", rname, path); fflush(stderr);
		return 1;
	}

	/* Write out the header, followed immediately by the data */
	fwrite(&header, 1, sizeof(header), funit);
	fwrite(image->raw, 1, tl->nbytes_raw, funit);
	if (tl->nbytes_raw%4 != 0) fwrite(&dummy_zeros, 1, 4-tl->nbytes_raw%4, funit);
	fclose(funit);

	return 0;
}

/* ===========================================================================
-- Save all valid images that would have been collected in burst run
--
-- Usage: TL_SaveBurstImages(TL_CAMERA *tl, char *pattern, FILE_FORMAT format);
--
-- Inputs: tl      - pointer to active camera
--         pattern - root of name for files
--							  <pattern>.csv - logfile 
--                     <pattern>_ddd.bmp - individual images
--         format  - format to save images (FILE_BMP or FILE_RAW - default FILE_BMP)
--
-- Output: Saves stored images as a series of bitmaps
--
-- Return: 0 ==> successful
--         1 ==> rings are not enabled in the code
--         2 ==> buffers not yet allocated or no data
--         3 ==> save abandoned by choice in FileOpen dialog
=========================================================================== */
int TL_SaveBurstImages(TL_CAMERA *tl, char *pattern, FILE_FORMAT format) {
	static char *rname = "TL_SaveBurstImages";

	char pathname[PATH_MAX], *extension;
	int i, istart, icount, inow;
	double tstart;
	FILE *funit;

	/* Have we cycled through the rings, or still on first cycle? */
	if (tl->nValid < tl->nBuffers) {
		istart = 0;											/* Haven't wrapped yet, so first image in slot 0 */
		icount = tl->nValid;
	} else {
		istart = (tl->iLast+1) % tl->nBuffers;		/* Points now to first one saved (next to be overwritten) */
		icount = tl->nBuffers;
	}

	/* Open a .csv log file with information on each image */
	sprintf_s(pathname, sizeof(pathname), "%s.csv", pattern);
RetryFileOpen:
	fopen_s(&funit, pathname, "w");
	if (funit == NULL) {
		char szTmp[1024];
		int rc;
		sprintf_s(szTmp, sizeof(szTmp),
					 "Failed to open the logfile for information about the burst bitmaps.\n"
					 "   %s\n"
					 "Check that the file is not currently open and try again.\n", pathname);
		rc = MessageBox(NULL, szTmp, "File open failed", MB_ICONERROR | MB_RETRYCANCEL | MB_DEFBUTTON2);
		if (rc == IDRETRY) goto RetryFileOpen;
		return 3;
	}

	/* Generate the appropriate extension for the files - default is bmp */
	extension = (format == FILE_RAW) ? "raw" : "bmp";

	/* Header line for the csv file */
	fprintf(funit, "/* Index,filename,t_relative,t_time,t_clock\n");

	inow = istart;								/* Frame to start with */
	tstart = -999;								/* Flag to copy first available value */
	for (i=0; i<icount; i++) {
		if (! tl->images[inow].valid) continue;
		if (tstart == -999) tstart = tl->images[inow].camera_time;

		/* Create the image pathname */
		sprintf_s(pathname, sizeof(pathname), "%s_%3.3d.%s", pattern, i, extension);

		/* Put an entry in the logfile */
		fprintf(funit, "%d,%s,%.4f,%lld,%4.4d.%2.2d.%2.2d %2.2d:%2.2d:%2.2d.%3.3d\n", 
				  i, pathname, tl->images[inow].camera_time-tstart, tl->images[inow].timestamp,
				  tl->images[inow].system_time.wYear, tl->images[inow].system_time.wMonth, tl->images[inow].system_time.wDay, 
				  tl->images[inow].system_time.wHour, tl->images[inow].system_time.wMinute, tl->images[inow].system_time.wSecond, 
				  tl->images[inow].system_time.wMilliseconds);
		
		/* Generate the bitmap file */
		TL_SaveImage(tl, pathname, inow, format);
		inow = (inow+1) % tl->nBuffers;
	}

	/* Close the logfile now */
	if (funit != NULL) fclose(funit);

	return 0;
}


/* ===========================================================================
-- Render an image in a specified window
-- 
-- Usage: int TL_RenderFrame(TL_CAMERA *tl, int frame, HWND hwnd);
--
-- Inputs: camera - an opened TL camera
--         frame  - frame to process from buffers (-1 ==> for most recent)
--         hwnd   - window to render the bitmap to
--
-- Output: converts image to RGB, generates bitmap, and displays in window
--
-- Return: 0 if successful, otherwise an error code
--           1 ==> camera pointer not valid
--           2 ==> path invalid
--           3 ==> no RGB24 image data in camera structure
--           4 ==> unable to allocate memory for new RGB data
--           5 ==> unable to get the mutex semaphore
=========================================================================== */
int TL_RenderFrame(TL_CAMERA *tl, int frame, HWND hwnd) {
	static char *rname = "TL_RenderFrame";

	HDC hdc;
	BITMAPINFOHEADER *bmih;
	HBITMAP hBitmap;
	HDC       hDCBits;
	BITMAP    Bitmap;
	BOOL      bResult;
	RECT		 Client;
	static sig_atomic_t active = 0;

	if (active != 0 || tl == NULL || ! IsWindow(hwnd)) return 1;							/* Don't even bother trying */
	active++;

	/* If frame <0, implies want most recent image */
	if (frame < 0) frame = tl->iLast;

	/* Get the bitmap to render (will deal with processing to RGB and all semaphores) */
	if ( NULL == (bmih = TL_CreateDIB(tl, frame, NULL))) { active--; return 2; }		/* Unable to create the bitmap */
	tl->iShow = frame;																					/* This frame now in memory */

	hdc = GetDC(hwnd);				/* Get DC */
	SetStretchBltMode(hdc, COLORONCOLOR);

	hBitmap = CreateDIBitmap(hdc, bmih, CBM_INIT, (LPSTR) bmih + bmih->biSize, (BITMAPINFO *) bmih, DIB_RGB_COLORS);
	GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&Bitmap);

	hDCBits = CreateCompatibleDC(hdc);
	SelectObject(hDCBits, hBitmap);

	GetClientRect(hwnd, &Client);
//	bResult = BitBlt(hdc, 0, 0, Bitmap.bmWidth, Bitmap.bmHeight, hDCBits, 0, 0, SRCCOPY);
	bResult = StretchBlt(hdc, 0,0, Client.right, Client.bottom, hDCBits, 0, 0, bmih->biWidth, bmih->biHeight, SRCCOPY);

	DeleteDC(hDCBits);				/* Delete the temporary DC for the bitmap */
	DeleteObject(hBitmap);			/* Delete the bitmap object itself */

	ReleaseDC(hwnd, hdc);			/* Release the main DC */
	free(bmih);							/* Release memory associated with the bitmap itself */

	active--; 
	return 0;
}


/* ===========================================================================
-- Query the current exposure time of the camera
--
-- Usage: double TL_GetExposure(TL_CAMERA *tl, BOOL bForceQuery);
--
-- Inputs: camera      - pointer to valid TL_CAMERA
--         bForceQuery - if TRUE, will query camera directly to update
--                       the value in the TL_CAMERA structure.
--
-- Output: none
--
-- Return: 0 on error; otherwise exposure time in ms
=========================================================================== */
double TL_GetExposure(TL_CAMERA *tl, BOOL bForceQuery) {
	static char *rname = "TL_GetExposure";

	int rc;
	long long us_expose;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 0.0;

	/* If bForceQuery, update directly from camera */
	if (bForceQuery) {
		if ( (rc = tl_camera_get_exposure_time(tl->handle, &us_expose)) != 0) {
			TL_CameraErrMsg(rc, "Failed to get exposure time", rname);
		} else {
			tl->us_expose = us_expose;
			tl->ms_expose = 0.001*us_expose;
		}
	}

	return tl->ms_expose;
}

/* ===========================================================================
-- Get exposure parameters from camera
--
-- Usage: int TL_GetExposureParms(TL_CAMErA *tl, double *ms_min, double *ms_max, double *ms_inc);
--
-- Inputs: camera  - pointer to valid TL_CAMERA structure
--         *ms_min - pointer to get minimum allowed exposure time
--         *ms_max - pointer to get maximum allowed exposure time
--
-- Output: for each non-NULL parameter, gets value
--
-- Return: 0 if successful, 1 if camera structure invalid or no camera active
=========================================================================== */
int TL_GetExposureParms(TL_CAMERA *tl, double *ms_min, double *ms_max) {
	static char *rname = "TL_GetExposureParms";

	/* Load default values */
	if (ms_min != NULL) *ms_min = 0.010;
	if (ms_max != NULL) *ms_max = 1000.0;

	/* Make sure we are alive and the camera is connected (open) */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Return values */
	if (ms_min != NULL) *ms_min = 0.001 * tl->us_expose_min;		/* Go from us to ms */
	if (ms_max != NULL) *ms_max = 0.001 * tl->us_expose_max;
	return 0;
}

/* ===========================================================================
-- Set the exposure time for a camera
--
-- Usage: double TL_SetExposure(TL_CAMERA *tl, double ms_expose);
--
-- Inputs: camera    - an opened TL camera
--         ms_expose - requested exposure in milliseconds
--                     set to 0 or negative to just return current value
--
-- Output: If exposure>0, sets exposure to the closest valid
--
-- Return: Current exposure time in milliseconds or 0 on error
=========================================================================== */
double TL_SetExposure(TL_CAMERA *tl, double ms_expose) {
	static char *rname = "TL_SetExposure";

	int rc;
	long long us_expose;

	/* Make sure we are alive and the camera is connected (open) */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 0.0;

	if (ms_expose > 0.0) {
		us_expose = (int) (1000*ms_expose + 0.5);
		if ( (rc = tl_camera_set_exposure_time(tl->handle, us_expose)) != 0) { 
			TL_CameraErrMsg(rc, "Unable to set exposure time", rname);
		} else {
			tl->us_expose = us_expose;
			tl->ms_expose = 0.001 * us_expose;
		}
	}

	/* Try to verify the value */
	if ( (rc = tl_camera_get_exposure_time(tl->handle, &us_expose)) != 0) {
		TL_CameraErrMsg(rc, "Failed to get exposure time", rname);
	} else {
		tl->us_expose = us_expose;
		tl->ms_expose = 0.001 * us_expose;
	}

	return tl->ms_expose;
}

/* ===========================================================================
-- Sets the frame rate
--
-- Usage: double TL_SetFPSControl(TL_CAMERA *tl, double fps);
--
-- Inputs: camera - an opened TL camera
--         fps    - desired fps rate
--
-- Output: none
--
-- Return: framerate or 0.0 on any error
=========================================================================== */
double TL_SetFPSControl(TL_CAMERA *tl, double fps) {
	static char *rname = "TL_SetFPSControl";
	int rc;
	
	/* Make sure we are alive and the camera is connected (open) */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 0.0;

	/* Query will fail if camera does not support frame rate control */
	if (! tl->bFrameRateControl) return 0.0;

	/* Try to set */
	if ( (rc = tl_camera_set_frame_rate_control_value(tl->handle, fps)) != 0) {
		TL_CameraErrMsg(rc, "Failed to get frame rate control", rname);
	} 

	/* And try to read back value for return, even if unsuccessful */
	if ( (rc = tl_camera_get_frame_rate_control_value(tl->handle, &fps)) != 0) {
		TL_CameraErrMsg(rc, "Failed to get frame rate control", rname);
		fps = 0.0;
	}

	/* Return best guess of value */
	return fps;
}


/* ===========================================================================
-- Query the set frame rate and the estimate actual rate
--
-- Usage: double TL_GetFPSControl(TL_CAMERA *tl);
--
-- Inputs: camera - an opened TL camera
--
-- Output: none
--
-- Return: Returns -1 if camera does not support frame rate control.
=========================================================================== */
double TL_GetFPSControl(TL_CAMERA *tl) {
	static char *rname = "TL_GetFPSControl";

	int rc;
	double rval;

	/* Make sure we are alive and the camera is connected (open) */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 0.0;

	/* Query will fail if camera does not support frame rate control */
	if (! tl->bFrameRateControl) {
		rval = -1.0;
	} else if ( (rc = tl_camera_get_frame_rate_control_value(tl->handle, &rval)) != 0) {
		TL_CameraErrMsg(rc, "Failed to get frame rate control", rname);
		rval = -1.0;
	}
	return rval;
}

/* ===========================================================================
-- Query estimated frame rate based on image acquisition timestamps
--
-- Usage: double TL_GetFPSActual(TL_CAMERA *tl);
--
-- Inputs: camera - an opened TL camera
--
-- Output: none
--
-- Return: Measured frame rate from the camera
=========================================================================== */
double TL_GetFPSActual(TL_CAMERA *tl) {
	static char *rname = "TL_GetFPSActual";

	int rc;
	double rval;

	/* Make sure we are alive and the camera is connected (open) */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 0.0;

	if ( (rc = tl_camera_get_measured_frame_rate(tl->handle, &rval)) != 0) {
		TL_CameraErrMsg(rc, "Failed to get frame rate", rname);
		rval = -1.0;
	}

	return rval;
}

/* ===========================================================================
-- Set the master gain for the camera (in dB)
--
-- Usage: int TL_SetMasterGain(TL_CAMERA *tl, double dB_gain);
--
-- Inputs: camera      - pointer to valid TL_CAMERA
--         db_Gain     - desired gain in dB
--
-- Output: sets camera within range
--
-- Return: 0 on success; otherwise
--          1 ==> bad camera structure
--          2 ==> camera does not support gain
--          3 ==> failure setting the value
=========================================================================== */
int TL_SetMasterGain(TL_CAMERA *tl, double dB_gain) {
	static char *rname = "TL_SetMasterGain";

	int rc, gain_index;
	
	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Must be able to control gain */
	if (! tl->bGainControl) return 2;
	
	if (dB_gain < tl->db_min) dB_gain = tl->db_min;
	if (dB_gain > tl->db_max) dB_gain = tl->db_max;

	if ( (rc = tl_camera_convert_decibels_to_gain(tl->handle, dB_gain, &gain_index)) != 0) { 
		TL_CameraErrMsg(rc, "Unable to convert gain dB", rname);
		return 3;
	} else if ( (rc = tl_camera_set_gain(tl->handle, gain_index)) != 0) {
		TL_CameraErrMsg(rc, "Unable to set gain to index determined", rname);
		return 3;
	}

	/* Try to verify that we were successful */
	if ( (rc = tl_camera_get_gain(tl->handle, &gain_index)) != 0) {
		TL_CameraErrMsg(rc, "Unable to read gain", rname);
	} else if ( (rc = tl_camera_convert_gain_to_decibels(tl->handle, gain_index, &dB_gain)) != 0) {
		TL_CameraErrMsg(rc, "Unable to convert gain index to dB", rname);
	} else {
		tl->dB_gain = dB_gain;									/* Only when fully successful */
	}

	return 0;
}

/* ===========================================================================
-- Query the master gain for the camera (in dB)
--
-- Usage: double TL_GetMasterGain(TL_CAMERA *tl);
--
-- Inputs: camera      - pointer to valid TL_CAMERA
--
-- Output: none
--
-- Return: Gain read back in dB or a negative value on error
--          -1 ==> bad camera structure
--          -2 ==> camera does not support gain
=========================================================================== */
int TL_GetMasterGain(TL_CAMERA *tl, double *db) {
	static char *rname = "TL_GetMasterGain";

	int rc, gain_index;
	double dB_gain;

	/* Default return values */
	if (db != NULL) *db = 0;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;
	if (! tl->bGainControl) return 2;

	if ( (rc = tl_camera_get_gain(tl->handle, &gain_index)) != 0) {
		TL_CameraErrMsg(rc, "Unable to read gain", rname);
		return 3;
	} else if ( (rc = tl_camera_convert_gain_to_decibels(tl->handle, gain_index, &dB_gain)) != 0) {
		TL_CameraErrMsg(rc, "Unable to convert gain index to dB", rname);
		return 3;
	} else {
		tl->dB_gain = dB_gain;											/* Only when fully successful */
	}

	if (db != NULL) *db = dB_gain;									/* Return value */
	return 0;
}

/* ===========================================================================
-- Query the master gain range for the camera (in dB)
--
-- Usage: int TL_GetMasterGainInfo(TL_CAMERA *tl, BOOL *bGain, double *db_dflt, double *db_min, double *db_max);
--
-- Inputs: camera  - pointer to valid TL_CAMERA
--         bGain   - pointer for flag whether the camera implements gain
--         db_dflt - pointer for default gain setting (dB)
--         db_min  - pointer for lower limit for gain settings (dB)
--         db_max  - pointer for upper limit for gain settings (dB)
--
-- Output: *bGain   - if ! NULL, flag set to true if camera implements master gain
--         *db_dflt - default gain value (on camera initialization)
--         *db_min  - minimum gain setting (0 if no gain capability)
--         *db_max  - maximum gain setting (6 if no gain capability)
--
-- Return: 0 if successful
--           1 ==> bad camera structure
--           2 ==> camera does not support gain
=========================================================================== */
int TL_GetMasterGainInfo(TL_CAMERA *tl, BOOL *bGain, double *db_dflt, double *db_min, double *db_max) {
	static char *rname = "TL_GetMasterGainRange";

	/* Default return values */
	if (bGain   != NULL) *bGain   = FALSE;
	if (db_dflt != NULL) *db_dflt = 0;
	if (db_min  != NULL) *db_min  = 0;
	if (db_max  != NULL) *db_max  = 6;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Otherwise, copy values to return */
	if (bGain != NULL) *bGain = tl->bGainControl;
	if (tl->bGainControl) {
		if (db_dflt != NULL) *db_dflt = tl->db_dflt;
		if (db_min  != NULL) *db_min  = tl->db_min;
		if (db_max  != NULL) *db_max  = tl->db_max;
	}
	return 0;
}


/* ===========================================================================
-- Query the RGB channel gains
--
-- Usage: double TL_GetRGBGains(TL_CAMERA *tl, double *red, double *green, double *blue);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         red, green, blue - pointers to variable to receive gain values
--
-- Output: none
--
-- Return: 0 if successful, otherwise
--           1 ==> bad camera structure
--           0x02, 0x04, 0x08 - bitwise failure to get specific channels
=========================================================================== */
int TL_GetRGBGains(TL_CAMERA *tl, double *red, double *green, double *blue) {
	static char *rname = "TL_GetRGBGains";

	int rc, rcode;
	float R, G, B;

	/* Default return values */
	if (red   != NULL) *red   = 0;
	if (green != NULL) *green = 0;
	if (blue  != NULL) *blue  = 0;
	
	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	rcode = 0;
	if ( (rc = tl_mono_to_color_get_red_gain(tl->color_processor, &R)) != 0) {
		TL_CameraErrMsg(rc, "Unable to query red gain", rname);
		rcode |= 0x02;
	}

	if ( (rc = tl_mono_to_color_get_green_gain(tl->color_processor, &G)) != 0) {
		TL_CameraErrMsg(rc, "Unable to query green gain", rname);
		rcode |= 0x04;
	}

	if ( (rc = tl_mono_to_color_get_blue_gain(tl->color_processor, &B)) != 0) {
		TL_CameraErrMsg(rc, "Unable to query blue gain", rname);
		rcode |= 0x08;
	}

	if (red   != NULL) *red   = R;
	if (green != NULL) *green = G;
	if (blue  != NULL) *blue  = B;

	return rcode;
}

/* ===========================================================================
-- Query the default RGB channel gains (original values)
--
-- Usage: double TL_GetDfltRGBGains(TL_CAMERA *tl, double *red, double *green, double *blue);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         red, green, blue - pointers to variable to receive gain values
--
-- Output: none
--
-- Return: 0 success
--          -1 ==> bad camera structure
--          -2 ==> camera does not support gain
=========================================================================== */
int TL_GetDfltRGBGains(TL_CAMERA *tl, double *red, double *green, double *blue) {
	static char *rname = "TL_DfltGetRGBGains";

	/* Default return value */
	if (red   != NULL) *red   = 0;
	if (green != NULL) *green = 0;
	if (blue  != NULL) *blue  = 0;

	/* Verify we can report a value */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* All okay, so report initial value when we started */
	if (red   != NULL) *red   = tl->red_dflt;
	if (green != NULL) *green = tl->green_dflt;
	if (blue  != NULL) *blue  = tl->blue_dflt;

	return 0;
}


/* ===========================================================================
-- Set the RGB channel gains
--
-- Usage: double TL_SetRGBGains(TL_CAMERA *tl, double red, double green, double blue);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         red, green, blue - gain values
--            if TL_IGNORE_GAIN is used, the channel gain will be left unchanged
--
-- Output: sets gains within the mono_to_color_processor
--
-- Return: 0 if successful, otherwise
--           -1 ==> bad camera structure
--           !0 ==> bitwise collection of which sets failed
=========================================================================== */
int TL_SetRGBGains(TL_CAMERA *tl, double red, double green, double blue) {
	static char *rname = "TL_SetRGBGains";

	int rc, rcode;
	
	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return -1;

	rcode = 0;
	if (red != TL_IGNORE_GAIN) {
		if ( (rc = tl_mono_to_color_set_red_gain(tl->color_processor, (float) red)) != 0) {
			TL_CameraErrMsg(rc, "Unable to set red gain", rname);
			rcode |= 0x02;
		}
	}

	if (green != TL_IGNORE_GAIN) {
		if ( (rc = tl_mono_to_color_set_green_gain(tl->color_processor, (float) green)) != 0) {
			TL_CameraErrMsg(rc, "Unable to set green gain", rname);
			rcode |= 0x04;
		}
	}

	if (blue != TL_IGNORE_GAIN) {
		if ( (rc = tl_mono_to_color_set_blue_gain(tl->color_processor, (float) blue)) != 0) {
			TL_CameraErrMsg(rc, "Unable to set blue gain", rname);
			rcode |= 0x02;
		}
	}

	return rcode;
}

/* ===========================================================================
-- Software arm/disarm camera (pending triggers)
--
-- Usage: TRIG_ARM_ACTION TL_Arm(TL_CAMERA *tl, TRIG_ARM_ACTION action);
--
-- Inputs: tl - structure associated with a camera
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
TRIG_ARM_ACTION TL_Arm(TL_CAMERA *tl, TRIG_ARM_ACTION action) {
	static char *rname = "TL_Arm";
	int rc;

	/* Verify structure and arm */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return TRIG_ARM_UNKNOWN;
	
	switch (action) {
		case TRIG_ARM:
			if ( (rc = tl_camera_arm(tl->handle, 2)) != 0) {
				TL_CameraErrMsg(rc, "Arm failed", rname);
				return TRIG_ARM_UNKNOWN;
			}
			tl->trigger.bArmed = TRUE;
			if (tl->trigger.mode == TRIG_FREERUN) {				/* Arming start freerun mode simultaneously */
				if (tl_camera_issue_software_trigger(tl->handle) != 0) TL_CameraErrMsg(rc, "Failed to trigger camera", rname);
				tl->nValid = tl->iLast = tl->iShow = 0;			/* Counting should reset to zero again */
			}
			break;
		case TRIG_DISARM:
			if ( (rc = tl_camera_disarm(tl->handle)) != 0) {
				TL_CameraErrMsg(rc, "Disarm failed", rname);
				return TRIG_ARM_UNKNOWN;
			}
			tl->trigger.bArmed = FALSE;
			break;
		default:
			break;
	}

	return tl->trigger.bArmed ? TRIG_ARM : TRIG_DISARM ;
}


/* ===========================================================================
-- Software trigger of camera
--
-- Usage: int TL_Trigger(TL_CAMERA *tl);
--
-- Inputs: camera - structure associated with a camera
--
-- Output: Triggers camera immediately if in an armed state
--
-- Return: 0 if successful
--           1 ==> dcx or dcx->hCam invalid
--           3 ==> not enabled
--           other ==> return from tl_camera_issue_software_trigger() command
--
-- Notes: While valid for all trigger modes, primarily intended for TRIG_BURST 
--        and TRIG_SOFTWARE.  In TRIG_FREERUN, applies if images were
--        halted via Trigger_Disarm(), and then rearmed via Trigger_Arm().
=========================================================================== */
int TL_Trigger(TL_CAMERA *tl) {
	static char *rname = "TL_Trigger";
	int rc;

	/* Verify structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	if (! tl->trigger.bArmed) {
		Beep(300,200);
		rc = 3;
	} else {
		rc = tl_camera_issue_software_trigger(tl->handle);
		if (rc != 0) TL_CameraErrMsg(rc, "Software trigger failed", rname);
	}

	return rc;
}


/* ===========================================================================
-- Set/Query the triggering mode for the camera
--
-- Usage: TRIGGER_MODE TL_SetTriggerMode(TL_CAMERA *tl, TRIGGER_MODE mode, TRIGGER_INFO *info);
--        TRIGGER_MODE TL_GetTriggerMode(TL_CAMERA *tl, TRIGGER_INFO *info);
--
-- Inputs: tl     - structure associated with a camera
--         mode   - one of the allowed triggering modes
--                  TRIG_SOFTWARE, TRIG_FREERUN, TRIG_EXTERNAL, TRIG_SS or TRIG_BURST
--         info   - if !NULL, details on triggering; if NULL, will use defaults 
--                  what is used and valid depends on the mode ... look at code 
--
-- Output: Sets camera triggering mode
--
-- Return: Mode in camera or -1 on error
=========================================================================== */
TRIGGER_MODE TL_GetTriggerMode(TL_CAMERA *tl, TRIGGER_INFO *info) {
	static char *rname = "TL_GetTriggerMode";

	if (info != NULL) *info = tl->trigger;

	return tl->trigger.mode;
}

TRIGGER_MODE TL_SetTriggerMode(TL_CAMERA *tl, TRIGGER_MODE mode, TRIGGER_INFO *info) {
	static char *rname = "TL_SetTriggerMode";

	int rc;

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	/* Handle potential changes to parameters in the new info first */
	if (info != NULL) {

		/* Change to number of frames per trigger ... because of structure, 0 can't be set here */
		if (info->frames_per_trigger > 0 && info->frames_per_trigger != tl->trigger.frames_per_trigger) {
			tl->trigger.frames_per_trigger = info->frames_per_trigger;
			if ( (mode == tl->trigger.mode) && (mode == TRIG_EXTERNAL || mode == TRIG_SS || mode == TRIG_SOFTWARE)) {
				if (tl->trigger.bArmed && (rc = tl_camera_disarm(tl->handle)) != 0) TL_CameraErrMsg(rc, "Failed to disarm", rname);
				if ( (rc = tl_camera_set_frames_per_trigger_zero_for_unlimited(tl->handle, tl->trigger.frames_per_trigger)) != 0) 
					TL_CameraErrMsg(rc, "Failed to set frames per trigger", rname);
				if (tl->trigger.bArmed && (rc = tl_camera_arm(tl->handle, 2)) != 0) TL_CameraErrMsg(rc, "Failed to re-arm", rname);
			}
		}

		/* Change to external trigger slope */
		if ( (info->ext_slope == TRIG_EXT_POS || info->ext_slope == TRIG_EXT_NEG) && (info->ext_slope != tl->trigger.ext_slope)) {
			tl->trigger.ext_slope = info->ext_slope;
			if (mode == tl->trigger.mode && (mode == TRIG_EXTERNAL || mode == TRIG_SS)) {
				if (tl->trigger.bArmed && (rc = tl_camera_disarm(tl->handle)) != 0) TL_CameraErrMsg(rc, "Failed to disarm", rname);
				if ( (rc = tl_camera_set_trigger_polarity(tl->handle, (tl->trigger.ext_slope == TRIG_EXT_POS) ? TL_CAMERA_TRIGGER_POLARITY_ACTIVE_HIGH : TL_CAMERA_TRIGGER_POLARITY_ACTIVE_LOW)) != 0)
					TL_CameraErrMsg(rc, "Failed to set edge triggering mode", rname);
				if (tl->trigger.bArmed && (rc = tl_camera_arm(tl->handle, 2)) != 0) TL_CameraErrMsg(rc, "Failed to re-arm", rname);
			}
		}
	}

	/* At this point, if mode hasn't changed there nothing left to do */
	if (mode == tl->trigger.mode) return mode;

	/* Step 1 ... disarm camera so can change mode */
	if ( (rc = tl_camera_disarm(tl->handle)) != 0) TL_CameraErrMsg(rc, "Failed to disarm", rname);
	tl->trigger.bArmed = FALSE;

	/* We may need to modify parameters so can't assume no-operation for same mode */
	switch (mode) {
		/* Infinite frames, arm, software trigger */
		case TRIG_FREERUN:
		case TRIG_BURST:										/* Almost identical */
			if ( (rc = tl_camera_set_operation_mode(tl->handle, TL_CAMERA_OPERATION_MODE_SOFTWARE_TRIGGERED)) != 0) 
				TL_CameraErrMsg(rc, "Failed to shift DCx to a software trigger mode", rname);
			if ( (rc = tl_camera_set_frames_per_trigger_zero_for_unlimited(tl->handle, 0)) != 0) 
				TL_CameraErrMsg(rc, "Failed to set frames per trigger", rname);
			if (mode == TRIG_FREERUN) {
				if (tl_camera_arm(tl->handle, 2) != 0)						 TL_CameraErrMsg(rc, "Failed to arm camera", rname);
				if (tl_camera_issue_software_trigger(tl->handle) != 0) TL_CameraErrMsg(rc, "Failed to trigger camera", rname);
				tl->trigger.bArmed = TRUE;
			}
			break;

		/* These are identical as far as setting trigger mode is concerned ... handle with image capture */
		case TRIG_EXTERNAL:
		case TRIG_SS:
			if (tl->trigger.ext_slope != TRIG_EXT_POS && tl->trigger.ext_slope != TRIG_EXT_NEG) tl->trigger.ext_slope = TRIG_EXT_POS;
			if (tl->trigger.frames_per_trigger <= 0) tl->trigger.frames_per_trigger = 1;

			if ( (rc = tl_camera_set_operation_mode(tl->handle, TL_CAMERA_OPERATION_MODE_HARDWARE_TRIGGERED)) != 0)
				TL_CameraErrMsg(rc, "Failed to set hardware triggering mode", rname);
			if ( (rc = tl_camera_set_frames_per_trigger_zero_for_unlimited(tl->handle, tl->trigger.frames_per_trigger)) != 0) 
				TL_CameraErrMsg(rc, "Failed to set frames per trigger", rname);
			if ( (rc = tl_camera_set_trigger_polarity(tl->handle, (tl->trigger.ext_slope == TRIG_EXT_POS) ? TL_CAMERA_TRIGGER_POLARITY_ACTIVE_HIGH : TL_CAMERA_TRIGGER_POLARITY_ACTIVE_LOW)) != 0)
				TL_CameraErrMsg(rc, "Failed to set edge triggering mode", rname);
			if ( (rc = tl_camera_arm(tl->handle, 2)) != 0)
				TL_CameraErrMsg(rc, "Unable to arm camera", rname);
			tl->trigger.bArmed = TRUE;
			break;

		/* Set to single frame via software trigger and arm (also default) */
		case TRIG_SOFTWARE:
		default:
			mode = TRIG_SOFTWARE;			/* Default will be TRIG_SOFTWARE if invalid value passed */
			if (tl->trigger.frames_per_trigger <= 0) tl->trigger.frames_per_trigger = 1;

			if ( (rc = tl_camera_set_operation_mode(tl->handle, TL_CAMERA_OPERATION_MODE_SOFTWARE_TRIGGERED)) != 0)
				TL_CameraErrMsg(rc, "Failed to set software triggering mode", rname);
			if ( (rc = tl_camera_set_frames_per_trigger_zero_for_unlimited(tl->handle, tl->trigger.frames_per_trigger)) != 0) 
				TL_CameraErrMsg(rc, "Failed to set frames per trigger", rname);
			if ( (rc = tl_camera_arm(tl->handle, 2)) != 0)
				TL_CameraErrMsg(rc, "Unable to arm camera", rname);
			tl->trigger.bArmed = TRUE;
			break;
	}

	/* Record the settings and reset all counters */
	tl->trigger.mode = mode;

	/* Return current mode */
	return tl->trigger.mode;
}

/* ===========================================================================
-- Set number of frames per trigger (allow 0 for infinite)
--
-- Usage: int TL_SetFramesPerTrigger(TL_CAMERA *tl, int frames);
--        int TL_GetFramesPerTrigger(TL_CAMERA *tl);
--
-- Inputs: tl     - structure associated with a camera
--         frames - # of frames per trigger, or 0 for infinite
--
-- Output: Sets camera triggering count
--
-- Return: 0 if successful, error from calls otherwise
--
-- Note: Value set internally always, but only passed to camera in 
--       TRIG_SOFTWARE, TRIG_EXTERNAL, and TRIG_SS modes.
=========================================================================== */
int TL_GetFramesPerTrigger(TL_CAMERA *tl) {

	/* Validate call */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return -1;

	return tl->trigger.frames_per_trigger;
}

int TL_SetFramesPerTrigger(TL_CAMERA *tl, int frames) {
	static char *rname = "TL_SetFramesPerTrigger";
	int rc;

	/* Validate call */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return -1;

	/* Validate the parameter and save */
	if (frames < 0) frames = 0;
	tl->trigger.frames_per_trigger = frames;

	/* Only actually send if in EXT or SOFTWARE modes ... need to deal with potentially armed */
	/* Disable first if armed, and then re-enable after change to the parameter */
	if (tl->trigger.mode == TRIG_EXTERNAL || tl->trigger.mode == TRIG_SS || tl->trigger.mode == TRIG_SOFTWARE) {
		if (tl->trigger.bArmed && (rc = tl_camera_disarm(tl->handle)) != 0) {
			TL_CameraErrMsg(rc, "Failed to disarm", rname);
		}
		if ( (rc = tl_camera_set_frames_per_trigger_zero_for_unlimited(tl->handle, frames)) != 0) {
			TL_CameraErrMsg(rc, "Failed to set frames per trigger", rname);
		}
		if (tl->trigger.bArmed && (rc = tl_camera_arm(tl->handle, 2)) != 0) {
			TL_CameraErrMsg(rc, "Failed to re-arm", rname);
		}
	}

	return 0;
}


/* ===========================================================================
-- Query the current ring buffer values
--
-- Usage: int TL_GetRingInfo(TL_CAMERA *tl, int *nBuffers, int *nValid, int *iLast, int *iShow);
--
-- Inputs: tl       - structure associated with a camera
--         nBuffers - pointer for # of buffers in the ring
--			  nValid   - pointer for # of buffers in the ring current with valid images
--			  iLast    - pointer for index of buffer with last image from the camera
--			  iShow    - pointer for index of buffer currently being shown (rgb values)
--
-- Output: For all parameter !NULL, copies appropriate value from internals
--
-- Return: 0 if successful, 1 if tl invalid
=========================================================================== */
int TL_GetRingInfo(TL_CAMERA *tl, int *nBuffers, int *nValid, int *iLast, int *iShow) {
	static char *rname = "TL_GetRingInfo";
	BOOL valid;

	/* Is structure valid ... return default values or values from the structure */
	valid = tl != NULL && tl->magic == TL_CAMERA_MAGIC;

	if (nBuffers != NULL) *nBuffers = valid ? tl->nBuffers : 1 ;
	if (nValid   != NULL) *nValid   = valid ? tl->nValid   : 0 ;
	if (iLast    != NULL) *iLast    = valid ? tl->iLast    : 0 ;
	if (iShow    != NULL) *iShow    = valid ? tl->iShow    : 0 ;

	return valid ? 0 : 1 ;
}

/* ===========================================================================
-- Query/set name associated with the camera
--
-- Usage: int TL_GetCameraName(TL_CAMERA *tl, char *name, size_t length);
--        int TL_SetCameraName(TL_CAMERA *tl, const char *name);
--
-- Inputs: tl     - structure associated with a camera
--         name   - name to be associated with the camera
--         length - space available in the name variable (get)
--
-- Output: sets camera name, or retrieves to *name
--
-- Return: 0 if successful, 1 if tl invalid
=========================================================================== */
int TL_GetCameraName(TL_CAMERA *tl, char *name, size_t length) {
	static char *rname = "TL_GetCameraName";

	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	return tl_camera_get_name(tl->handle, name, (int) length);
}


int TL_SetCameraName(TL_CAMERA *tl, char *name) {
	static char *rname = "TL_SetCameraName";
	
	/* Must be valid structure */
	if (tl == NULL || tl->magic != TL_CAMERA_MAGIC) return 1;

	return tl_camera_set_name(tl->handle, name);
}


/* ===========================================================================
-- Print standard error message for tl_camera error message
--
-- Usage: int TL_CameraErrMsg(int rc, char *msg, char *rname);
--
-- Inputs: rc    - return code from call ... only print message if rc != 0
--         msg   - brief text to be included with the error message
--         rname - name of the function calling this message
--
-- Output: Sends message to stderr and flushes the buffer.  Includes the
--         API standard message from tl_camera_get_last_error()
--
-- Return: rc value passed to the routing
=========================================================================== */
static int TL_CameraErrMsg(int rc, char *msg, char *rname) {
	if (rc != 0) {
		fprintf(stderr, "[%s] %s [rc:%d %s]\n", rname, msg, rc, tl_camera_get_last_error());
		fflush(stderr);
	}

	return rc;
}




#if 0
/* ===========================================================================
-- Save data from TL camera (tests)
-- 
-- Usage: SaveData(TL_CAMERA *camera);
--
-- Inputs: camera - an opened TL camera
--
-- Output: varies
--
-- Return: none
=========================================================================== */
void SaveData(TL_CAMERA *camera) {
	static HIRES_TIMER *timer = NULL;
	double start;

	if (timer == NULL) timer = HiResTimerCreate();

	TL_ProcessRawSeparation(camera, -1);			/* Make sure these are valid */
	TL_ProcessRGB(camera, -1);

	if (camera->frame_count < 0) {
		start = HiResTimerDelta(timer);

#if 0
		funit = fopen("raw.dat",   "w"); fwrite(camera->raw,   1, camera->nbytes_raw,   funit); fclose(funit);
		funit = fopen("red.dat",   "w"); fwrite(camera->red,   1, camera->nbytes_red,   funit); fclose(funit);
		funit = fopen("green.dat", "w"); fwrite(camera->green, 1, camera->nbytes_green, funit); fclose(funit);
		funit = fopen("red.dat",   "w"); fwrite(camera->red,   1, camera->nbytes_blue,  funit); fclose(funit);
#endif

#if 0
		funit = fopen("red.dat", "w");
		for (i=0; i<camera->height/2; i++) {
			for (j=0; j<camera->width/2; j++) fprintf(funit, " %d", camera->red[(i*camera->width/2+j)]);
			fprintf(funit, "\n");
		}
		fclose(funit);

		funit = fopen("blue.dat", "w");
		for (i=0; i<camera->height/2; i++) {
			for (j=0; j<camera->width/2; j++) fprintf(funit, " %d", camera->blue[(i*camera->width/2+j)]);
			fprintf(funit, "\n");
		}
		fclose(funit);

		funit = fopen("green.dat", "w");
		for (i=0; i<camera->height; i++) {
			for (j=0; j<camera->width/2; j++) fprintf(funit, " %d", camera->green[(i*camera->width/2+j)]);
			fprintf(funit, "\n");
		}
		fclose(funit);
#endif
//		fprintf(stderr, "\n Write time required %.2f ms\n", 1000.0*(HiResTimerDelta(timer)-start)); fflush(stderr);

#if 0
		funit = fopen("red_rgb.dat", "w");
		for (i=0; i<camera->height; i++) {
			for (j=0; j<camera->width; j++) fprintf(funit, " %d", camera->rgb32[4*(i*camera->width+j)+0]);
			fprintf(funit, "\n");
		}
		fclose(funit);
		funit = fopen("green_rgb.dat", "w");
		for (i=0; i<camera->height; i++) {
			for (j=0; j<camera->width; j++) fprintf(funit, " %d", camera->rgb32[4*(i*camera->width+j)+1]);
			fprintf(funit, "\n");
		}
		fclose(funit);
		funit = fopen("blue_rgb.dat", "w");
		for (i=0; i<camera->height; i++) {
			for (j=0; j<camera->width; j++) fprintf(funit, " %d", camera->rgb32[4*(i*camera->width+j)+2]);
			fprintf(funit, "\n");
		}
		fclose(funit);
#endif
	}
	return;
}

#endif
