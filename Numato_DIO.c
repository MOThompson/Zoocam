/* Numato_DIO.c */

/* ------------------------------ */
/* Feature test macros            */
/* ------------------------------ */
#define _POSIX_SOURCE						/* Always require POSIX standard */

/* ------------------------------ */
/* Standard include files         */
/* ------------------------------ */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#undef	_POSIX_
//	#include <io.h>
	#include <windows.h>
#define	_POSIX_
	
/* ------------------------------ */
/* Local include files            */
/* ------------------------------ */
#include "numato_DIO.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#ifndef TRUE
	#define	TRUE	(1)
#endif
#ifndef FALSE
	#define	FALSE	(0)
#endif

#define	NUMATO_MAGIC	0x826B

typedef struct _NUMATO {
	int magic;						/* Validate structure */
	int status;						/* What is the current status of the unit */
	int port;						/* Which COM port is associated with this unit */
	HANDLE hComPort;				/* System file handle for communications */
} NUMATO;

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
-- Routine to open/close a NUMATO unit for write control
--
-- Usage:  NUMATO *NumatoOpenDIO(int port, int *ierr);
--         int NumatoCloseDIO(NUMATO *dio);
--
-- Inputs: port - com port number configured for the DIO unit
--         ierr - if not NULL, address for return of error code
--
-- Output: *ierr - error code (if ierr not NULL)
--            0 ==> successfully opened requested and makes basic verification passed
--            1 ==> specified port invalid (0 or negative)
--            2 ==> com port failed to open
--
-- Return: Pointer to a NUMATO * structure to be used in calls
=========================================================================== */
NUMATO *NumatoOpenDIO(int port, int *ierr) {

	NUMATO *dio;
	int rc, icnt;
	HANDLE hComPort;
	COMMTIMEOUTS timeouts;
	char com_port[10], msg[256];

	if (ierr == NULL) ierr = &rc;								/* Just so can always write it ierr */

	/* Verify it is a valid port number */
	if (port < 0) { *ierr = 1; return NULL; }

	/* Try to open the com port and set basic communication protocols */
	sprintf_s(com_port, sizeof(com_port), "\\\\.\\COM%d", port);
	hComPort = CreateFile(com_port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hComPort == INVALID_HANDLE_VALUE) {
		rc = GetLastError();
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, rc, 0, msg, sizeof(msg), NULL);
		fprintf(stderr, "ERROR: Unable to open the serial port on %s\n", com_port); fflush(stderr);
		fprintf(stderr, msg); fflush(stderr);
		*ierr = 2;
		return NULL;
	}

	/* Set timeouts - seems to be critical! */
	timeouts.ReadIntervalTimeout = 1;				/* ms Between chars (10) */
	timeouts.ReadTotalTimeoutMultiplier = 1;		/* ms per char*/
	timeouts.ReadTotalTimeoutConstant = 1;			/* ms overhead (10) -- may be total time before abandoning read also */
	timeouts.WriteTotalTimeoutMultiplier = 1;		/* ms per char	*/
	timeouts.WriteTotalTimeoutConstant = 10;		/* ms overhead (10) */
	SetCommTimeouts(hComPort, &timeouts);

	/* Send a simple CR and clear the input buffer (remaining junk) */
	if (! WriteFile(hComPort, "\r", 1, &icnt, NULL)) {
		fprintf(stderr, "Numato: Unable to communicate with COMx port\n"); fflush(stderr);
		CloseHandle(hComPort);
		*ierr = 6;
		return NULL;
	}
	Sleep(10);
	PurgeComm(hComPort, PURGE_RXCLEAR | PURGE_RXABORT);

	/* All looks successful, so go ahead and create the structure and return */
	dio = calloc(1, sizeof(*dio));
	dio->magic    = NUMATO_MAGIC;
	dio->status   = 0;
	dio->port     = port;
	dio->hComPort = hComPort;

	/* Finally, do a quick "ver" query to see that things work */
	if (NumatoQuery(dio, "ver", NULL, 0) != 0) {
		fprintf(stderr, "WARNING: The com port opened but did not get a response back from the \"ver\" command\n");
		fflush(stderr);
	}

	*ierr = 0;
	return dio;
}

