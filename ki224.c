/* ki224.c */

#define	DEFAULT_GPIB_CHAN	(19)
#define	GPIB_BOARD_ID		(0)		/* Which GPIB interface to use (0 is first, 1 second, ...) */

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
#include <string.h>						/* for manipulating several kinds of strings */
#include <time.h>
#include <direct.h>
#include <math.h>
#include <stdint.h>

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
#include "ni4882.h"						/* GPIB support */
#include "resource.h"
#include "win32ex.h"
#include "ki224.h"

/* ------------------------------- */
/* My local typedef's and defines  */
/* ------------------------------- */
#define	LinkDate	(__DATE__ "  "  __TIME__)

#define	nint(x)	(((x)>0) ? ( (int) (x+0.5)) : ( (int) (x-0.5)) )

/* ------------------------------- */
/* My external function prototypes */
/* ------------------------------- */

/* ------------------------------- */
/* My internal function prototypes */
/* ------------------------------- */
static int ibwrtex(char *text, int msSleep);		/* Helper routines for GPIB access */
static int ibrdex(char *szBuf, int len);
static int OpenKeithley(HWND hdlg, int gpib_chan);

/* ------------------------------- */
/* My usage of other external fncs */
/* ------------------------------- */

/* ------------------------------- */
/* My share of global vars         */
/* ------------------------------- */
static KI224_INFO info = {
	FALSE,											/* active */
	NULL,												/* Handle to dialog window */
	GPIB_BOARD_ID, DEFAULT_GPIB_CHAN,		/* board and gpib address */
	0,													/* handle to the GPIB device */
	1.0, 0.0,										/* Set values for compliance and voltage */
	0.0, 0.0											/* Current readings */
};
KI224_INFO *ki224_info = &info;

/* ------------------------------- */
/* Locally defined global vars     */
/* ------------------------------- */
static HWND hdlgMain = NULL;					/* Handle to primary dialog box */

static HINSTANCE hInstance=NULL;

static HBITMAP m_GreenLED  = NULL;
static HBITMAP m_RedLED    = NULL;
static HBITMAP m_YellowLED = NULL;
static HBITMAP m_GrayLED   = NULL;

/* ===========================================================================
-- Standalone module (WinMain()) to initialize the dialog box
--
-- Usage: Standard call from operating systems for main program
=========================================================================== */
#ifdef STANDALONE
int WINAPI WinMain(HINSTANCE hThisInst, HINSTANCE hPrevInst, LPSTR lpszArgs, int nWinMode) {

	/* If not done, make sure we are loaded.  Assume safe to call multiply */
	InitCommonControls();
	LoadLibrary("RICHED20.DLL");

	/* Load the class for the graph and bitmap windows */
	/* And show the dialog box */
	DialogBox(hThisInst, "IDD_KEITHLEY_224", HWND_DESKTOP, (DLGPROC) Keith224DlgProc);

	return 0;
}
#endif


/* ===========================================================================
-- Update the status
=========================================================================== */
void UpdateStatus(HWND hdlg) {
	char szBuf[256], *aptr;
	unsigned char status_byte;
	
	if (ki224_info->handle > 0) {
		ibwrtex("G1U0X", 0);
		ibrdex(szBuf, sizeof(szBuf));
		SetDlgItemText(hdlg, IDT_STATUS, szBuf);

		SendDlgItemMessage(hdlg, ID_LED, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (szBuf[1] == '0' ? m_RedLED : m_GreenLED) );

		ibwrtex("G1X", 0);
		ibrdex(szBuf, sizeof(szBuf));
		SetDlgItemText(hdlg, IDT_LEVELS, szBuf);

		ki224_info->I = strtod(szBuf, &aptr);
		SetDlgItemDouble(hdlg, IDT_CURRENT, "%.3g", ki224_info->I*1000);
		if (*aptr == ',') aptr++;
		ki224_info->V = strtod(aptr, &aptr);
		SetDlgItemDouble(hdlg, IDT_VOLTAGE, "%.1f", ki224_info->V);

		ibrsp(ki224_info->handle, &status_byte);
		sprintf_s(szBuf, sizeof(szBuf), "0x%2.2x", status_byte);
		SetDlgItemText(hdlg, IDT_SRQ, szBuf);
		SetDlgItemCheck(hdlg, IDC_B0,   (status_byte & 0x20) && (status_byte & 0x03));
		SetDlgItemCheck(hdlg, IDC_B1, ! (status_byte & 0x20) && (status_byte & 0x01));
		SetDlgItemCheck(hdlg, IDC_B2, ! (status_byte & 0x20) && (status_byte & 0x02));
		SetDlgItemCheck(hdlg, IDC_B3, ! (status_byte & 0x20) && (status_byte & 0x04));
		SetDlgItemCheck(hdlg, IDC_B4, ! (status_byte & 0x20) && (status_byte & 0x08));

		ibloc(ki224_info->handle);
	}
	return;
}

