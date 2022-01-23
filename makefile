#################################################################
# REMEMBER ... May have to undo a "cl64.bat" command
#################################################################
CFLAGS = /nologo /W3 /WX- /O2 /Ob1 /GF /MD /GS /Gy /fp:precise /Zc:forScope /Gd
LFLAGS = /nologo /subsystem:windows
RFLAGS = /nologo

DCx_LIB = \code\lab\DCx\Lib\uc480.lib
NI488_LIB = "c:\Program Files (x86)\National Instruments\Shared\ExternalCompilerSupport\C\lib32\msvc\ni4882.obj"
NI488_LIB = \code\lab\LaserRange\ni4882.obj

SYSLIBS = wsock32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib advapi32.lib vfw32.lib shell32.lib

TL_SDK_INCLUDE = -I/code/lab/DCx/tl_sdk\include -I/code/lab/DCx/tl_sdk/load_dll_helpers

OBJS = DCx.obj DCx_server.obj tl.obj numato_dio.obj focus_client.obj win32ex.obj graph.obj ki224.obj server_support.obj tl_camera_sdk_load.obj tl_mono_to_color_processing_load.obj timer.obj

# server.exe  -- removed since must now be able to access the dialog box
ALL: client.exe DCxLive.exe

INSTALL: z:\lab\exes\DCxLive.exe

CLEAN: 
	rm *.exe *.obj *.res

client.exe : DCx_client.c DCx_client.h wnd.h server_support.obj server_support.h
	cl -Feclient.exe -DLOCAL_CLIENT_TEST $(CFLAGS) DCx_client.c server_support.obj $(SYSLIBS)

server.exe : server_test.c DCx_server.obj server_support.obj server_support.h DCx_server.h DCx_client.h 
	cl -Feserver.exe $(CFLAGS) server_test.c DCx_server.obj server_support.obj $(SYSLIBS)

DCx.obj : DCx.c wnd.h DCx_client.h uc480.h resource.h win32ex.h graph.h
	cl -c  $(TL_SDK_INCLUDE) -DSTANDALONE $(CFLAGS) DCx.c

tl.obj : tl.c tl.h timer.h
	cl -c $(TL_SDK_INCLUDE) $(CFLAGS) tl.c

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

tl_camera_sdk_load.obj : tl_camera_sdk_load.c
	cl -I..\tl_sdk\include -c $(CFLAGS) tl_camera_sdk_load.c

tl_mono_to_color_processing_load.obj : tl_mono_to_color_processing_load.c
	cl -I..\tl_sdk\include -c $(CFLAGS) tl_mono_to_color_processing_load.c

timer.obj : timer.h
	cl -c $(CFLAGS) timer.c

DCxLive.exe : $(OBJS) DCx.res
	cl $(CFLAGS) -FeDCxLive.exe $(OBJS) DCx.res $(DCx_LIB) $(NI488_LIB) $(SYSLIBS)

DCx.res : DCx.rc resource.h
	rc $(RFLAGS) DCx.rc

# Distribution for the lab use
z:\lab\exes\DCxLive.exe : DCxLive.exe
	copy $** $@

# ---------------------------------------------------------------------------
# Support modules 
# ---------------------------------------------------------------------------
timer.h : \code\lab\TimeFncs\timer.h
	copy $** $@

timer.c : \code\lab\TimeFncs\timer.c
	copy $** $@

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

tl_mono_to_color_processing_load.h : ..\TL_SDK\load_dll_helpers\tl_mono_to_color_processing_load.h
	copy $** $@

tl_mono_to_color_processing_load.obj : tl_mono_to_color_processing_load.h

win32ex.obj : win32ex.h

ki224.obj : ki224.h win32ex.h

tl.obj : tl.h

dcx.obj : wnd.h dcx_server.h uc480.h resource.h win32ex.h graph.h focus_client.h ki224.h tl.h

server_test.obj : wnd.h