/* ===========================================================================
-- Routine to close a NUMATO unit
--
-- Usage:  int NumatoCloseDIO(NUMATO *dio);
--
-- Inputs: dio - pointer to previously opened Numato DIO unit
--
-- Output: closes the file descriptor associated with the DIO unit
--
-- Return: 0 on success, 1 if dio is not valid or previously closed
=========================================================================== */
int NumatoCloseDIO(NUMATO *dio) {
	if (dio == NULL || dio->magic != NUMATO_MAGIC || dio->status != 0) return 1;
	CloseHandle(dio->hComPort);
	dio->magic = 0;
	return 0;
}


/* ===========================================================================
-- Routine to send a string to the NUMATO unit and return the response string
--
-- Usage:  int NumatoFlush(NUMATO *dio);
--
-- Inputs: dio - pointer to previously opened Numato DIO unit
--
-- Output: clears the file handle buffer
--
-- Return: 0 on success
--            1 ==> bad dio (closed or invalid)
=========================================================================== */
int NumatoFlush(NUMATO *dio) {

	/* Verify parameters */
	if (dio == NULL || dio->magic != NUMATO_MAGIC || dio->status != 0) return 1;

	/* Flush the Serial port's RX buffer. This is a very important step*/
	Sleep(10);
	PurgeComm(dio->hComPort, PURGE_RXCLEAR | PURGE_RXABORT);
	return 0;
}

/* ===========================================================================
-- Routine to send a string to the NUMATO unit and return the response string
--
-- Usage:  int NumatoQuery(NUMATO *dio, char *query, char *response, size_t max_len);
--
-- Inputs: dio      - pointer to previously opened Numato DIO unit
--         query    - text to sent as query (no CR needed but limited to 250 characters)
--         response - pointer (if not NULL) to string to receive the response
--         max_len  - maximum length of response
--         ierr - if not NULL, address for return of error code
--
-- Output: *response - response string without the echo
--
-- Return: 0 on success
--            1 ==> bad dio (closed or invalid)
--            2 ==> query NULL or is longer than 250 characters
--            3 ==> error returned trying to write to the unit
--            4 ==> error return on _read
--            5 ==> no data returned on _read
--            6 ==> returned string shorter than query (invalid echo)
--            7 ==> returned string did not match the query itself (invalid echo)
=========================================================================== */
int NumatoQuery(NUMATO *dio, char *query, char *response, size_t max_len) {

	char szTmp[280], *aptr;
	int rdcnt, icnt;

	/* Verify parameters */
	if (dio == NULL || dio->magic != NUMATO_MAGIC || dio->status != 0) return 1;
	if (query == NULL || strlen(query) > 250) return 2;
	
	/* Set default return values */
	if (response != NULL && max_len > 0) *response = '\0';

	/* Send the query */
	sprintf_s(szTmp, sizeof(szTmp), "%s\r", query);
	if (! WriteFile(dio->hComPort, szTmp, (DWORD) strlen(szTmp), &icnt, NULL)) {
		fprintf(stderr, "Numato: WriteFile reported error: written: %d expect: %zu\n", icnt, strlen(szTmp));
		fflush(stderr);
		return 3;
	}

	/* Get the response */
	if (! ReadFile(dio->hComPort, szTmp, sizeof(szTmp), &rdcnt, NULL)) {
		fprintf(stderr, "Numato: ReadFile reported error\n");
		fflush(stderr);
		return 4;
	}
	szTmp[rdcnt] = '\0';
	if (rdcnt == 0) {
		fprintf(stderr, "NUMATO: read returned empty string\n"); fflush(stderr);
		return 5;
	}

	/* Need to terminate the string now before further processing */
	if (rdcnt < (int) (strlen(query)+2)) {
		fprintf(stderr, "NUMATO: read returned string shorter than the query itself ... no echo (rdcnt=%d, query=%zu)\n\t%s", rdcnt, strlen(query), szTmp); fflush(stderr);
		return 6;
	} else if (_strnicmp(szTmp, query, strlen(query)) != 0) {
		fprintf(stderr, "NUMATO: read returned text that doesn't match initial query ... bad echo\n\t%s", szTmp); fflush(stderr);
		return 7;
	}

	/* Strip trailing > and CR/LF */
	aptr = szTmp+rdcnt-1;
	if (*aptr == '>') *(aptr--) = '\0';
	if (*aptr == '\r' || *aptr == '\n') *(aptr--) = '\0';
	if (*aptr == '\r' || *aptr == '\n') *(aptr--) = '\0';
		
	/* Go past the query test, strip expected CR/LF, and return rest */
	aptr = szTmp+strlen(query);											/* Should point to the CR/LF */
	if (*aptr == '\r' || *aptr == '\n') aptr++;
	if (*aptr == '\r' || *aptr == '\n') aptr++;

	/* If response is not NULL, copy over and return */
	if (response != NULL && max_len > 0) strcpy_s(response, max_len, aptr);
	return 0;
}


