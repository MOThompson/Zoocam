#################################################################
# REMEMBER ... HAVE TO GO TO x64 using "cl64.bat" TO COMPILE 
#################################################################
CFLAGS = /nologo /W3 /WX- /O2 /Ob1 /GF /MT /GS /Gy /fp:precise /Zc:forScope /Gd
LFLAGS = /nologo /subsystem:windows
RFLAGS = /nologo
SYSLIBS = wsock32.lib user32.lib 

ALL: client.exe server.exe

CLEAN: 
	rm *.exe *.obj

client.exe : client.c DCx_client.h DCx.h
	cl $(CFLAGS) client.c $(SYSLIBS)

server.exe : server.c DCx_client.h DCx.h
	cl $(CFLAGS) server.c $(SYSLIBS)
