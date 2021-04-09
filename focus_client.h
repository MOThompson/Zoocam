#ifndef _FOCUS_CLIENT_INCLUDED

#define	_FOCUS_CLIENT_INCLUDED

/* ===========================================================================
-- IT IS CRITICAL THAT THE VERSION BE MODIFEID anytime code changes and
-- would BREAK EXISTING COMPILATIONS.  Version is checked by the client
-- open routine, so as long as this changes, don't expect problems.
=========================================================================== */
#define	FOCUS_CLIENT_SERVER_VERSION	(1001)			/* Version of this code */

/* =============================
-- Port that the server runs
============================= */
#define	FOCUS_ACCESS_PORT		(1911)				/* Port for client/server connections */

#define	DFLT_SERVER_IP_ADDRESS	"128.253.129.93"		/* "127.0.0.1" for loop-back */
#define	LOOPBACK_SERVER_IP_ADDRESS	"127.0.0.1"			/* Server on this computer */

typedef int32_t BOOL32;

#pragma pack(4)
typedef struct _POSN3D {
	double x,y,z;
} POSN3D;
#pragma pack()

#pragma pack(4)
typedef struct _TESS_PT {
	double x,y,z;						/* X,Y and focus positions */
	BOOL32 valid;						/* True if point is "calibrated" */
} _TESS_PT;
#pragma pack()

/* List of the allowed requests */
#define SERVER_END							(0)		/* Shut down server (please don't use) */
#define FOCUS_QUERY_VERSION				(1)		/* Version 1001 for now */
#define FOCUS_QUERY_SAMPLE_ID				(2)		/* String ID of the sample (eg. 18F64) */
#define FOCUS_SET_SAMPLE_ID				(3)
#define FOCUS_QUERY_SAMPLE_TEXT			(4)		/* String description of sample (eg. Bi2O3/Al2O3 binary spread) */
#define FOCUS_SET_SAMPLE_TEXT				(5)
#define FOCUS_QUERY_POSN					(6)		/* Return current x,y,z position */
#define FOCUS_QUERY_FOCUS					(7)		/* Return focus corresponding to x,y of a POSN3D */
#define FOCUS_GOTO_POSN						(8)		/* Goto specified x,y,z position */
	#define FOCUS_POSN_NO_CHANGE			(-999)	/* If coordinate is -999, don't change the value */
	#define FOCUS_POSN_AUTO_FOCUS			(-998)	/* If z (focus) set to -998, autofocus based on the wafer calibration map */
#define FOCUS_QUERY_SPECIAL				(9)		/* Query POSN3d information about special points (option) */
#define FOCUS_SET_SPECIAL					(10)		/* Set option special position (mirror, Si, blank) */
#define FOCUS_GOTO_SPECIAL					(11)		/* Goto one of the special positions (option) */
#ifndef REF_BLANK
	#define	REF_BLANK	(0)							/* Index of the blank (off stage) position */
	#define	REF_MIRROR	(1)							/* Index of the mirror reference position */
	#define	REF_SILICON	(2)							/* Index of the silicon reference position */
	#define	REF_POSN_3	(3)							/* Index for third position */
	#define	REF_POSN_4	(4)							/* Index for fourth position */
#endif

#define FOCUS_QUERY_GRID_TYPE				(12)		/* Return type of sample grid and diameter */
	#define GRID_WAFER_9PT		(0)					/* Circular wafer pattern grids */ 
	#define GRID_WAFER_25PT		(1)					/* Make sure matches focus.h TESS_TYPE */
	#define GRID_WAFER_57PT		(2)					/* Currently only 0, 1, 4, 5 and 8 active */
	#define GRID_WAFER_121PT	(3)
	#define GRID_PLATE_9PT		(4)					/* Square plate pattern grid */ 
	#define GRID_PLATE_25PT		(5) 
	#define GRID_PLATE_36PT		(6) 
	#define GRID_PLATE_49PT		(7)
	#define GRID_LITHO_9PT		(8)				 	/* Lithographically patterned spots */
#define FOCUS_SELECT_GRID_TYPE			(13)		/* Specify type of sample grid and diameter */
#define FOCUS_QUERY_SAMPLE_GRID			(14)		/* Return x,y,z list of wafer calibration points */
#define FOCUS_SET_SAMPLE_GRID				(15)
#define FOCUS_GOTO_SAMPLE_GRID			(16)		/* Go to a specific point within the wafer list */