/* ===========================================================================
-- Routine to send a string to the NUMATO unit and return the response string
--
-- Usage:  int NumatoSetIOdir(NUMATO *dio, unsigned int flags);
--
-- Inputs: dio      - pointer to previously opened Numato DIO unit
--         flags    - bitwise value for dio (0 ==> output, 1 ==> input)
--
-- Output: sets the direction on the board
--
-- Return: 0 on success, otherwise error from NumatoQuery
=========================================================================== */
int NumatoSetIOdir(NUMATO *dio, unsigned int flags) {

	char query[256];
	
	/* Just format and send */
	flags &= 0xFF;											/* Assume we are a 8 bit unit */
	sprintf_s(query, sizeof(query), "gpio iodir %2.2x", flags & 0xFF);
	return NumatoQuery(dio, query, NULL, 0);
}	

/* ===========================================================================
-- Routine to set a mask to limit behavior of the writeall/readall commands
-- Only bits that are 1 in the iomask are processed with the readall/writeall
-- commands; these are implemented in the NumatoSetAll(), NumatoQueryAll() calls
--
-- Usage:  int NumatoSetIOmask(NUMATO *dio, unsigned int flags);
--
-- Inputs: dio      - pointer to previously opened Numato DIO unit
--         flags    - bitwise value for dio (1 ==> process, 0 ==> ignore)
--
-- Output: sets whether the given bit will be included in global reads/writes
--
-- Return: 0 on success, otherwise error from NumatoQuery
=========================================================================== */
int NumatoSetIOmask(NUMATO *dio, unsigned int flags) {

	char query[256];

	/* Just format and send */
	flags &= 0xFF;											/* Assume we are a 8 bit unit */
	sprintf_s(query, sizeof(query), "gpio iomask %2.2x", flags & 0xFF);
	return NumatoQuery(dio, query, NULL, 0);
}	

/* ===========================================================================
-- Routine to set/clear or read a given bit from the DIO
--
-- Usage:  int NumatoSetBit(NUMATO *dio, int bit, BOOL on);
--         int NumatoQueryBit(NUMATO *dio, int bit, BOOL *on);
--
-- Inputs: dio  - pointer to previously opened Numato DIO unit
--         bit  - which bit to modify (0 to 7)
--         on   - TRUE to set, FALSE to clear
--         *on  - for Query, receives the current status
--
-- Output: sets or queries the individual bit.  The given bit
--         will be reclassified as an output as a result of this
--         call (IOdir equivalent automatically)
--
-- Return: 0 on success, otherwise error from NumatoQuery
--
-- Timing: Minimum pulse width is 20 ms.  SET/CLEAR period is 40 ms.
--         Jitter on pulse width is small. +/- 0.25 ms max (high or low)
=========================================================================== */
int NumatoSetBit(NUMATO *dio, int bit, BOOL on) {
	
	char query[64];
	
	if (bit < 0 || bit > 7) return -1;

	sprintf_s(query, sizeof(query), "gpio %s %d", on ? "set" : "clear", bit);
	return NumatoQuery(dio, query, NULL, 0);

}

int NumatoQueryBit(NUMATO *dio, int bit, BOOL *on) {

	int rc;
	char query[64], response[64];

	if (bit < 0 || bit > 7) return -1;

	sprintf_s(query, sizeof(query), "gpio read %d", bit);
	rc = NumatoQuery(dio, query, response, sizeof(response));
	if (on != NULL) *on = (_strnicmp(response, "on", 2) == 0);
	return rc;
}

