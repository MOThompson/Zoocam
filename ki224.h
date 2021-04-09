typedef struct _KI224_INFO {
	int active;							/* Is unit initialized and active		*/
	HWND hdlg;							/* Handle of an active Keith224 dialog	*/
	int board, address;				/* GPIB board and address					*/
	int handle;							/* GPIB handle for accessing the board */
	double V_set, I_set;				/* V and I values set in dialog box		*/
	double V, I;						/* Actual V and I values from status	*/
	BOOL enabled;						/* True if output known to be enabled	*/
} KI224_INFO;

KI224_INFO *ki224_info;

INT_PTR CALLBACK Keith224DlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

#define	KEITH224_DISABLE	(0)
#define	KEITH224_ENABLE	(1)
#define	KEITH224_QUERY		(2)
BOOL Keith224_Output(int action);	/* 0 => off, 1 => on, otherwise no change; returns state */
