/* test server (standalone) */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stddef.h>				  /* for defining several useful types and macros */
#include <stdio.h>				  /* for performing input and output */
#include <stdlib.h>				  /* for performing a variety of operations */
#include <string.h>
#include <math.h>               /* basic math functions */
#include <assert.h>
#include <stdint.h>             /* C99 extension to get known width integers */

/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "server_support.h"		/* Server support routine */
#include "ZooCam.h"					/* Access to the ZooCam info */
#include "DCx_server.h"				/* Prototypes for main	  */
#include "DCx_client.h"				/* Version info and port  */

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
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
/* My share of global externals    */
/* ------------------------------- */

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */

int DCx_Acquire_Image(DCX_IMAGE_INFO *info, char **buffer) {

	uint32_t i,j,pitch;
	char *image;

	/* Fake an RGB image */
	info->height = 1024;
	info->width  = 1280;
	info->memory_pitch = 3840;
	info->exposure = 99.87;
	info->gamma = 1.00;
	info->master_gain = 100;
	info->red_gain = 15;
	info->green_gain = 0;
	info->blue_gain = 25;
	info->color_correction = 1;
	info->color_correction_factor = 0.00;
	info->red_saturate = 283;
	info->green_saturate = 8273;
	info->blue_saturate = 82;

	*buffer = image = malloc(info->memory_pitch * info->height);
	pitch = info->memory_pitch;
	for (i=0; i<info->height; i++) {
		for (j=0; j<info->width; j++) {
			image[i*pitch+3*j+0] = (j) % 255 ;
			image[i*pitch+3*j+1] = (info->width-j) % 255;
			image[i*pitch+3*j+2] = (j+128) % 255;
		}
	}
	return 0;
}

int DCx_Status(DCX_STATUS *status) {
	strcpy_s(status->manufacturer, sizeof(status->manufacturer), "Manufacturer: Thorlabs GmbH");
	strcpy_s(status->model, sizeof(status->model), "C1284R13C");
	strcpy_s(status->serial, sizeof(status->serial), "Serial: 4103534309");
	strcpy_s(status->version, sizeof(status->version),"Version: V1.0");
	strcpy_s(status->date, sizeof(status->date), "Date: 22.05.2019");
	status->CameraID = 1;
	status->color_mode = IMAGE_COLOR;
	status->pixel_pitch = 360;
	status->fps = 5.0;
	status->exposure = 99.87;
	status->gamma = 1.00;
	status->master_gain = 0;
	status->red_gain    = 15;
	status->green_gain  = 0;
	status->blue_gain   = 25;
	status->color_correction = 1;
	status->color_correction_factor = 0.0;
	return 0;
}

int main(int argc, char *argv[]) {
	int rc;

	rc = Init_DCx_Server();
	fprintf(stderr, "Init_DCx_Server() returns: %d\n", rc); fflush(stderr);
	if (rc != 0) return 3;
	while (TRUE) Sleep(500);
	return 0;
}