/* ===========================================================================
-- Routine to set/clear or read a given bit from the DIO
--
-- Usage:  int NumatoWriteAll(NUMATO *dio, unsigned int flags);
--         int NumatoReadAll(NUMATO *dio, unsigned int *flags);
--
-- Inputs: dio   - pointer to previously opened Numato DIO unit
--         flags - values to be written to the bits
--         *flags - values retrieved from reading the unit
--
-- Output: sets or queries the full DIO.
--
-- Return: 0 on success, otherwise error from NumatoQuery
--
-- Notes: 1) Unlike set/read of an individual bit, this command does
--           not change whether a bit is input or output
--        2) The bits that are controlled by these command are 
--           limited to those with a 1 in the iomask
=========================================================================== */
int NumatoWriteAll(NUMATO *dio, unsigned int flags) {

	int rc;
	char query[64];

	flags &= 0xFF;								/* Limit to 8 bits */
	sprintf_s(query, sizeof(query), "gpio writeall %2.2x", flags);
	rc = NumatoQuery(dio, query, NULL, 0);

	return rc;
}

int NumatoReadAll(NUMATO *dio, unsigned int *flags) {

	int rc, value;
	char response[64], *aptr;

	if ( (rc = NumatoQuery(dio, "gpio readall", response, sizeof(response))) != 0) {
		if (flags != NULL) *flags = 0;
		return rc;
	}

	value = 0;
	for (aptr=response; *aptr; aptr++) {
		rc = toupper(*aptr) - '0';
		if (rc > 9) rc -= 7;
		if (rc < 0) rc = 0;
		if (rc > 15) rc = 15;
		value = 16*value + rc;
	}
	if (flags != NULL) *flags = value;
	return 0;
}

#ifdef LOCAL_TEST

/* ===========================================================================
-- Test routine
=========================================================================== */
int main(int argc, char *argv[]) {

	NUMATO *dio;
	int i, rc;
	char szTmp[256];
	unsigned int flags;

	if ( (dio = NumatoOpenDIO(5, &rc)) == NULL) {
		fprintf(stderr, "Numato open failed (rc = %d)\n", rc); fflush(stderr);
		return 3;
	}

	if (NumatoQuery(dio, "ver", szTmp, sizeof(szTmp))    == 0) { printf("VER: %s\n", szTmp); fflush(stdout); }
	if (NumatoQuery(dio, "id get", szTmp, sizeof(szTmp)) == 0) { printf("ID:  %s\n", szTmp); fflush(stdout); }
	NumatoQuery(dio, "id set 87654321", NULL, 0);
	if (NumatoQuery(dio, "id get", szTmp, sizeof(szTmp)) == 0) { printf("ID:  %s\n", szTmp); fflush(stdout); }
	NumatoQuery(dio, "id set 12345678", NULL, 0);
	if (NumatoQuery(dio, "id get", szTmp, sizeof(szTmp)) == 0) { printf("ID:  %s\n", szTmp); fflush(stdout); }

	for (i=0; i<10000; i++) {
		if (i%1000 == 0) { fprintf(stdout, "[%4.4d]\n", i); fflush(stdout); }
		rc = NumatoSetBit(dio, 0, TRUE);			/* 40 ms period (high to high) */
		rc = NumatoSetBit(dio, 0, FALSE);
	}		
	rc = NumatoSetBit(dio, 0, TRUE);
	rc = NumatoSetBit(dio, 1, TRUE);
	rc = NumatoSetBit(dio, 6, TRUE);
	rc = NumatoSetBit(dio, 7, TRUE);
	rc = NumatoReadAll(dio, &flags); printf("ReadAll: rc = %d  flags = %2.2x\n", rc, flags); fflush(stdout);

#if 0	/* The writeall command failes ... no idea why */
	rc = NumatoSetIOmask(dio, 0xFF); printf("SetIOmask: %d\n", rc); fflush(stdout);
	rc = NumatoSetIOdir(dio, 0x00);  printf("SetIOdir: %d\n", rc); fflush(stdout);
	rc = NumatoWriteAll(dio, 0xA6);  printf("WriteAll: %d\n", rc); fflush(stdout);
	rc = NumatoWriteAll(dio, 0xA6);  printf("WriteAll: %d\n", rc); fflush(stdout);
#endif

	return 0;
}

#endif /* LOCAL_TEST */