#define FOCUS_QUERY_Z_MOTOR_STATUS		(17)		/* Query status bits for the motor */
#define FOCUS_QUERY_Z_MOTOR_ENGAGE		(18)		/* Is Z-motor currently engaged */
#define FOCUS_SET_Z_MOTOR_ENGAGE			(19)		/* Engage motor if request.option != 0 */
#define FOCUS_QUERY_Z_MOTOR_POSN			(20)		/* Query position of the Z motor */
#define FOCUS_SET_Z_MOTOR_POSN			(21)		/* Set Z-motor to specified position (return immediately) */
#define FOCUS_SET_Z_MOTOR_POSN_WAIT		(22)		/* Set Z-motor to specified position (return when done) */

/* definitions of bits returned from the FM_MOTOR structure (FM_ACTION) */
#define	FM_MOTOR_STATUS_ACTIVE		(0x0001)		/* motor is active */
#define	FM_MOTOR_STATUS_ENGAGED		(0x0002)		/* motor is engaged */
#define	FM_MOTOR_STATUS_HOMING		(0x0004)		/* motor is currently homing */
#define	FM_MOTOR_STATUS_HOMED		(0x0008)		/* motor has been homed */
#define	FM_MOTOR_STATUS_MOVING		(0x0010)		/* motor is currently moving */
#define	FM_MOTOR_STATUS_SWEEP		(0x0100)		/* motor is sweeping through focus */
#define	FM_MOTOR_STATUS_INVALID		(0xF000)		/* status word is invalid (error) */

/* ===========================================================================
-- Routine to open and initialize the socket to the Focus server
--
-- Usage: int Init_Focus_Client(char *IP_address);
--
-- Inputs: IP_address - IP address in normal form.  Use "127.0.0.1" for loopback test
--                      if NULL, uses DFLT_SERVER_IP_ADDRESS
--
-- Output: Creates MUTEX semaphores, opens socket, sets atexit() to ensure closure
--
-- Return:  0 - successful
--          1 - unable to create semaphores for controlling access to hardware
--          2 - unable to open the sockets (one or the other)
--
-- Notes: Must be called before any attempt to communicate across the socket
=========================================================================== */
int Init_Focus_Client(char *IP_address);

/* ===========================================================================
-- Routine to shutdown high level Focus remote socket server
--
-- Usage: void Shutdown_Focus_Client(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_Focus_Client(void);

/* ===========================================================================
--	Routine to return current version of this code
--
--	Usage:  int Focus_Query_Server_Version(void);
--         int Focus_Query_Client_Version(void);
--
--	Inputs: none
--		
--	Output: none
--
-- Return: Integer version number.  
--
--	Notes: The verison number returned is that given in this code when
--        compiled. The routine simply returns this version value and allows
--        a program that may be running in the client/server model to verify
--        that the server is actually running the expected version.  Programs
--        should always call and verify the expected returns.
=========================================================================== */
int Focus_Remote_Query_Client_Version(void);
int Focus_Remote_Query_Server_Version(void);

/* ===========================================================================
--	Routine to return the status of the focus motor
--
--	Usage:  int Focus_Remote_Get_Focus_Status(int *status);
--
--	Inputs: status - pointer to variable to current status of the focus motor
--		
--	Output: *status - bitwise status flag
--
-- Return: 0 if successful
=========================================================================== */
int Focus_Remote_Get_Focus_Status(int *status);

/* ===========================================================================
--	Routine to return the position of the focus motor
--
--	Usage:  int Focus_Remote_Get_Focus_Posn(double *zposn);
--
--	Inputs: zposn - pointer to variable to current z position of the focus motor
--		
--	Output: *zposn - position
--
-- Return: 0 if successful
=========================================================================== */
int Focus_Remote_Get_Focus_Posn(double *zposn);

/* ===========================================================================
--	Routine to set the position of the focus motor
--
--	Usage:  int Focus_Remote_Set_Focus_Posn(double zposn, BOOL wait);
--
--	Inputs: zposn - z position to be requested from the motor 
--         wait  - if TRUE, wait for move to complete, otherwise return immediately
--		
--	Output: none
--
-- Return: 0 if successful
=========================================================================== */
int Focus_Remote_Set_Focus_Posn(double zposn, BOOL wait);

#endif		/* _FOCUS_CLIENT_INCLUDED */
