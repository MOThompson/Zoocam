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

	int rc;											/* return code */

	/* Allow multiple calls as NOP */
	if (TL_is_initialized) return 0;

	/* Clear the camera list and count */
	memset(tl_camera_list, 0, TL_MAX_CAMERAS*sizeof(*tl_camera_list));
	tl_camera_count = 0;

	/* Assume success */
	rc = 0;

	/* Build the DLL link points for the sdk (painful) */
	is_camera_dll_open = tl_camera_sdk_dll_initialize() == 0;
	if (! is_camera_dll_open) { 
		fprintf(stderr, "%s: Failed to initialize the TL camera DLLs\n", rname); fflush(stderr);
		rc |= 0x01;
	}

	/* Open the SDK */
	is_camera_sdk_open = tl_camera_open_sdk() == 0;
	if (! is_camera_sdk_open) { 
		fprintf(stderr, "%s: Failed to open the tl camera SDK\n", rname); fflush(stderr);
		rc |= 0x02;
	}

	/* Open the SDK for the mono to color processing (painful) */
	is_mono_to_color_sdk_open = tl_mono_to_color_processing_initialize() == 0;
	if (! is_mono_to_color_sdk_open) { 
		fprintf(stderr, "%s: Failed to initialize mono to color processing sdk\n", rname); fflush(stderr);
		rc |= 0x04;
	}

	/* Register the camera connect/disconnect event callbacks. */
	if (tl_camera_set_camera_connect_callback(camera_connect_callback, 0) != 0)       { fprintf(stderr, "%s: Unable to register camera connect callback: %s\n", rname, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_set_camera_disconnect_callback(camera_disconnect_callback, 0) != 0) { fprintf(stderr, "%s: Unable to register camera disconnect callback: %s\n", rname, tl_camera_get_last_error()); fflush(stderr); }

	/* Mark that we have initialized and are open for business */
	TL_is_initialized = TRUE;
	return rc;
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
	TL_CAMERA *camera;

	/* Initialize the return values */
	if (plist != NULL) *plist = NULL;

	/* Initialize now if not done before */
	if ( TL_Initialize() != 0) {
		fprintf(stderr, "[%s:] TL not (and cannot be) initialized\n", rname); fflush(stderr);
		return -1;
	}

	/* Query the list of known camera ID's */
	*szBuf = 0;
	if (tl_camera_discover_available_cameras(szBuf, sizeof(szBuf)) != 0) { 
		fprintf(stderr, "[%s:] Call to discover cameras failed\n", rname); fflush(stderr);
		return 0;
	} else if (*szBuf == '\0') {
		fprintf(stderr, "[%s:] No cameras identified\n", rname); fflush(stderr);
		return 0;
	} 
	fprintf(stderr, "[%s:] TL Cameras (full list): %s\n", rname, szBuf);  fflush(stderr);
	szBuf[strlen(szBuf)] = '\0';				/* Double NULL terminate */

	/* Code maintains list of open cameras and will not double open */
	for (aptr=szBuf; *aptr!='\0'; aptr+=strlen(aptr)) {
		if ( (bptr = strchr(aptr, ' ')) != NULL) *bptr = '\0';
		if ( (camera = TL_FindCamera(aptr, &rc)) == NULL) {
			fprintf(stderr, "[%s:] Failed to open camera with ID %s (rc = %d)\n", rname, aptr, rc);
			fflush(stderr);
		} else {
			fprintf(stderr, "Camera %s:   handle: 0x%p\n", camera->ID, camera->handle);
			fprintf(stderr, "   Model: %s\n", camera->model);
			fprintf(stderr, "   Serial Number: %s\n", camera->serial);
			fprintf(stderr, "   Bit depth: %d\n", camera->bit_depth);
			fprintf(stderr, "   Size %d x %d\n", camera->width, camera->height); 
			fprintf(stderr, "   Exposure (us):  %lld (%lld < exp < %lld)\n", camera->us_expose, camera->us_expose_min, camera->us_expose_max);
			fprintf(stderr, "   Framerate: %.2f < fps < %.2f\n", camera->fps_min, camera->fps_max);
			fprintf(stderr, "   Pixel size (um): %.3f x %.3f\n", camera->pixel_width_um, camera->pixel_height_um);
			fprintf(stderr, "   Bytes per pixel: %d\n", camera->pixel_bytes);
			fprintf(stderr, "   Clock rate (Hz): %d\n", camera->clock_Hz);
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
-- Usage: TL_CAMERA *TL_FindCamera(char *ID, int *rc);
--
-- Inputs: ID - character string ID of camera (serial number) - generally from camera enumeration
--         rc - pointer to variable receive error codes (or NULL if unneeded)
--
-- Output: Creates basic structures and parameters for a TL camera
--         *rc - if rc != NULL, error code with 0 if successful
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
--				camera->handle
--				camera->color_processor
--				camera->image_mutex
--				camera->raw,red,green,blue,rgb24
=========================================================================== */
TL_CAMERA *TL_FindCamera(char *ID, int *rc) {
	static char *rname = "TL_FindCamera";

	TL_CAMERA *camera;
	FILE *handle;
	int myrc, ilow, ihigh;

	/* Just have something where *rc works */
	if (rc == NULL) rc = &myrc;

	/* Assume we will be successful */
	*rc = 0;										

	/* First make sure we don't already have it opened */
	if ( (camera = TL_FindCameraByID(ID)) != NULL) return camera;

	/* Make sure there is an open slot for the camera */
	if (tl_camera_count >= TL_MAX_CAMERAS) {
		fprintf(stderr, "Error opening camera %s: Too many cameras already open\n", ID); fflush(stderr);
		*rc = 1;
		return NULL;
	}

	/* Open the camera, get handle, and make sure all seems okay */
	if (tl_camera_open_camera(ID, &handle) != 0) {
		fprintf(stderr, "Error opening camera %s: %s\n", ID, tl_camera_get_last_error()); fflush(stderr);
		*rc = 2;
		return NULL;
	}

	/* Create a structure for the camera now */
	camera = calloc(1, sizeof(*camera));
	camera->magic = TL_CAMERA_MAGIC;
	strcpy_s(camera->ID, sizeof(camera->ID), ID);
	camera->handle = handle;

	/* And get lots of information about the camera */
	if (tl_camera_get_model                       (handle,  camera->model, sizeof(camera->model))       != 0) { fprintf(stderr, "Error determining model for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_serial_number               (handle,  camera->serial, sizeof(camera->serial))     != 0) { fprintf(stderr, "Error determining serial number for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_name								 (handle,  camera->name, sizeof(camera->name))         != 0) { fprintf(stderr, "Error determining name for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_firmware_version				 (handle,  camera->firmware, sizeof(camera->firmware)) != 0) { fprintf(stderr, "Error determining firmware for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }

	if (tl_camera_get_camera_sensor_type          (handle, &camera->sensor_type)      != 0) { fprintf(stderr, "Error determining sensor type for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_color_filter_array_phase    (handle, &camera->color_filter)     != 0) { fprintf(stderr, "Error determining color filter array for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_color_correction_matrix     (handle,  camera->color_correction) != 0) { fprintf(stderr, "Error determining color correction for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_default_white_balance_matrix(handle,  camera->white_balance)    != 0) { fprintf(stderr, "Error determining white balance for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }

	if (tl_camera_get_image_width                 (handle, &camera->width)            != 0) { fprintf(stderr, "Unable to get image width: %s\n", tl_camera_get_last_error()); }
	if (tl_camera_get_image_height                (handle, &camera->height)           != 0) { fprintf(stderr, "Unable to get image height: %s\n", tl_camera_get_last_error()); }
	if (tl_camera_get_bit_depth                   (handle, &camera->bit_depth)        != 0) { fprintf(stderr, "Error determining bit depth for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }

	if (tl_camera_get_sensor_pixel_height         (handle, &camera->pixel_height_um)  != 0) { fprintf(stderr, "Error determining pixel height for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_sensor_pixel_width          (handle, &camera->pixel_width_um)   != 0) { fprintf(stderr, "Error determining pixel width for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_sensor_pixel_size_bytes     (handle, &camera->pixel_bytes)      != 0) { fprintf(stderr, "Error determining pixel bytes for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }

	if (tl_camera_get_exposure_time_range         (handle, &camera->us_expose_min, &camera->us_expose_max) != 0) { fprintf(stderr, "Error determining min/max exposure time for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	if (tl_camera_get_exposure_time               (handle, &camera->us_expose)      != 0) { fprintf(stderr, "Unable to set exposure time fro camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }

	if (tl_camera_get_timestamp_clock_frequency	 (handle, &camera->clock_Hz)         != 0) { fprintf(stderr, "Unable to get camera clock frequency: %s\n", tl_camera_get_last_error()); }

	if (tl_camera_get_gain_range (handle, &ilow, &ihigh) != 0) { fprintf(stderr, "Unable to get gain range for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); }
	camera->bGainControl = ihigh > 0;		
	camera->db_min = 0.1*ilow; camera->db_max = 0.1*ihigh;

	if (tl_camera_get_frame_rate_control_value_range(handle, &camera->fps_min, &camera->fps_max) != 0) { fprintf(stderr, "Error determining min/max frame rate for camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr); }
	camera->bFrameRateControl = camera->fps_max > 0.0;

	if (tl_camera_close_camera(camera->handle) != 0) {
		fprintf(stderr, "Failed to close camera %s: (%s)\n", camera->ID, tl_camera_get_last_error()); 
		fflush(stderr);
	}
	camera->handle = NULL;

	/* Register in my list of known cameras and return with no error */
	tl_camera_list[tl_camera_count++] = camera;

	/* And return camera (*rc already set at start) */
	return camera;
}


/* ===========================================================================
-- Routine to open a camera for use (active)
--
-- Usage: int TL_OpenCamera(TL_CAMERA *camera, int nBuf);
--
-- Inputs: camera - a partially completed structure from TL_FindCamera()
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
=========================================================================== */
int TL_OpenCamera(TL_CAMERA *camera, int nBuf) {
	static char *rname = "TL_OpenCamera";

	FILE *handle;
	int i, rc;

	/* Verify that the structure is valid and hasn't already been closed */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 0x8001;

	/* Are we already open and initialized? */
	if (camera->handle != NULL) return 0;	

	/* Open the camera first and make sure it exists */
	if (tl_camera_open_camera(camera->ID, &camera->handle) != 0) {
		fprintf(stderr, "Error opening camera %s: %s\n", camera->ID, tl_camera_get_last_error()); fflush(stderr);
		camera->handle = NULL;
		return 0x8002;
	}
	handle = camera->handle;
	
	/* Flash the camera's light to show which one is being initialized */
	tl_camera_set_is_led_on(handle, FALSE); Sleep(100); tl_camera_set_is_led_on(handle, TRUE); 

	/* Open the color image processor if camera is a color ... and set default linear RGB */
	rc = 0;
	if (camera->sensor_type == TL_CAMERA_SENSOR_TYPE_BAYER) {
		if (tl_mono_to_color_create_mono_to_color_processor(camera->sensor_type, camera->color_filter, camera->color_correction, camera->white_balance, camera->bit_depth, &camera->color_processor) != 0) {
			fprintf(stderr, "Failed to create a color to mono processor for camera %s: %s\n", camera->ID, tl_mono_to_color_get_last_error());
			fflush(stderr);
			camera->color_processor = NULL;
			rc |= 0x04;
		} else {
			/* Use TL_MONO_TO_COLOR_SPACE_SRGB if really interested in photographs */
			if (tl_mono_to_color_set_color_space(camera->color_processor, TL_MONO_TO_COLOR_SPACE_LINEAR_SRGB) != 0) { 
				fprintf(stderr, "Failed to set linear RGB: %s\n", tl_mono_to_color_get_last_error());
				fflush(stderr);
				rc |= 0x08;			/* Failed to set linear RGB model */
			}
		}
	}

	/* Simple flag to indicate if we are color or B/W */
	camera->IsSensorColor = camera->sensor_type == TL_CAMERA_SENSOR_TYPE_BAYER;

	/* Allocate the buffers within the raw structure (just offset) */
	camera->nBuffers = max(1, min(TL_MAX_RING_SIZE, nBuf));
	camera->nValid   = camera->iLast = camera->iShow = 0;

	/* Create a semaphore for access to the data itself */
	camera->image_mutex = CreateMutex(NULL, FALSE, NULL);

	/* Allocate space for the raw data and, if color, for the rgb24 image */
	/* Note that this may need to change for B/W cameras */
	camera->npixels = camera->width * camera->height;
	camera->nbytes_raw = sizeof(unsigned short) * camera->npixels;
	camera->raw = calloc(camera->nBuffers, sizeof(*camera->raw));
	camera->timestamp = calloc(camera->nBuffers, sizeof(*camera->timestamp));
	for (i=0; i<camera->nBuffers; i++) {
		if ( (camera->raw[i] = malloc(camera->nbytes_raw)) == NULL) {
			fprintf(stderr, "[%s:] Only able to allocate %d buffers, resetting value\n", rname, i); fflush(stderr);
			camera->nBuffers = i;
			break;
		}
	}

	/* If color sensor, create RGB channels for a separation of a raw frame */
	if (camera->IsSensorColor) {
		camera->nbytes_red   = camera->nbytes_raw / 4;
		camera->nbytes_green = camera->nbytes_raw / 2;
		camera->nbytes_blue  = camera->nbytes_raw / 4;
		camera->red   = malloc(camera->nbytes_red);
		camera->green = malloc(camera->nbytes_green);
		camera->blue  = malloc(camera->nbytes_blue);
	}
	/* If color sensor, create RGB combined buffer */
	if (camera->IsSensorColor) {
		camera->nbytes_rgb24 = 3 * camera->width * camera->height;
		camera->rgb24 = malloc(camera->nbytes_rgb24);
	}

	/* Query the initial gains so could be reset later */
	TL_GetMasterGain(camera, &camera->db_dflt);
	TL_GetRGBGains(camera, &camera->red_dflt, &camera->green_dflt, &camera->blue_dflt);

	/* Register the routine that will process images */
	if (tl_camera_set_frame_available_callback(handle, frame_available_callback, 0) != 0) { 
		fprintf(stderr, "Unable to set frame_available_callback: %s\n", tl_camera_get_last_error());
		fflush(stderr);
		rc |= 0x10;
	}

	/* Return either 0 or one/more of errors 0x04 and/or 0x08 */
	return rc;
}

/* ===========================================================================
-- Allocate (or deallocate) ring buffer for images
--
-- Usage: int TL_SetRingBufferSize(TL_CAMERA *camera, int nBuf);
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
int TL_SetRingBufferSize(TL_CAMERA *camera, int nBuf) {
	static char *rname = "TL_SetRingBufferSize";

	int i;

	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC || camera->handle == NULL) return 0;

	/* Get access to the memory structures */
	if (WAIT_OBJECT_0 == WaitForSingleObject(camera->image_mutex, 5*TL_IMAGE_ACCESS_TIMEOUT)) {
		nBuf = max(1, min(TL_MAX_RING_SIZE, nBuf));
		if (nBuf < camera->nBuffers) {								/* Need to release buffers */
			for (i=nBuf; i<camera->nBuffers; i++) free(camera->raw[i]);
			camera->raw       = realloc(camera->raw, nBuf*sizeof(*camera->raw));
			camera->timestamp = realloc(camera->timestamp, nBuf*sizeof(*camera->timestamp));
			camera->nBuffers  = nBuf;
		} else if (nBuf > camera->nBuffers) {						/* Need to increase number of buffers */
			camera->raw = realloc(camera->raw, nBuf*sizeof(*camera->raw));
			for (i=camera->nValid; i<=nBuf; i++) {
				if ( (camera->raw[i] = malloc(camera->nbytes_raw)) == NULL) {
					fprintf(stderr, "[%s:] Only able to increase buffer size to %d\n", rname, i); fflush(stderr);
					nBuf = i;
					break;
				}
			}
			camera->timestamp = realloc(camera->timestamp, nBuf*sizeof(*camera->timestamp));
			camera->nBuffers  = nBuf;
		}
		camera->nValid = camera->iLast = camera->iShow = 0;
		ReleaseMutex(camera->image_mutex);
	}

	return camera->nBuffers;
}

/* ===========================================================================
-- Routine to close a camera and release associated resources
--
-- Usage: int TL_CloseCamera(TL_CAMERA *camera);
--
-- Inputs: camera - pointer to valid opened camera
--                  (if NULL, is a nop returning rc=1)
--
-- Output: Closes camera and releases resources
--         Camera remains in the list of known cameras however
--
-- Return: 0 if successful, otherwise error code
--           0x01 -> camera is invalid (NULL, invalid, or already closed)
--           0x02 -> failed to close the camera handle
--           0x04 -> failed to release the color processor
=========================================================================== */
int TL_CloseCamera(TL_CAMERA *camera) {
	static char *rname = "TL_CloseCamera";

	int rc;
	
	/* Assume successful return */
	rc = 0;

	/* Verify that the structure is valid and hasn't already been closed */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;
	if (camera->handle == NULL) return 0;						/* Already closed ... can call multiple */
	
	/* Close the camera handle */
	if (tl_camera_close_camera(camera->handle) != 0) {
		fprintf(stderr, "Failed to close camera %s: (%s)\n", camera->ID, tl_camera_get_last_error()); fflush(stderr);
		rc |= 0x02;
	}
	camera->handle = NULL;

	/* Destroy the color processor */
	if (camera->color_processor != NULL) {
		if (tl_mono_to_color_destroy_mono_to_color_processor(camera->color_processor) != 0) {
			fprintf(stderr, "Failed to destory color processor for camera %s: (%s)\n", camera->ID, tl_mono_to_color_get_last_error()); 
			fflush(stderr);
			rc |= 0x04;
		}
		camera->color_processor = NULL;
	}

	/* Release allocated memory for the image */
	if (camera->raw != NULL) {
		int i;
		for (i=0; i<camera->nBuffers; i++) free(camera->raw[i]);
		free(camera->raw); camera->raw = NULL;
	}
	if (camera->timestamp != NULL) { free(camera->timestamp); camera->timestamp = NULL; }
	if (camera->red       != NULL) { free(camera->red);       camera->red       = NULL; }
	if (camera->green     != NULL) { free(camera->green);     camera->green     = NULL; }
	if (camera->blue      != NULL) { free(camera->blue);      camera->blue      = NULL; }
	if (camera->rgb24     != NULL) { free(camera->rgb24);     camera->rgb24     = NULL; }

	/* Release semaphores */
	CloseHandle(camera->image_mutex);

	return rc;
}


/* ===========================================================================
-- Routine to completely remove a camera from the list.  Does an implicit
-- TL_CloseCamera before releasing the entire structure.  After this call,
-- the camera structure will be invalid and not in the mast list.
--
-- Usage: int TL_ForgetCamera(TL_CAMERA *camera);
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
int TL_ForgetCamera(TL_CAMERA *camera) {
	static char *rname = "TL_ForgetCamera";

	int i,j;

	/* Verify that the structure is valid and hasn't already been closed */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	/* Close if not already done */
	TL_CloseCamera(camera);

	/* Remove from the list of known cameras */
	for (i=0; i<tl_camera_count; i++) {
		if (tl_camera_list[i] == camera) {
			for (j=i; j<tl_camera_count-1; j++) tl_camera_list[j] = tl_camera_list[j+1];
			tl_camera_list[j] = NULL;
			tl_camera_count--;
			break;
		}
	}

	/* Finally mark the structure invalid and release the structure itself */
	camera->magic = 0;
	free(camera);
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
-- Test if a given camera pointer is active and live
--
-- Usage: BOOL TL_IsValidCamera(TL_CAMERA *camera);
--
-- Inputs: camera - pointer to a TL_CAMERA structure ... hopefully opened
--
-- Output: none
--
-- Return: TRUE if pointer appears to be to a valid active camera structure
=========================================================================== */
BOOL TL_IsValidCamera(TL_CAMERA *camera) {
	static char *rname = "TL_IsValidCamera";

	return camera != NULL && camera->magic == TL_CAMERA_MAGIC;
}

/* ===========================================================================
-- Routine to request a signal when new image arrives for a camera
--
-- Usage: int TL_AddImageSignal(TL_CAMERA *camera, HANDLE signal);
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
--        "camera->image_mutex" to ensure data is not changed before all
--        processing is complete.
=========================================================================== */
int TL_AddImageSignal(TL_CAMERA *camera, HANDLE signal) {
	static char *rname = "TL_AddImageSignal";

	int i;

	/* Verify that the structure is valid and hasn't already been closed */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	for (i=0; i<TL_MAX_SIGNALS; i++) {
		if (camera->new_image_signals[i] == NULL) {
			camera->new_image_signals[i] = signal;
			return 0;
		}
	}
	
	/* No space for another signal */
	return 2;
}

/* ===========================================================================
-- Routine to remove an event semaphore signal from the active list
--
-- Usage: int TL_RemoveImageSignal(TL_CAMERA *camera, HANDLE signal);
--
-- Inputs: camera - valid pointer to an existing opened camera
--         signal - existing event semaphore signal to be removed
--
-- Output: Removes the signal from the list if it is there
--
-- Return: 0 if found, 1 if camera invalid, or 2 if signal wasn't in list
=========================================================================== */
int TL_RemoveImageSignal(TL_CAMERA *camera, HANDLE signal) {
	static char *rname = "TL_RemoveImageSignal";

	int i;

	/* Verify that the structure is valid and hasn't already been closed */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	for (i=0; i<TL_MAX_SIGNALS; i++) {
		if (camera->new_image_signals[i] == signal) {
			camera->new_image_signals[i] = NULL;
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
-- Routine to determine the buffer number within the ring corresponding to a 
-- requested frame relative to current.  0 => current, 1 => back one, ...
--
-- Usage: int ibuf_from_frame(TL_CAMERA *camera, int frame);
--
-- Inputs: camera - valid camera
--         frame  - requested frame number
--
-- Output: none
--
-- Return: best guess of index of this frame within the buffers
=========================================================================== */
static int ibuf_from_frame(TL_CAMERA *camera, int frame) {

	int ibuf;

	/* Hopefully don't get called when the camera is not initialized, but be safe */
	if (camera->nBuffers < 1 || camera->nValid < 1) return 0;

	/* Limit the frame to [0, nBuffers-1] */
	frame = max(0, min(camera->nValid-1, frame));

	/* Count backwards from the last used buffer (iLast) */
	ibuf = camera->iLast - frame;
	if (ibuf < 0) ibuf += camera->nBuffers;

	return ibuf;
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
	
	TL_CAMERA *camera;
	int i;

	/* Variables to process the metadata */
	UINT32 dval;									/* 4-byte integer */
	union {
		UINT64 value;								/* 8-byte timestamp (in ns) */
		UINT32 word[2];
	} timestamp;
	char tag[5];

	static HIRES_TIMER *timer = NULL;

	if (timer == NULL) timer = HiResTimerCreate();

	timestamp.value = 0;
	for (i=0; i<metadata_size_in_bytes; i+=8) {
		memcpy(&dval, metadata+i+4, 4);							/* Make a dval so can handle */
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
	if ( (camera = TL_FindCameraByHandle(sender)) == NULL) {
		fprintf(stderr, "ERROR: Unable to identify the camera for this callback\n"); fflush(stderr);
		return;
	}

	/* Take control of the memory buffers */
	if (WAIT_OBJECT_0 == WaitForSingleObject(camera->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {
		int ibuf;							/* Which buffer gets the data */
		unsigned short *raw;				/* Pointer to actual data */

		/* Put into the next available position */
		ibuf = (camera->nValid == 0) ? 0 : camera->iLast+1;
		if (ibuf >= camera->nBuffers) ibuf = 0;
		raw = camera->raw[ibuf];		/* Point to the requested buffer */

		/* Save where we are and increment the number of valid (up to nBuffers) */
		camera->iLast = ibuf;
		if (camera->nValid < camera->nBuffers) camera->nValid++;

		/* Copy raw data from sensor (<0.45 ms) */
		memcpy(raw, image_buffer, camera->nbytes_raw);

		/* Save image timestamp .. documentation (page 42) incorrect ... clock seems to be exactly 99 MHz, not reported value */
		camera->timestamp[ibuf] = timestamp.value/99000000.0;

		/* Copy framecount and mark raw data valid, other datas "not done" */
		camera->frame_count = frame_count;
		camera->valid_raw = TRUE;

		/* These are optional ... no reason to do unless they are used later */
//		TL_ProcessRawSeparation(camera);
//		TL_ProcessRGB(camera);

		/* We are done needing exclusive access to the memory buffers */
		ReleaseMutex(camera->image_mutex);

//		fprintf(stderr, "[%4.4d] %10.6f:  %10.6f  sender: 0x%p  buffer: 0x%p  meta_buffer: 0x%p  size: %d\n", frame_count, HiResTimerDelta(timer), camera->timestamp, sender, image_buffer, metadata, metadata_size_in_bytes);

		/* Set all event semaphores that have been registered (want to process images) */
		for (i=0; i<TL_MAX_SIGNALS; i++) {
			if (camera->new_image_signals[i] != NULL) SetEvent(camera->new_image_signals[i]);
		}
	}

	return;
}


/* ===========================================================================
-- Convert the raw buffer in camera structure to separated red, green and blue
--
-- Usage: int TL_ProcessRawSeparation(TL_CAMERA *camera, int frame);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         frame  - frame to process from buffers (0 = most recent)
--
-- Output: fills in the ->red, ->green, ->blue buffers in camera
--
-- Return: 0 if successful.  
--            1 => not valid camera
--	           2 => no image yet valid in the camera structure
--            3 => one of the buffers needed has not been allocated
--            4 => unable to get the semaphore for image data access
--
-- Note: Calling thread is assumed to have camera->image_mutex semaphore
=========================================================================== */
int TL_ProcessRawSeparation(TL_CAMERA *camera, int frame) {
	static char *rname = "TL_ProcessRawSeparation";

	int rc, row,col,ibuf;
	unsigned short *red, *green, *blue, *raw;

	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	/* Make sure we have valid data */
	if (! camera->valid_raw) return 2;

	/* What is the internal buffer that is being requested (0 = current) */
	ibuf = ibuf_from_frame(camera, frame);

	/* Once done for a raw image, don't ever need to repeat */
	if (camera->separations_timestamp == camera->timestamp[ibuf]) return 0;		/* Already done */

	/* And must have the structures defined */
	if (camera->raw == NULL || camera->red == NULL || camera->green == NULL || camera->blue == NULL) return 3;

	/* Get control of the memory buffers */
	if (WAIT_OBJECT_0 == WaitForSingleObject(camera->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {

		/* Separate the raw signal into raw red/green/blue buffers */
		raw = camera->raw[ibuf];			/* Point to the requested buffer */
		red = camera->red; green = camera->green; blue = camera->blue;
		for (row=0; row<camera->height; row++) {
			for (col=0; col<camera->width; col+=2) {
				if (row%2 == 0) {																/* Even rows */
					*(green++) = *(raw++); *(red++) = *(raw++); 
				} else {
					*(blue++) = *(raw++); *(green++) = *(raw++);
				}
			}
		}
		camera->separations_timestamp = camera->timestamp[ibuf];
		ReleaseMutex(camera->image_mutex);												/* Done with the mutex */
		rc = 0;

	} else {
		rc = 4;
	}

	return rc;
}

/* ===========================================================================
-- Convert the raw buffer in camera structure to full RGB24 buffer
--
-- Usage: int TL_ProcessRGB(TL_CAMERA *camera, int frame);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         frame  - frame to process from buffers (0 = most recent)
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
-- Note: Calling thread is assumed to have camera->image_mutex semaphore
=========================================================================== */
int TL_ProcessRGB(TL_CAMERA *camera, int frame) {
	static char *rname = "TL_ProcessRGB";

	int rc, ibuf;
	unsigned short *raw;
	
	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	/* Make sure we have valid data */
	if (! camera->valid_raw) return 2;

	/* What is the internal buffer that is being requested (0 = current) */
	ibuf = ibuf_from_frame(camera, frame);

	/* Once done for a raw image, don't ever need to repeat */
	if (camera->rgb24_timestamp == camera->timestamp[ibuf]) return 0;

	/* And must have the structures defined */
	if (camera->rgb24 == NULL) return 3;
	if (camera->color_processor == NULL) return 4;

	/* Get control of the memory buffers */
	if (WAIT_OBJECT_0 == WaitForSingleObject(camera->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {

		/* Convert to true RGB format */
		raw = camera->raw[ibuf];			/* Point to the requested buffer */
		if (tl_mono_to_color_transform_to_24(camera->color_processor, raw, camera->width, camera->height, camera->rgb24) != 0) {
			fprintf(stderr, "Unable to transform to rgb24: %s\n", tl_mono_to_color_get_last_error()); fflush(stderr);
		} else {
			int i, itmp;
			for (i=0; i<3*camera->width*camera->height; i+=3) {					/* Reverse order or rgb to bgr to match DCX pattern */
				itmp = camera->rgb24[i+0];
				camera->rgb24[i+0] = camera->rgb24[i+2];
				camera->rgb24[i+2] = itmp;
			}
			camera->rgb24_timestamp = camera->timestamp[ibuf];
		}
		ReleaseMutex(camera->image_mutex);												/* Done with the mutex */

		rc = 0;

	/* Unable to get the semaphore error */
	} else {
		rc = 5;
	}

	return rc;
}

/* ===========================================================================
-- Convert from internal structure to device independent bitmap (DIB) for display
-- 
-- Usage: BITMAPINFOHEADER *TL_GenerateDIB(TL_CAMERA *camera, int frame, int *rc);
--
-- Inputs: camera - an opened TL camera
--         frame  - frame to process from buffers (0 = most recent)
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
BITMAPINFOHEADER *TL_CreateDIB(TL_CAMERA *camera, int frame, int *rc) {
	static char *rname = "TL_CreateDIB";

	BITMAPINFOHEADER *bmih;
	unsigned char *data;
	int irow, icol, ineed;
	int my_rc;

	/* Make life easy if user doesn't want error codes */
	if (rc == NULL) rc = &my_rc;
	*rc = 0;

	/* Verify that the structure is valid and hasn't already been closed */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) { *rc = 1; return NULL; }
	if (camera->rgb24 == NULL) { *rc = 3; return NULL; }

	/* Make sure we have data and color information loaded */
	if (0 != (*rc = TL_ProcessRGB(camera, frame))) return NULL;

	/* Allocate the structure */
	ineed = sizeof(*bmih)+3*camera->width*camera->height;
	if ( (bmih = calloc(1, ineed)) == NULL) { *rc = 4; return NULL; }

	/* Fill in the bitmap information */
	bmih->biSize = sizeof(*bmih);					/* Only size of the header itself */
	bmih->biWidth         = camera->width;
	bmih->biHeight        = camera->height;	/* Make the image upright when saved */
	bmih->biPlanes        = 1;
	bmih->biBitCount      = 24;					/* Value for RGB24 color images */
	bmih->biCompression   = BI_RGB;
	bmih->biSizeImage     = 3*camera->width*camera->height;
	bmih->biXPelsPerMeter = 3780;					/* Just make it 96 ppi (doesn't matter) */
	bmih->biYPelsPerMeter = 3780;					/* Same value ThorCam reports */
	bmih->biClrUsed       = 0;
	bmih->biClrImportant  = 0;

	/* Get access to the mutex semaphore for the data processing */
	/* and save data reversing row sequence and byte sequence (colors / rotation) */
	/* Note, the reversal of bits in the rgb24 can be here or in ProcessRGB24 */
	if (WAIT_OBJECT_0 != WaitForSingleObject(camera->image_mutex, TL_IMAGE_ACCESS_TIMEOUT)) {
		*rc = 5;
		return NULL;
	} else {

		data = ((unsigned char *) bmih) + sizeof(*bmih);		/* Where RGB in the bitmap really starts */
		for (irow=0; irow<camera->height; irow++) {
			for (icol=0; icol<camera->width; icol++) {
#if 0			/* Switch bgr to rgb here */
				data[3*(irow*camera->width+icol)+0] = camera->rgb24[3*((camera->height-1-irow)*camera->width+icol)+2];
				data[3*(irow*camera->width+icol)+1] = camera->rgb24[3*((camera->height-1-irow)*camera->width+icol)+1];
				data[3*(irow*camera->width+icol)+2] = camera->rgb24[3*((camera->height-1-irow)*camera->width+icol)+0];
#else			/* Already done in TL_ProcessRGB */
				data[3*(irow*camera->width+icol)+0] = camera->rgb24[3*((camera->height-1-irow)*camera->width+icol)+0];
				data[3*(irow*camera->width+icol)+1] = camera->rgb24[3*((camera->height-1-irow)*camera->width+icol)+1];
				data[3*(irow*camera->width+icol)+2] = camera->rgb24[3*((camera->height-1-irow)*camera->width+icol)+2];
#endif
			}
		}
	}

	ReleaseMutex(camera->image_mutex);									/* Done with the mutex */
	*rc = 0;
	return bmih;
}

/* ===========================================================================
-- Save data from TL camera as a bitmap (.bmp) file
-- 
-- Usage: int TL_SaveBMPFile(TL_CAMERA *camera, char *path, int frame);
--
-- Inputs: camera - an opened TL camera
--         path   - pointer to name of a file to save data (or NULL for query)
--         frame  - which frame to output (0 => most recent)
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
int TL_SaveBMPFile(TL_CAMERA *camera, char *path, int frame) {
	static char *rname = "TL_SaveBMPFile";

	BITMAPINFOHEADER *bmih=NULL;
	BITMAPFILEHEADER  bmfh;
	int rc, isize;
	FILE *funit;

	/* Verify all is okay and get a bitmap corresponding to current image */
	/* CreateDIB locks the semaphore while creating the DIB, but once
	 * the bmih is created, we no longer need access to the raw data */
	if ( (bmih = TL_CreateDIB(camera, frame, &rc)) == NULL) return rc;
	isize = sizeof(*bmih)+3*camera->width*camera->height;

	/* Create the file header */
	memset(&bmfh, 0, sizeof(bmfh));	
	bmfh.bfType = 19778;
	bmfh.bfSize = sizeof(bmfh)+isize;
	bmfh.bfOffBits = sizeof(bmfh)+sizeof(*bmih);

	if ( (fopen_s(&funit, path, "wb")) != 0) {
		fprintf(stderr, "TL_SaveBMPFile: Failed to open \"%s\"\n", path); fflush(stderr);
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
-- Render an image in a specified window
-- 
-- Usage: int TL_RenderImage(TL_CAMERA *camera, int frame, HWND hwnd);
--
-- Inputs: camera - an opened TL camera
--         frame  - frame to process from buffers (0 = most recent)
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
int TL_RenderImage(TL_CAMERA *camera, int frame, HWND hwnd) {
	static char *rname = "TL_RenderImage";

	HDC hdc;
	BITMAPINFOHEADER *bmih;
	HBITMAP hBitmap;
	HDC       hDCBits;
	BITMAP    Bitmap;
	BOOL      bResult;
	RECT		 Client;
	static sig_atomic_t active = 0;

	if (active != 0 || camera == NULL || hwnd == NULL) return 1;						/* Don't even bother trying */
	active++;

	/* Get the bitmap to render (will deal with processing to RGB and all semaphores) */
	if ( NULL == (bmih = TL_CreateDIB(camera, frame, NULL))) { active--; return 2; }		/* Unable to create the bitmap */

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
-- Usage: double TL_GetExposure(TL_CAMERA *camera, BOOL bForceQuery);
--
-- Inputs: camera      - pointer to valid TL_CAMERA
--         bForceQuery - if TRUE, will query camera directly to update
--                       the value in the TL_CAMERA structure.
--
-- Output: none
--
-- Return: 0 on error; otherwise exposure time in ms
=========================================================================== */
double TL_GetExposure(TL_CAMERA *camera, BOOL bForceQuery) {
	static char *rname = "TL_GetExposure";

	long long us_expose;

	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 0.0;

	/* If bForceQuery, update directly from camera */
	if (bForceQuery) {
		if (tl_camera_get_exposure_time(camera->handle, &us_expose) != 0) {
			fprintf(stderr, "[%s:] Failed to get exposure time: %s)\n", rname, tl_camera_get_last_error());
			fflush(stderr);
		} else {
			camera->us_expose = us_expose;
		}
	}

	return 0.001*camera->us_expose;
}

/* ===========================================================================
-- Set the exposure time for a camera
--
-- Usage: double TL_SetExposure(TL_CAMERA *camera, double ms_expose);
--
-- Inputs: camera    - an opened TL camera
--         ms_expose - requested exposure in milliseconds
--                     set to 0 or negative to just return current value
--
-- Output: If exposure>0, sets exposure to the closest valid
--
-- Return: Current exposure time in milliseconds or 0 on error
=========================================================================== */
double TL_SetExposure(TL_CAMERA *camera, double ms_expose) {
	static char *rname = "TL_SetExposure";

	long long us_expose;

	/* Make sure we are alive and the camera is connected (open) */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 0.0;

	if (ms_expose > 0.0) {
		us_expose = (int) (1000*ms_expose + 0.5);
		if (tl_camera_set_exposure_time(camera->handle, us_expose) != 0) { 
			fprintf(stderr, "[%s:] Unable to set exposure time: %s\n", rname, tl_camera_get_last_error()); 
			fflush(stderr);
		} else {
			camera->us_expose = us_expose;
		}
	}

	/* Try to verify the value */
	if (tl_camera_get_exposure_time(camera->handle, &us_expose) != 0) {
		fprintf(stderr, "[%s:] Failed to get exposure time: %s)\n", rname, tl_camera_get_last_error());
		fflush(stderr);
	} else {
		camera->us_expose = us_expose;
	}

	return 0.001*camera->us_expose;
}

/* ===========================================================================
-- Sets the frame rate
--
-- Usage: double TL_SetFPSControl(TL_CAMERA *camera, double fps);
--
-- Inputs: camera - an opened TL camera
--         fps    - desired fps rate
--
-- Output: none
--
-- Return: framerate or 0.0 on any error
=========================================================================== */
double TL_SetFPSControl(TL_CAMERA *camera, double fps) {
	static char *rname = "TL_SetFPSControl";

	/* Make sure we are alive and the camera is connected (open) */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 0.0;

	/* Query will fail if camera does not support frame rate control */
	if (! camera->bFrameRateControl) return 0.0;

	/* Try to set */
	if (tl_camera_set_frame_rate_control_value(camera->handle, fps) != 0) {
		fprintf(stderr, "[%s:] Failed to get frame rate control: %s)\n", rname, tl_camera_get_last_error());
		fflush(stderr);
	} 

	/* And try to read back value for return, even if unsuccessful */
	if (tl_camera_get_frame_rate_control_value(camera->handle, &fps) != 0) {
		fprintf(stderr, "[%s:] Failed to get frame rate control: %s)\n", rname, tl_camera_get_last_error());
		fflush(stderr);
		fps = 0.0;
	}

	/* Return best guess of value */
	return fps;
}


/* ===========================================================================
-- Query the set frame rate and the estimate actual rate
--
-- Usage: double TL_GetFPSControl(TL_CAMERA *camera);
--
-- Inputs: camera - an opened TL camera
--
-- Output: none
--
-- Return: Returns -1 if camera does not support frame rate control.
=========================================================================== */
double TL_GetFPSControl(TL_CAMERA *camera) {
	static char *rname = "TL_GetFPSControl";

	double rval;

	/* Make sure we are alive and the camera is connected (open) */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 0.0;

	/* Query will fail if camera does not support frame rate control */
	if (! camera->bFrameRateControl) {
		rval = -1.0;
	} else if (tl_camera_get_frame_rate_control_value(camera->handle, &rval) != 0) {
		fprintf(stderr, "[%s:] Failed to get frame rate control: %s)\n", rname, tl_camera_get_last_error());
		fflush(stderr);
		rval = -1.0;
	}
	return rval;
}

/* ===========================================================================
-- Query estimated frame rate based on image acquisition timestamps
--
-- Usage: double TL_GetFPSActual(TL_CAMERA *camera);
--
-- Inputs: camera - an opened TL camera
--
-- Output: none
--
-- Return: Measured frame rate from the camera
=========================================================================== */
double TL_GetFPSActual(TL_CAMERA *camera) {
	static char *rname = "TL_GetFPSActual";

	double rval;

	/* Make sure we are alive and the camera is connected (open) */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 0.0;

	if (tl_camera_get_measured_frame_rate(camera->handle, &rval) != 0) {
		fprintf(stderr, "[%s:] Failed to get frame rate: %s)\n", rname, tl_camera_get_last_error());
		fflush(stderr);
		rval = -1.0;
	}

	return rval;
}

/* ===========================================================================
-- Set the master gain for the camera (in dB)
--
-- Usage: int TL_SetMasterGain(TL_CAMERA *camera, double dB_gain);
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
int TL_SetMasterGain(TL_CAMERA *camera, double dB_gain) {
	static char *rname = "TL_SetMasterGain";

	int gain_index;
	
	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	/* Must be able to control gain */
	if (! camera->bGainControl) return 2;
	
	if (dB_gain < camera->db_min) dB_gain = camera->db_min;
	if (dB_gain > camera->db_max) dB_gain = camera->db_max;

	if (tl_camera_convert_decibels_to_gain(camera->handle, dB_gain, &gain_index) != 0) { 
		fprintf(stderr, "[%s:] Unable to convert gain dB: %s\n", rname, tl_camera_get_last_error());
		fflush(stderr);
		return 3;
	} else if (tl_camera_set_gain(camera->handle, gain_index) != 0) {
		fprintf(stderr, "[%s:] Unable to set gain to index determined: %s\n", rname, tl_camera_get_last_error());
		fflush(stderr);
		return 3;
	}

	return 0;
}

/* ===========================================================================
-- Query the master gain for the camera (in dB)
--
-- Usage: double TL_GetMasterGain(TL_CAMERA *camera);
--
-- Inputs: camera      - pointer to valid TL_CAMERA
--
-- Output: none
--
-- Return: Gain read back in dB or a negative value on error
--          -1 ==> bad camera structure
--          -2 ==> camera does not support gain
=========================================================================== */
int TL_GetMasterGain(TL_CAMERA *camera, double *db) {
	static char *rname = "TL_GetMasterGain";

	int gain_index;
	double dB_gain;

	/* Default return values */
	if (db != NULL) *db = 0;

	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;
	if (! camera->bGainControl) return 2;

	if (tl_camera_get_gain(camera->handle, &gain_index) != 0) {
		fprintf(stderr, "[%s:] Unable to read gain: %s\n", rname, tl_camera_get_last_error());
		return 3;
	} else if (tl_camera_convert_gain_to_decibels(camera->handle, gain_index, &dB_gain) != 0) {
		fprintf(stderr, "[%s:] Unable to convert gain index to dB: %s\n", rname, tl_camera_get_last_error());
		return 3;
	}

	if (db != NULL) *db = dB_gain;
	return 0;
}

/* ===========================================================================
-- Query the master gain range for the camera (in dB)
--
-- Usage: int TL_GetMasterGainInfo(TL_CAMERA *camera, BOOL *bGain, double *db_dflt, double *db_min, double *db_max);
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
int TL_GetMasterGainInfo(TL_CAMERA *camera, BOOL *bGain, double *db_dflt, double *db_min, double *db_max) {
	static char *rname = "TL_GetMasterGainRange";

	/* Default return values */
	if (bGain   != NULL) *bGain   = FALSE;
	if (db_dflt != NULL) *db_dflt = 0;
	if (db_min  != NULL) *db_min  = 0;
	if (db_max  != NULL) *db_max  = 6;

	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	/* Otherwise, copy values to return */
	if (bGain != NULL) *bGain = camera->bGainControl;
	if (camera->bGainControl) {
		if (db_dflt != NULL) *db_dflt = camera->db_dflt;
		if (db_min  != NULL) *db_min  = camera->db_min;
		if (db_max  != NULL) *db_max  = camera->db_max;
	}
	return 0;
}


/* ===========================================================================
-- Query the RGB channel gains
--
-- Usage: double TL_GetRGBGains(TL_CAMERA *camera, double *red, double *green, double *blue);
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
int TL_GetRGBGains(TL_CAMERA *camera, double *red, double *green, double *blue) {
	static char *rname = "TL_GetRGBGains";

	int rc;
	float R, G, B;

	/* Default return values */
	if (red   != NULL) *red   = 0;
	if (green != NULL) *green = 0;
	if (blue  != NULL) *blue  = 0;
	
	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	rc = 0;
	if (tl_mono_to_color_get_red_gain(camera->color_processor, &R) != 0) {
		fprintf(stderr, "[%s:] Unable to query red gain: %s\n", rname, tl_mono_to_color_get_last_error()); 
		rc |= 0x02;
	}
	if (tl_mono_to_color_get_green_gain(camera->color_processor, &G) != 0) {
		fprintf(stderr, "[%s:] Unable to query green gain: %s\n", rname, tl_mono_to_color_get_last_error()); 
		rc |= 0x04;
	}
	if (tl_mono_to_color_get_blue_gain(camera->color_processor, &B) != 0) {
		fprintf(stderr, "[%s:] Unable to query blue gain: %s\n", rname, tl_mono_to_color_get_last_error()); 
		rc |= 0x08;
	}
	if (rc != 0) fflush(stderr);

	if (red   != NULL) *red   = R;
	if (green != NULL) *green = G;
	if (blue  != NULL) *blue  = B;

	return rc;
}

/* ===========================================================================
-- Query the default RGB channel gains (original values)
--
-- Usage: double TL_GetDfltRGBGains(TL_CAMERA *camera, double *red, double *green, double *blue);
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
int TL_GetDfltRGBGains(TL_CAMERA *camera, double *red, double *green, double *blue) {
	static char *rname = "TL_DfltGetRGBGains";

	/* Default return value */
	if (red   != NULL) *red   = 0;
	if (green != NULL) *green = 0;
	if (blue  != NULL) *blue  = 0;

	/* Verify we can report a value */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	/* All okay, so report initial value when we started */
	if (red   != NULL) *red   = camera->red_dflt;
	if (green != NULL) *green = camera->green_dflt;
	if (blue  != NULL) *blue  = camera->blue_dflt;

	return 0;
}


/* ===========================================================================
-- Set the RGB channel gains
--
-- Usage: double TL_SetRGBGains(TL_CAMERA *camera, double red, double green, double blue);
--
-- Inputs: camera - pointer to valid TL_CAMERA
--         red, green, blue - gain values
--            if TL_IGNORE_GAIN is used, the channel gain will be left unchanged
--
-- Output: sets gains within the mono_to_color_processor
--
-- Return: 0 if successful, otherwise
--           1 ==> bad camera structure
=========================================================================== */
int TL_SetRGBGains(TL_CAMERA *camera, double red, double green, double blue) {
	static char *rname = "TL_SetRGBGains";

	int rc;
	
	/* Must be valid structure */
	if (camera == NULL || camera->magic != TL_CAMERA_MAGIC) return 1;

	rc = 0;
	if (red != TL_IGNORE_GAIN) {
		if (tl_mono_to_color_set_red_gain(camera->color_processor, (float) red) != 0) {
			fprintf(stderr, "[%s:] Unable to set red gain: %s\n", rname, tl_mono_to_color_get_last_error());
			fflush(stderr);
			rc |= 0x02;
		}
	}

	if (green != TL_IGNORE_GAIN) {
		if (tl_mono_to_color_set_green_gain(camera->color_processor, (float) green) != 0) {
			fprintf(stderr, "[%s:] Unable to set green gain: %s\n", rname, tl_mono_to_color_get_last_error()); 
			fflush(stderr);
			rc |= 0x04;
		}
	}

	if (blue != TL_IGNORE_GAIN) {
		if (tl_mono_to_color_set_blue_gain(camera->color_processor, (float) blue) != 0) {
			fprintf(stderr, "[%s:] Unable to set blue gain: %s\n", rname, tl_mono_to_color_get_last_error()); 
			fflush(stderr);
			rc |= 0x02;
		}
	}

	return 0;
}