/* ===========================================================================

=========================================================================== */
#define	TIMER_STATUS_UPDATE	(1)

INT_PTR CALLBACK Keith224DlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *rname = "Keith224DlgProc";

	BOOL rcode, mod;
	int i, wID, wNotifyCode;
	char *aptr, szBuf[256];

	/* List of controls which will respond to <ENTER> with a WM_NEXTDLGCTL message */
	/* Make <ENTER> equivalent to losing focus */
	static int DfltEnterList[] = {					
		IDV_CURRENT, IDV_VOLTAGE,
		IDC_STATIC };

	static CB_INT_LIST GPIB_List[] = {
		{"none", -1}, 
		{"  1",  1}, {"  2",  2}, {"  3",  3}, {"  4",  4}, {"  5",  5}, {"  6",  6}, {"  7",  7}, {"  8",  8},
		{"  9",  9}, {" 10", 10}, {" 11", 11}, {" 12", 12}, {" 13", 13}, {" 14", 14}, {" 15", 15}, {" 16", 16},
		{" 17", 17}, {" 18", 18}, {" 19", 19}, {" 20", 20}, {" 21", 21}, {" 22", 22}, {" 23", 23}, {" 24", 24},
		{" 25", 25}, {" 26", 26}, {" 27", 27}, {" 28", 28}, {" 29", 29}, {" 30", 30}, {" 31", 31}
	};

	static int ControlList[] = {
		IDB_STATUS_UPDATE, IDC_STATUS_ON,
		IDV_CURRENT, IDT_CURRENT, IDB_SET_I, IDV_VOLTAGE, IDT_VOLTAGE, IDB_SET_V, 
		IDB_CURRENT_ON, IDB_CURRENT_OFF, ID_LED,
		IDT_STATUS, IDT_LEVELS, IDT_SRQ, 
		IDC_B0, IDC_B1, IDC_B2, IDC_B3, IDC_B4
	};

	HWND hwndTest;
	int *hptr;
	double rval;

