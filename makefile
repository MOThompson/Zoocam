#################################################################
# REMEMBER ... May have to undo a "cl64.bat" command
#################################################################
CFLAGS = /nologo /W3 /WX- /O2 /Ob1 /GF /MD /GS /Gy /fp:precise /Zc:forScope /Gd
LFLAGS = /nologo /subsystem:windows
RFLAGS = /nologo

DCx_LIB = \code\lab\DCx\Lib\uc480.lib 
NI488_LIB = "c:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib32\msvc\ni4882.obj"

SYSLIBS = wsock32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib vfw32.lib shell32.lib

# server.exe  -- removed since must now be able to access the dialog box
ALL: client.exe DCxLive.exe

INSTALL: z:\lab\exes\DCxLive.exe

CLEAN: 
	rm *.exe *.obj *.res

client.exe : DCx_client.c DCx_client.h server_support.obj server_support.h
	cl -Feclient.exe -DLOCAL_CLIENT_TEST $(CFLAGS) DCx_client.c server_support.obj $(SYSLIBS)

server.exe : server_test.c DCx_server.obj server_support.obj server_support.h DCx_server.h DCx_client.h 
	cl -Feserver.exe $(CFLAGS) server_test.c DCx_server.obj server_support.obj $(SYSLIBS)

DCx.obj : DCx.c uc480.h resource.h win32ex.h graph.h
	cl -c -DSTANDALONE $(CFLAGS) DCx.c

win32ex.obj : win32ex.c win32ex.h
	cl -c $(CFLAGS) win32ex.c

graph.obj : graph.c graph.h
	cl -c $(CFLAGS) graph.c

server_support.obj : server_support.c server_support.h
	cl -c $(CFLAGS) server_support.c

numato_dio.obj : numato_dio.c numato_dio.h
	cl -c $(CFLAGS) numato_dio.c

ki224.obj : ki224.c ki224.h
	cl -I\code\NI488.nt -c $(CFLAGS) ki224.c

DCxLive.exe : DCx.obj DCx_server.obj DCx.res numato_dio.obj focus_client.obj win32ex.obj graph.obj ki224.obj server_support.obj
	cl $(CFLAGS) -FeDCxLive.exe DCx.obj DCx_server.obj numato_dio.obj focus_client.obj win32ex.obj graph.obj ki224.obj server_support.obj DCx.res $(DCx_LIB) $(NI488_LIB) $(SYSLIBS)

DCx.res : DCx.rc resource.h
	rc $(RFLAGS) DCx.rc

# Distribution for the lab use
z:\lab\exes\DCxLive.exe : DCxLive.exe
	copy $** $@

# ---------------------------------------------------------------------------
# Support modules 
# ---------------------------------------------------------------------------
graph.h : \code\Window_Classes\graph\graph.h
	copy $** $@

graph.c : \code\Window_Classes\graph\graph.c
	copy $** $@

win32ex.h : \code\Window_Classes\win32ex\win32ex.h
	copy $** $@

win32ex.c : \code\Window_Classes\win32ex\win32ex.c
	copy $** $@

server_support.c : \code\lab\Server_Support\server_support.c
	copy $** $@

server_support.h : \code\lab\Server_Support\server_support.h
	copy $** $@

focus_client.c : \code\lab\sara\focus_client.c
	copy $** $@

focus_client.h : \code\lab\sara\focus_client.h
	copy $** $@

win32ex.obj : win32ex.h

ki224.obj : ki224.h win32ex.h

dcx.obj : dcx.h dcx_server.h uc480.h resource.h win32ex.h graph.h focus_client.h ki224.h

server_test.obj : dcx.h
