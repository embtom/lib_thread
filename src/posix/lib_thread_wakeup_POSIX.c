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
	return lib_timer__init();
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
	return lib_timer__cleanup();
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
