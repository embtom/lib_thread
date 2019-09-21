/*
 * This file is part of the EMBTOM project
 * Copyright (c) 2018-2019 Thomas Willetal 
 * (https://github.com/embtom)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
 * \brief Init of the wakeup component
 *
 * Attention:
 * At the POSIX environment have to be called at the start of the "main"
 * because the signal mask are modified
 *
 * \return EOK, if successful, < EOK if not successful
 * ****************************************************************************/
int lib_thread__wakeup_init(void)
{
	s_number_open_wakeup_obj = 0;
	return EOK;
}

/* ************************************************************************//**
 * \brief Cleanup wakeup component
 * 
 * \return EOK, if successful, < EOK if not successful
 * ****************************************************************************/
int lib_thread__wakeup_cleanup(void)
{
	if (s_number_open_wakeup_obj) {
		return -ESTD_BUSY;
	}
	return EOK;
}

/* ************************************************************************//**
 * \brief Create a new wakeup object with the specified interval
 *
 * \param	*_wu_obj[out]	:   pointer to a wakeup object handle
 * \param	_interval			wakeup interval in ms (must not be 0)
 * \return	EOK				:   Success
 *			-EEXEC_NOINIT	:   Component not initialized
 *			-EPAR_NULL		:   NULL pointer specified for _wu_obj
 *			-ESTD_INVAL	    :   _interval is 0
 *			-ESTD_NOMEM		:   Insufficient memory available to initialize the wakeup object
 *			-ESTD_AGAIN		:   Not enougth timer resources available
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
 * \param	*_wu_obj[in/out] :	pointer to a wakeup object handle
 * \return	EOK				 :  Success
 *			-EEXEC_NOINIT	 :  Component not initialized
 *			-EPAR_NULL		 :  NULL pointer for _wu_obj
 *			-ESTD_INVAL		 :  _wu_obj is invalid
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
 * \brief	Wait on the specified wakeup object
 *
 * This function waits until the specified wakeup object's timeout interval
 * has elapsed. This function should be called at the top of a while or 
 * for loop.
 * If the wakeup object will be destroyed the wakeup wait routine unblocks
 * with -ESTD_INTR
 *
 * \param	*_wu_obj [in/out]:	pointer to a wakeup object handle
 * \return	EOK				 :  Success
 *			-EEXEC_NOINIT	 :  Component not (yet) initialized (any more)
 *			-EPAR_NULL		 :  NULL pointer for _wu_obj
 *			-ESTD_INTR		 :  The wakeup object is destroyed
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
