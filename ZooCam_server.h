int Init_ZooCam_Server(void);
int Shutdown_ZooCam_Server(void);

#define	ZOOCAM_SERVER_WAIT	(30000)		/* 30 second time-out */

/* Typedef's */
typedef enum _RING_ACTION {RING_GET_INFO=0, RING_GET_SIZE=1, RING_SET_SIZE=2, RING_GET_ACTIVE_CNT=3} RING_ACTION;

