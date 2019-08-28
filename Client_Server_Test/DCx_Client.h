#ifndef _DCX_CLIENT_INCLUDED

#define	_DCX_CLIENT_INCLUDED

/* ===========================================================================
-- IT IS CRITICAL THAT THE VERSION BE MODIFEID anytime code changes and
-- would BREAK EXISTING COMPILATIONS.  Version is checked by the client
-- open routine, so as long as this changes, don't expect problems.
=========================================================================== */
#define	DCX_CLIENT_SERVER_VERSION	(1001)			/* Version of this code */

#ifndef	IS_DCX_SERVER

typedef int BOOL;

#define	DFLT_SERVER_IP_ADDRESS	"128.253.129.93"		/* "127.0.0.1" for loop-back */
#define	LOOPBACK_SERVER_IP_ADDRESS	"127.0.0.1"			/* Server on this computer */


/* ===========================================================================
-- Routine to open and initialize the socket to the DCx server
--
-- Usage: int Init_DCx_Client(char *IP_address);
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
int Init_DCx_Client(char *IP_address);

/* ===========================================================================
-- Routine to shutdown high level DCx remote socket server
--
-- Usage: void Shutdown_DCx_Client(void)
--
-- Inputs: none
--
-- Output: at moment, does nothing but may ultimately have a semaphore
--         to cleanly shutdown
--
-- Return:  0 if successful, !0 otherwise
=========================================================================== */
int Shutdown_DCx_Client(void);

/* ===========================================================================
--	Routine to return current version of this code
--
--	Usage:  int DCx_Query_Server_Version(void);
--         int DCx_Query_Client_Version(void);
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
int DCx_Remote_Query_Client_Version(void);
int DCx_Remote_Query_Server_Version(void);


/* ===========================================================================
--	Routine to return information on the camera
--
--	Usage:  int DCx_Remote_Get_Camera_Info(DCX_STATUS *status);
--
--	Inputs: status - pointer to variable to receive information
--		
--	Output: *status filled with information
--
-- Return: 0 if successful
=========================================================================== */
int DCx_Remote_Get_Camera_Info(DCX_STATUS *status);

/* ===========================================================================
--	Routine to acquire an image (local save)
--
--	Usage:  int DCx_Remote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);
--
--	Inputs: info - pointer to buffer to receive information about image
--         image - pointer to get malloc'd memory with the image itself
--                 caller responsible for releasing this memory
-- 
--	Output: info and image defined if new image obtained
--
-- Return: 0 if successful, other error indication
--         On error *image will be NULL and *info will be zero
--
-- Note: This is really 3 transactions with the server
--         (1) DCX_ACQUIRE_IMAGE   [captures the image]
--         (2) DCX_GET_IMAGE_INFO  [transmits information about image]
--         (3) DCX_GET_IMAGE_DATA  [transmits actual image bytes]
=========================================================================== */
int DCx_Remote_Acquire_Image(DCX_IMAGE_INFO *info, char **image);

#endif		/*	IS_DCx_SERVER */

#endif		/* _DCx_CLIENT_INCLUDED */
