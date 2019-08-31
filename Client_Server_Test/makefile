CFLAGS = /nologo /W3
SYSLIBS = wsock32.lib

ALL: client.exe server.exe

CLEAN: 
	rm *.exe *.obj

client.exe : client.c DCx_client.h DCx.h
	cl $(CFLAGS) client.c $(SYSLIBS)

server.exe : server.c DCx_client.h DCx.h
	cl $(CFLAGS) server.c $(SYSLIBS)
