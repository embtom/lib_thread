/* ****************************************************************************************************
 * lib_thread.c within the following project: lib_thread
 *
 *  compiler:   GNU Tools ARM LINUX
 *  target:     armv6
 *  author:	    Tom
 * ****************************************************************************************************/

/* ****************************************************************************************************/

/*
 *	******************************* change log *******************************
 *  date			user			comment
 * 	06 April 2015			Tom			- creation of lib_thread.c
 *  21 April 2015			Tom			- add of comments
 *
 */


#ifndef SH_LIB_THREAD_LIB_THREAD_WAKEUP_H_
#define SH_LIB_THREAD_LIB_THREAD_WAKEUP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct internal_wakeup *	wakeup_hdl_t;	  /* opaque pointer to a wakeup object handle */


int lib_thread__wakeup_init(void);
int lib_thread__wakeup_cleanup(void);
int lib_thread__wakeup_create(wakeup_hdl_t *_wu_obj, unsigned _interval);
int lib_thread__wakeup_destroy(wakeup_hdl_t *_wu_obj);
int lib_thread__wakeup_wait(wakeup_hdl_t *_wu_obj);




#ifdef __cplusplus
}
#endif

#endif /* SH_LIB_THREAD_LIB_THREAD_WAKEUP_H_ */
