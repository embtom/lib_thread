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

/* system */
#include <stdlib.h>

/* frame */
#include <lib_convention__errno.h>
#include <lib_log.h>
#include <lib_timer_init_POSIX.h>
#include <lib_timer.h>

/* project*/
#include <lib_thread.h>
#include "lib_thread_wakeup.h"

/* *******************************************************************
 * defines
 * ******************************************************************/
#define M_DEV_LIB_THREAD_WHP_MODULE_ID 		"LIB_THD_WHP"


/* wakeup handle object structure */
struct internal_wakeup {
	timer_hdl_t timer;
	mutex_hdl_t timer_lock;
	unsigned int timer_interval;
	unsigned int timer_to_start;
};

/* *******************************************************************
 * (static) variables declarations
 * ******************************************************************/


/* *******************************************************************
 * Global Functions
 * ******************************************************************/

/* ************************************************************************//**
 * \brief Initialization of the wakeup 
 *
 * Attention:
 * At the POSIX environment it to be called at the start of the "main"
 * because the signal mask has to be modified
 *
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-ESTD_EBUSY		There are still some wakeup objects currently in use
 * ****************************************************************************/
int lib_thread__wakeup_init(void)
{
	return lib_timer__init();
}

/* ************************************************************************//**
 * \brief	Cleanup of wakeup component
 *
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-ESTD_EBUSY		There are still some wakeup objects currently in use
 * ****************************************************************************/
int lib_thread__wakeup_cleanup(void)
{
	return lib_timer__cleanup();
}

/* ************************************************************************//**
 * \brief	Creation of a wakeup object with the specified interval
 *
 * The underlaying timer is monotonic system clock 
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
	wu_obj = (wakeup_hdl_t)malloc(sizeof(**_wu_obj));
	if (wu_obj == NULL){
		line = __LINE__;
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	ret = lib_thread__mutex_init(&wu_obj->timer_lock);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_1;
	}

	ret = lib_timer__open(&wu_obj->timer, NULL, NULL);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_2;
	}

	lib_thread__mutex_lock(wu_obj->timer_lock);
	//ret = lib_timer__start(wu_obj->timer,_interval);
	wu_obj->timer_interval = _interval;
	wu_obj->timer_to_start = 1;
	lib_thread__mutex_unlock(wu_obj->timer_lock);

	*_wu_obj = wu_obj;
	return EOK;

	ERR_2:
	lib_thread__mutex_destroy(&wu_obj->timer_lock);

	ERR_1:
	free(wu_obj);

    ERR_0:
    msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
    return ret;

}

/* ************************************************************************//**
 * \brief	Destroy an initialized wakeup object
 *
 * \param	*_wu_obj	[in/out]	:	pointer to handle of the wakeup object (is only destroyed on successful return)
 * \return	EOK						:	Success
 *			-EEXEC_NOINIT			: 	Component not (yet) initialized (any more)
 *			-EPAR_NULL				:	NULL pointer specified for _wu_obj
 *			-EINVAL					:	_wu_obj is invalid
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

	ret = lib_thread__mutex_lock((*_wu_obj)->timer_lock);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}

	ret = lib_timer__stop((*_wu_obj)->timer);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}

	ret = lib_timer__close(&(*_wu_obj)->timer);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}

	lib_thread__mutex_unlock((*_wu_obj)->timer_lock);

	ret = lib_thread__mutex_destroy(&(*_wu_obj)->timer_lock);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}

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

	ret = lib_thread__mutex_lock(_wu_obj->timer_lock);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}

    ret = lib_timer__stop(_wu_obj->timer);
    if (ret < EOK) {
    	line = __LINE__;
    	goto ERR_0;
    }

	ret = lib_timer__start(_wu_obj->timer, _interval);
	if (ret < EOK) {
		line = __LINE__;
	    goto ERR_0;
	}

	_wu_obj->timer_interval = _interval;

	ret = lib_thread__mutex_unlock(_wu_obj->timer_lock);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}

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

	if ((_wu_obj->timer_to_start) && (_wu_obj->timer_interval != 0)) {

		ret = lib_thread__mutex_lock(_wu_obj->timer_lock);
		if (ret < EOK) {
			line = __LINE__;
			goto ERR_0;
		}

		ret = lib_timer__start(_wu_obj->timer,_wu_obj->timer_interval);
		if (ret < EOK) {
			line = __LINE__;
			lib_thread__mutex_unlock(_wu_obj->timer_lock);
			goto ERR_0;
		}
		_wu_obj->timer_to_start = 0;
		lib_thread__mutex_unlock(_wu_obj->timer_lock);
	}

	ret = lib_timer__wakeup_wait(_wu_obj->timer);
	if (ret < EOK) {
		line = __LINE__;
		goto ERR_0;
	}
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID, "%s(): failed with retval %i (line %u)\n",__func__, ret , line);
	return ret;
}