/* The message loop */
	switch (msg) {

		case WM_INITDIALOG:
			DlgCenterWindow(hdlg);				/* Have start in upper right */

			if (m_GreenLED == NULL) {
				m_GreenLED  = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_GREEN_LED),  IMAGE_BITMAP, 20, 20, 0L);
				m_RedLED    = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_RED_LED),    IMAGE_BITMAP, 20, 20, 0L);
				m_YellowLED = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_YELLOW_LED), IMAGE_BITMAP, 20, 20, 0L);
				m_GrayLED   = (HBITMAP) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_GRAY_LED),   IMAGE_BITMAP, 20, 20, 0L);
			}

			/* Initialize GPIB address, etc. */
			ComboBoxFillIntList(hdlg, IDC_GPIB, GPIB_List, CB_COUNT(GPIB_List));
			ComboBoxSetByIntValue(hdlg, IDC_GPIB, ki224_info->address);
			SetDlgItemDouble(hdlg, IDV_CURRENT, "%.3g", ki224_info->I_set*1000.0);
			SetDlgItemDouble(hdlg, IDV_VOLTAGE, "%.1f", ki224_info->V_set);

			/* Set current state based on the ki224_info block */
			SetDlgItemCheck(hdlg, IDC_CONNECT, ki224_info->active);
			for (i=0; i<sizeof(ControlList)/sizeof(*ControlList); i++) EnableDlgItem(hdlg, ControlList[i], ki224_info->active);
			SendDlgItemMessage(hdlg, ID_LED, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (ki224_info->active ? (ki224_info->enabled ? m_GreenLED : m_RedLED) : m_YellowLED) );

			/* Store the current handle for direct responses */
			ki224_info->hdlg = hdlg;
			rcode = TRUE; break;

		case WM_CLOSE:
			EndDialog(hdlg,0);
			ki224_info->hdlg = NULL;
			rcode = TRUE; break;

		case WM_TIMER:
			if (wParam == TIMER_STATUS_UPDATE && ki224_info->active) UpdateStatus(hdlg);
			rcode = TRUE; break;

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

		case WM_COMMAND:
			wID = LOWORD(wParam);									/* Control sending message	*/
			wNotifyCode = HIWORD(wParam);							/* Type of notification		*/

			rcode = FALSE;												/* Assume we don't process */
			switch (wID) {
				case IDOK:												/* Default response for pressing <ENTER> */
					hwndTest = GetFocus();							/* Just see if we use to change focus */
					for (hptr=DfltEnterList; *hptr!=IDC_STATIC; hptr++) {
						if (GetDlgItem(hdlg, *hptr) == hwndTest) {
							PostMessage(hdlg, WM_NEXTDLGCTL, 0, 0L);
							break;
						}
					}
					rcode = TRUE; break;

				case IDCANCEL:
					SendMessage(hdlg, WM_CLOSE, 0, 0);
					rcode = TRUE; break;

				case IDC_GPIB:
					if (CBN_SELCHANGE == wNotifyCode) {
						ki224_info->address = ComboBoxGetIntValue(hdlg, IDC_GPIB);
					}
					rcode = TRUE; break;

				case IDV_CURRENT:
					if (EN_KILLFOCUS == wNotifyCode) {
						GetDlgItemText(hdlg, wID, szBuf, sizeof(szBuf));
						if (*szBuf == '\0') {
							rval = 0.0;
						} else {
							mod = FALSE;
							rval = strtod(szBuf, &aptr);
							if (rval >  50.0) { mod = TRUE; rval =  50.0; }
							if (rval < -50.0) { mod = TRUE; rval = -50.0; }
							if (*aptr != '\0' || mod) SetDlgItemDouble(hdlg, wID, "%.3g", rval);
						}
					}
					rcode = TRUE; break;

				case IDB_SET_I:
					if (BN_CLICKED == wNotifyCode) {
						if (! ki224_info->active) {
							Beep(300,200);
						} else {
							rval = GetDlgItemDouble(hdlg, IDV_CURRENT);
							sprintf_s(szBuf, sizeof(szBuf), "R0I%gX", rval*1E-3);
							ki224_info->I_set = rval*1E-3;
							ibwrtex(szBuf, 20);
							UpdateStatus(hdlg);
						}
					}
					rcode = TRUE; break;

				case IDV_VOLTAGE:
					if (EN_KILLFOCUS == wNotifyCode) {
						GetDlgItemText(hdlg, wID, szBuf, sizeof(szBuf));
						if (*szBuf == '\0') {
							rval = 0.0;
						} else {
							mod = FALSE;;
							rval = strtod(szBuf, &aptr);
							if (rval <  0.0) { mod = TRUE; rval = -rval; }
							if (rval == 0.0) { mod = TRUE; rval = 1.0; }
							if (rval >  6.0) { mod = TRUE; rval = 6.0; }
							if (*aptr != '\0' || mod) SetDlgItemDouble(hdlg, wID, "%.1f", rval);
						}
					}
					rcode = TRUE; break;

				case IDB_SET_V:
					if (BN_CLICKED == wNotifyCode) {
						if (! ki224_info->active) {
							Beep(300,200);
						} else {
							rval = GetDlgItemDouble(hdlg, IDV_VOLTAGE);
							sprintf_s(szBuf, sizeof(szBuf), "V%.3fX", rval);
							ibwrtex(szBuf, 20);
							ki224_info->V_set = rval;
							UpdateStatus(hdlg);
						}
					}
					rcode = TRUE; break;

				case IDC_STATUS_ON:
					if (ki224_info->active) {
						if (GetDlgItemCheck(hdlg, wID)) {
							SetTimer(hdlg, TIMER_STATUS_UPDATE, 1000, NULL);
						} else {
							KillTimer(hdlg, TIMER_STATUS_UPDATE);
						}
					} else {
						SetDlgItemCheck(hdlg, IDC_STATUS_ON, FALSE);
					}
					rcode = TRUE; break;
					
				case IDB_STATUS_UPDATE:
					if (BN_CLICKED == wNotifyCode) UpdateStatus(hdlg);
					rcode = TRUE; break;
					
				case IDC_CONNECT:
					if (GetDlgItemCheck(hdlg, IDC_CONNECT) && OpenKeithley(hdlg, ki224_info->address) == 0) {
						ki224_info->active = TRUE;
						for (i=0; i<sizeof(ControlList)/sizeof(*ControlList); i++) EnableDlgItem(hdlg, ControlList[i], TRUE);
						EnableDlgItem(hdlg, IDC_GPIB, FALSE);
						SetDlgItemCheck(hdlg, IDC_STATUS_ON, FALSE);
						UpdateStatus(hdlg);
					} else {
						ki224_info->active  = FALSE;								/* No longer active */
						ki224_info->enabled = FALSE;								/* No longer know the state */
						ki224_info->V = ki224_info->I = 0;
						if (ki224_info->handle > 0) ibonl(ki224_info->handle, 1);
						ki224_info->handle = -1;

						KillTimer(hdlg, TIMER_STATUS_UPDATE);
						for (i=0; i<sizeof(ControlList)/sizeof(*ControlList); i++) EnableDlgItem(hdlg, ControlList[i], FALSE);
						EnableDlgItem(hdlg, IDC_GPIB, TRUE);
					}
					rcode = TRUE; break;

				case IDB_CURRENT_ON:
				case IDB_CURRENT_OFF:
					if (BN_CLICKED == wNotifyCode) {
						if (! ki224_info->active) {										/* Only if active */
							Beep(300,200);
						} else {
							ki224_info->enabled = (wID == IDB_CURRENT_ON) ;
							ibwrtex(ki224_info->enabled ? "F1X" : "F0X", 20);
							ibloc(ki224_info->handle);
							SendDlgItemMessage(hdlg, ID_LED, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (ki224_info->enabled ? m_GreenLED : m_RedLED) );
						}
					}
					rcode = TRUE; break;

				/* Know to be unused notification codes (handled otherwise) */
				case IDT_SRQ:
				case IDT_STATUS:
				case IDT_LEVELS:
				case IDT_CURRENT:
				case IDT_VOLTAGE:
				case IDC_B0:
				case IDC_B1:
				case IDC_B2:
				case IDC_B3:
				case IDC_B4:
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
-- External routine to allow the output to be turned on / off
--
-- Usage: BOOL Keith224_Output(int action);
--
-- Inputs: action - 0 = KEITH224_DISABLE ==> turn current off
--                  1 = KEITH224_ENABLE  ==> turn current on (assuming unit initialized)
--                  n = KEITH224_QUERY   ==> just return current status
--
-- Output: Sends F1 or F0 as requested to Keithley via GPIB
--
-- Return: current "known" output enable status
=========================================================================== */
BOOL Keith224_Output(int action) {

	if (ki224_info->active && (action == 0 || action == 1)) {
		ki224_info->enabled = action != 0;					/* New status */
		ibwrtex(ki224_info->enabled ? "F1X" : "F0X", 20);
		ibloc(ki224_info->handle);
		if (ki224_info->hdlg != NULL) {
			SendDlgItemMessage(ki224_info->hdlg, ID_LED, STM_SETIMAGE, (WPARAM) IMAGE_BITMAP, (LPARAM) (ki224_info->enabled ? m_GreenLED : m_RedLED) );
		}
	}
	return ki224_info->enabled;
}


/* ===========================================================================
	-- Helper routines for GPIB access
	=========================================================================== */
#if 0
/* Some tests / initialization for first time */

ibwrtex("G1U0X", 0);
ibrdex(szBuf, sizeof(szBuf));
fprintf(stderr, "Status (G1U0): \"%s\"\n", szBuf); fflush(stderr);
SetDlgItemText(hdlgMain, IDT_STATUS, szBuf);

ibwrtex("U1X", 0);
ibrdex(szBuf, sizeof(szBuf));
fprintf(stderr, "Status (U1): \"%s\"\n", szBuf); fflush(stderr);

ibwrtex("G1X", 0);
ibrdex(szBuf, sizeof(szBuf));
fprintf(stderr, "Return for G1 \"%s\"\n", szBuf); fflush(stderr);
#endif

static int OpenKeithley(HWND hdlg, int gpib_chan) {
	char szBuf[256], errmsg[256];

	/* Validate the gpib channel */
	if (ki224_info->address <= 0 || ki224_info->address > 31) {
		ki224_info->handle = -1;
		return 1;
	}

	ki224_info->handle = ibdev(ki224_info->board, ki224_info->address, 0, T1s, 1, 0x10);
	fprintf(stderr, "\nPort: %d  handle: %d\n", ki224_info->address, ki224_info->handle); fflush(stderr);
	if (ki224_info->handle <= 0) {
		sprintf_s(errmsg, sizeof(errmsg),
					 "Device at GPIB address %d failed to open\n"
					 "   Return hande: %d\n",
					 ki224_info->address, ki224_info->handle);
		MessageBox(hdlg, errmsg, "Error opening Keithley 224", MB_OK | MB_ICONERROR);
		ki224_info->handle = -1;
		return 2;
	}

	/* Query a G0U0 status and look for the leading 224 to indicate correct unit */
	ibwrtex("G0U0X", 0);
	ibrdex(szBuf, sizeof(szBuf));
	fprintf(stderr, "Status (G0U0): \"%s\"\n", szBuf); fflush(stderr);
	if (strncmp(szBuf, "224", 3) != 0 && strncmp(szBuf, "220", 3) != 0) {
		sprintf_s(errmsg, sizeof(errmsg),
					 "Device at GPIB address %d does not\n"
					 "appear to be a Keithley 224 current source\n"
					 "Returned string from G0U0 was\n"
					 "   %s\n"
					 "which did not start with the expected 224 digits\n",
					 ki224_info->address, szBuf);
		MessageBox(hdlg, errmsg, "Error opening Keithley 224", MB_OK | MB_ICONERROR);
		ibonl(ki224_info->handle, 1);
		ki224_info->handle = -1;
		return 3;
	}

	return 0;
}


static int ibwrtex(char *text, int msSleep) {
	char szBuf[256];
	int rc;

	if (ki224_info->handle > 0) {
		sprintf_s(szBuf, sizeof(szBuf), "%s\r\n", text);
		rc = ibwrt(ki224_info->handle, szBuf, strlen(szBuf));
		if (msSleep > 0) Sleep(msSleep);
	} else {
		rc = -1;
	}
	return rc;
}

static int ibrdex(char *szBuf, int len) {
	int rc;
	char *aptr;

	*szBuf = '\0';
	if ( ki224_info->handle > 0) {
		rc = ibrd(ki224_info->handle, szBuf, len);
		if ( (aptr = strchr(szBuf, '\n')) != NULL) *aptr = '\0';
		if ( (aptr = strchr(szBuf, '\r')) != NULL) *aptr = '\0';
	} else {
		rc = -1;
	}

	return rc;
}

