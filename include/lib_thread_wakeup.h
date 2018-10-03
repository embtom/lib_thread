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

typedef struct internal_wakeup* wakeup_hdl_t;	  /* opaque pointer to a wakeup object handle */

/* ************************************************************************//**
 * \brief	Init of the wakeup component
 *
 * Attention:
 * At the POSIX environment have to be called at the start of the "main"
 * because the signal mask are modified
 *
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-ESTD_EBUSY		There are still some wakeup objects currently in use
 * ****************************************************************************/
int lib_thread__wakeup_init(void);

/* ************************************************************************//**
 * \brief	Cleanup wakeup component of lib_thread
 *
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-ESTD_EBUSY		There are still some wakeup objects currently in use
 * ****************************************************************************/
int lib_thread__wakeup_cleanup(void);

/* ************************************************************************//**
 * \brief	Create a new wakeup object with the specified interval
 *
 * The utilized time base is a strict monotonic system clock, and so is not
 * affected when e.g. adjusting the local date and time of the system.
 *
 * \param	*_wu_obj	[out]	pointer to handle of the wakeup object (will be allocated; only valid on successful return)
 * \param	_interval			wakeup interval in ms (must not be 0)
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_interval is 0
 *			-ESTD_NOMEM		Insufficient memory available to initialize the wakeup object
 *			-ESTD_AGAIN		All available signals or timers are currently in use
 * ****************************************************************************/
int lib_thread__wakeup_create(wakeup_hdl_t *_wu_obj, unsigned _interval);


/* ************************************************************************//**
 * \brief	Destroy an initialized wakeup object
 *
 * \param	*_wu_obj	[in/out]	pointer to handle of the wakeup object (is only destroyed on successful return)
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_wu_obj is invalid
 * ****************************************************************************/
int lib_thread__wakeup_destroy(wakeup_hdl_t *_wu_obj);

/* ************************************************************************//**
 * \brief	Destroy an initialized wakeup object
 *
 * \param	*_wu_obj [in/out]	pointer to handle of the wakeup object (is only destroyed on successful return)
 * \param	_interval			wakeup interval in ms (must not be 0)
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_wu_obj is invalid
 * ****************************************************************************/
int lib_thread__wakeup_setinterval(wakeup_hdl_t _wu_obj, unsigned _interval);

/* ************************************************************************//**
 * \brief	Wait on the specified wakeup object
 *
 * This function waits until the specified wakeup object's timeout interval
 * has elapsed. Since this is not synchronized with the actual function call,
 * the call may unblock virtually instantly, particularly when being executed
 * for the first time. Hence this function should be called at the top of
 * a while or for loop.
 *
 * If the wakeup object will be destroyed the wakeup wait routine unblocks
 * with -ESTD_INTR
 *
 *
 * \param	*_wu_obj	[in/out]	pointer to handle of the wakeup object
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-ESTD_INTR		The wakeup object is destroyed
 * ****************************************************************************/
int lib_thread__wakeup_wait(wakeup_hdl_t _wu_obj);


#ifdef __cplusplus
}
#endif

#endif /* SH_LIB_THREAD_LIB_THREAD_WAKEUP_H_ */
