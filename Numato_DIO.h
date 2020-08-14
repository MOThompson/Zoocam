typedef struct _NUMATO NUMATO;						/* Actual structure in Numato_DIO.c */

NUMATO *NumatoOpenDIO(int port, int *ierr);
int NumatoCloseDIO(NUMATO *dio);
int NumatoFlush(NUMATO *dio);
int NumatoQuery(NUMATO *dio, char *query, char *response, size_t max_len);
int NumatoSetIOdir(NUMATO *dio, unsigned int flags);
int NumatoSetIOmask(NUMATO *dio, unsigned int flags);
int NumatoSetBit(NUMATO *dio, int bit, BOOL on);
int NumatoQueryBit(NUMATO *dio, int bit, BOOL *on);
int NumatoReadAll(NUMATO *dio, unsigned int *flags);

/* THIS ROUTINE IS WRITTEN, BUT DOES NOT CURRENTLY WORK */
int NumatoWriteAll(NUMATO *dio, unsigned int flags);
