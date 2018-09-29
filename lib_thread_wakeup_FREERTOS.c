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
 *  21 April 2015			Tom			- add of comments anf logging messages
 *
 */

/* *******************************************************************
 * includes
 * ******************************************************************/

/* c-runtime */
#include <stdlib.h>

/* system */
#include <FreeRTOS.h>
#include <task.h>

/* frame */
#include <lib_convention__mem.h>
#include <lib_convention__errno.h>
#include <lib_log.h>

/* project*/
#include <lib_thread.h>
#include "lib_thread_wakeup.h"

/* *******************************************************************
 * defines
 * ******************************************************************/
#define M_DEV_LIB_THREAD_WHP_MODULE_ID 		"LIB_THD_WHP"


/* wakeup handle object structure */
struct internal_wakeup {
	mutex_hdl_t timer_lock;
	TaskHandle_t wakeupTask;
	TickType_t last_wait_time;
	TickType_t interval_time;
	unsigned int destroy;
};

/* *******************************************************************
 * (static) variables declarations
 * ******************************************************************/
static unsigned int s_number_open_wakeup_obj = 0;

/* *******************************************************************
 * Global Functions
 * ******************************************************************/

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
int lib_thread__wakeup_init(void)
{
	s_number_open_wakeup_obj = 0;
	return EOK;
}

/* ************************************************************************//**
 * \brief	Cleanup wakeup component of lib_thread
 *
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-ESTD_EBUSY		There are still some wakeup objects currently in use
 * ****************************************************************************/
int lib_thread__wakeup_cleanup(void)
{
	if (s_number_open_wakeup_obj) {
		return -ESTD_BUSY;
	}
	return EOK;
}

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
 *			-EINVAL		    _interval is 0
 *			-ESTD_NOMEM		Insufficient memory available to initialize the wakeup object
 *			-ESTD_AGAIN		All available signals or timers are currently in use
 * ****************************************************************************/
int lib_thread__wakeup_create(wakeup_hdl_t * _wu_obj, unsigned _interval)
{
	int line, ret;
	wakeup_hdl_t wu_obj;

	/* check arguments */
	if (_wu_obj == NULL){
		line = __LINE__;
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_interval == 0){
		line = __LINE__;
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	/* create thread handle on heap */
	wu_obj = (wakeup_hdl_t)alloc_memory(1, sizeof(**_wu_obj));
	if (wu_obj == NULL){
		line = __LINE__;
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	wu_obj->interval_time = pdMS_TO_TICKS(_interval);
	s_number_open_wakeup_obj++;
	*_wu_obj = wu_obj;
	return EOK;

	ERR_1:
	free_memory(wu_obj);

    ERR_0:
    msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
    return ret;

}

/* ************************************************************************//**
 * \brief	Destroy an initialized wakeup object
 *
 * \param	*_wu_obj	[in/out]	pointer to handle of the wakeup object (is only destroyed on successful return)
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_wu_obj is invalid
 * ****************************************************************************/
int lib_thread__wakeup_destroy(wakeup_hdl_t *_wu_obj)
{
	int line, ret;

	/* check arguments */
	if (_wu_obj == NULL){
		line = __LINE__;
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_wu_obj == NULL){
		line = __LINE__;
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	(*_wu_obj)->destroy = 1;
	xTaskAbortDelay((*_wu_obj)->wakeupTask);
	free_memory(*_wu_obj);
	s_number_open_wakeup_obj--;


	*_wu_obj = NULL;
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID, "%s(): failed with retval %i (line %u)\n",__func__, ret , line );
	return ret;
}

/* ************************************************************************//**
 * \brief	A new cyclic wait interval is set to the wakeup object
 *
 * \param	_wu_obj	[in/out]	pointer to handle of the wakeup object
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-PAR_NULL		_wu_obj is not valid
 * ****************************************************************************/
int lib_thread__wakeup_setinterval(wakeup_hdl_t _wu_obj, unsigned _interval)
{
	int line, ret;

	if (_wu_obj == NULL){
		line = __LINE__;
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	_wu_obj->interval_time = pdMS_TO_TICKS(_interval);
	return EOK;

    ERR_0:
    msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID, "%s(): failed with retval %i (line %u)\n",__func__, ret, line );
    return ret;
}

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
int lib_thread__wakeup_wait(wakeup_hdl_t _wu_obj)
{
	int line, ret;

	if (_wu_obj == NULL){
		line = __LINE__;
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_wu_obj->last_wait_time == 0) {
		_wu_obj->wakeupTask = xTaskGetCurrentTaskHandle();
		_wu_obj->last_wait_time = xTaskGetTickCount();
	}
	vTaskDelayUntil(&_wu_obj->last_wait_time,_wu_obj->interval_time);

	if (_wu_obj->destroy) {
		return -ESTD_INTR;
	}

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID, "%s(): failed with retval %i (line %u)\n",__func__, ret , line);
	return ret;
}
