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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>


/* system */
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
/* project*/
#include "lib_convention__errno.h"
#include "lib_thread.h"
#include "lib_log.h"


/* *******************************************************************
 * defines
 * ******************************************************************/
#define LIB_THREAD_MODULE_ID 		"LIB_THD"


/* *******************************************************************
 * custom data types (e.g. enumerations, structures, unions)
 * ******************************************************************/
struct thread_hdl_attr {
	pthread_t thread_hdl;
	char *thread_name;
	size_t thread_name_len;
};

struct mutex_hdl_attr {
	pthread_mutex_t	mtx_hdl;
};

struct signal_hdl_attr {
	pthread_cond_t cond_hdl;
	pthread_mutex_t mtx_hdl;
	unsigned int number_of_waiting_threads;
	unsigned int number_of_outstanding_signals;
	unsigned int destroy_active;
};

struct sem_hdl_attr {
	sem_t sem_hdl;
};

struct condvar_hdl_attr {
	pthread_cond_t	cond;					// condition variable
};


/* *******************************************************************
 * static function declarations
 * ******************************************************************/
static char* lib_thread__strsched(enum process_sched _sched);
void lib_thread__signal_pthread_cancel_handler(void *_hdl);
static int lib_thread__convert_relative2abstime(const int _clock_src, const unsigned _tmoms, struct timespec *_timespec);


/* *******************************************************************
 * \brief	Initialization of the lib_thread
 * ---------
 * \remark
 * ---------
 *
 * \param	_sched : Set scheduling mode of the actual calling process
 * 					 Possible scheduling policies are
 * 					 	PROCESS_SCHED_other
 *						PROCESS_SCHED_fifo
 *						PROCESS_SCHED_rr
 *						PROCESS_SCHED_batch
 *						PROCESS_SCHED_idle
 *
 * \param   _pcur  :  Priority of the main process, will only considered
 * 					  if PROCESS_SCHED_fifo, PROCESS_SCHED_rr is selected
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__init(enum process_sched _sched, int _pcur)
{
	int ret ;
	int sched;
	pid_t	pid;
	struct sched_param	param;

	/* Get ID of actual process */
	pid = getpid();

	ret = sched_getscheduler(pid);
	if (ret == -1)	{
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	enum process_sched old_sched = (enum process_sched)ret;


	if ((_sched == PROCESS_SCHED_fifo) || (_sched == PROCESS_SCHED_rr))
	{	param.__sched_priority = _pcur;	}
	else
	{	param.__sched_priority = 0; 	}

	switch (_sched)
	{
		case PROCESS_SCHED_other:  sched = SCHED_OTHER; break;
		case PROCESS_SCHED_fifo :  sched = SCHED_FIFO; 	break;
		case PROCESS_SCHED_rr   :  sched = SCHED_RR;    break;
		case PROCESS_SCHED_batch:  sched = SCHED_BATCH; break;
		case PROCESS_SCHED_idle :  sched = SCHED_IDLE;  break;
		default: {
			ret = -ESTD_INVAL;
			goto ERR_0;
		}
	}

	ret = sched_setscheduler(pid, (int)sched, &param);
	if(ret == -1) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	ret = sched_getscheduler(pid);
	if(ret != EOK) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	if(ret != (int)_sched) {
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): failed policy set (Policy from %s to %s with prio %u)\n",__func__, lib_thread__strsched(old_sched), lib_thread__strsched(_sched), _pcur);
		return -EACCES;
	}
	else {
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully (Policy from %s to %s with prio %u)\n",__func__, lib_thread__strsched(old_sched), lib_thread__strsched(_sched), _pcur);
		return 0;
	}

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}


/* *******************************************************************
 * \brief	Creation of thread worker
 * ---------
 * \remark  The call of the init routine is only be allowed at the main
 * 			routine of a process
 * ---------
 *
 * \param	*_hdl			[out] :		pointer to a handle for the thread to be created
 * \param	*_worker		[in]  : 	pointer to a worker routine
 * \param	*_arg			[in]  :		pointer to a worker routine argument
 * \param	_relative_priority	  : 	relative priority to the parent thread/process
 * 										The parameter is only relevant if PROCESS_SCHED_fifo or PROCESS_SCHED_rr
 * 										is selected.
 * \param	*_thread_name	[in]  :		pointer to the thread name is optional
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__create (thread_hdl_t *_hdl, thread_worker_t *_worker, void *_arg, int _relative_priority, const char *_thread_name)
{
	int ret, len;
	int prio_min, prio_max, thread_prio;
	struct sched_param	priority_param;
	enum process_sched  sched;

	pthread_attr_t thread_attr;
	thread_hdl_t hdl;

	if ((_hdl == NULL) || (_worker == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* Request of the adjusted scheduling parameters of the calling thread or process */
	ret = pthread_getschedparam(pthread_self(), (int*)&sched, &priority_param);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	prio_min = sched_get_priority_min(sched);
	prio_max = sched_get_priority_max(sched);
	if((prio_min == -1) || (prio_max == -1)) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	/* Priority check is only necessary if PROCESS_SCHED_fifo or PROCESS_SCHED_rr is selected during the init */
	if (prio_min == prio_max) {
		thread_prio = prio_min;
	}
	else {
		thread_prio = priority_param.__sched_priority + _relative_priority;
		if ((thread_prio < prio_min ) || (thread_prio > prio_max )) {
			ret = -EPAR_RANGE;
			goto ERR_0;
		}
	}

	ret = pthread_attr_init(&thread_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	/*Specifies that the scheduling policy and associated attributes are to be set to the corresponding values from this attribute object*/
	ret = pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	/* Set scheduling policy at attribute object */
	ret = pthread_attr_setschedpolicy(&thread_attr, (int)sched);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	priority_param.__sched_priority = thread_prio;
	ret = pthread_attr_setschedparam(&thread_attr, &priority_param);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	hdl = calloc(1,sizeof (struct thread_hdl_attr));
	if(hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_1;
	}

	/* Creation of thread */
	ret = pthread_create(&hdl->thread_hdl, &thread_attr, _worker, _arg);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_2;
	}

	ret = pthread_attr_destroy(&thread_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_3;
	}

	if (_thread_name != NULL) {
		pthread_setname_np(hdl->thread_hdl,_thread_name);
		len = strlen(_thread_name) +1;
		hdl->thread_name = calloc(len,sizeof(char));
		if (hdl->thread_name != NULL) {
			memcpy(hdl->thread_name,_thread_name,len);
			hdl->thread_name_len = len;
		}
	}
	else {
		hdl->thread_name = NULL;
		hdl->thread_name_len = 0;
	}


	if (_thread_name == NULL)
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully (Thread ID '%u' Name '<NO NAME>' prio: '%u')\n",__func__, hdl->thread_hdl, thread_prio);
	else
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully (Thread ID '%u' Name '%s' prio: '%u')\n",__func__, hdl->thread_hdl, _thread_name, thread_prio);

	*_hdl = hdl;
	return EOK;

	ERR_3:
	pthread_cancel(hdl->thread_hdl);
	pthread_join(hdl->thread_hdl, NULL);

	ERR_2:
	free(hdl);

	ERR_1:
	pthread_attr_destroy(&thread_attr);

	ERR_0:

	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	if (_hdl != NULL) {
		*_hdl = NULL;
	}
	return ret;
}


/* *******************************************************************
 * \brief	Join a thread and destroys the handle.
 * ---------
 * \remark  The calling routine blocks until the referenced thread returns
 * ---------
 *
 * \param	*_hdl			[in/out] :	pointer to a handle for the thread to be joined
 * \param	**_ret_val   	[out] : 	pointer to a pointer to a return value of the target thread
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__join(thread_hdl_t *_hdl, void **_ret_val)
{
	int ret, threadid;
	char buffer[20] = {0};

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL){
		ret = -ESTD_SRCH;
		goto ERR_0;
	}

	lib_thread__getname(*_hdl, &buffer[0], sizeof(buffer));

	ret = pthread_join((*_hdl)->thread_hdl, _ret_val);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	if ((*_hdl)->thread_name != NULL) {
		free((*_hdl)->thread_name);
	}

	threadid = (*_hdl)->thread_hdl;

	free(*_hdl);
	(*_hdl) = NULL;
	//LOG_LEVEL_debug_prio_1
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully (Thread ID '%u' Name '%s') \n",__func__, threadid ,buffer);

	return EOK;


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}

/* *******************************************************************
 * \brief	Request to cancel a working thread
 * ---------
 * \remark  The execution of a thread stops only at cancellation points
 * ---------
 *
 * \param	_hdl			[in] :	handle for the thread to be canceled
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__cancel(thread_hdl_t _hdl)
{
	int ret;

	/* check argument */
	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* cancel thread */
	ret = pthread_cancel(_hdl->thread_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully (Thread ID '%u')\n",__func__, _hdl->thread_hdl);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	Get the thread name
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in] :	handle for the thread to be canceled
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__getname(thread_hdl_t _hdl, char * _name, int _maxlen)
{
	int ret, len;

	if ((_hdl == NULL) || (_name == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if(_hdl->thread_name != NULL) {
		if (_hdl->thread_name_len >= _maxlen) {
			len = _maxlen;
		}
		else {
			len = _hdl->thread_name_len;
		}
		memcpy(_name,_hdl->thread_name, len);
		return EOK;
	}

	ret = ENOENT;
	ret = convert_std_errno(ret);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	The calling task sleeps for a defined time
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			:	milliseconds to sleep
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__msleep (unsigned int _milliseconds)
{
	int ret;
	struct timespec sleep_interval;
	sleep_interval.tv_sec = _milliseconds / 1000UL;
	sleep_interval.tv_nsec = (_milliseconds % 1000UL) * 1000000UL;

	ret = nanosleep(&sleep_interval, NULL);
	if (ret < EOK) {
		ret = convert_std_errno(errno);
		msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n", __func__, ret);
		return ret;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully\n",__func__ );
	return EOK;
}

/* *******************************************************************
 * \brief	Initialization of a mutex object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a mutex object
 * 										to initialize
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_init (mutex_hdl_t *_hdl)
{
	int ret;
	pthread_mutexattr_t mutex_attr;
	mutex_hdl_t hdl;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutexattr_init(&mutex_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

    //	PTHREAD_PRIO_NONE The mutex uses no priority protocol.
    //	PTHREAD_PRIO_PROTECT The mutex uses the priority ceiling protocol.
    //	PTHREAD_PRIO_INHERIT The mutex uses the priority inheritance protocol.

	//	The priority inheritance protocol lets a mutex elevate the priority of its holder
	//  to that of the waiting thread with the highest priority.
	//	Because the priority inheritance protocol awards a priority boost to a mutex holder
	//  only when it's absolutely needed, it can be more efficient than the priority ceiling protocol.

	ret = pthread_mutexattr_setprotocol (&mutex_attr,PTHREAD_PRIO_INHERIT);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	ret = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	hdl = malloc (sizeof(struct mutex_hdl_attr));
	if (hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_1;
	}

	ret = pthread_mutex_init(&hdl->mtx_hdl,&mutex_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_2;
	}

	ret = pthread_mutexattr_destroy(&mutex_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	*_hdl = hdl;
    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully\n",__func__);

	return EOK;

	ERR_2:
	free(hdl);

	ERR_1:
	pthread_mutexattr_destroy(&mutex_attr);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	Destroys an mutex again
 * ---------
 * \remark	Mutex must be unlocked
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a mutex object
 * 										to destroy
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_destroy (mutex_hdl_t *_hdl)
{
	int ret, mutex_id;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	/*Check if mutex is locked*/
	ret = lib_thread__mutex_trylock(*_hdl);
	if (ret != EOK){
		goto ERR_0;
	}

	ret = pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	ret = pthread_mutex_destroy(&(*_hdl)->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	free(*_hdl);
	*_hdl = NULL;
    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully\n",__func__);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}


/* *******************************************************************
 * \brief	Look of an mutex
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in] :	the handle of a mutex object to lock
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_lock (mutex_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully (Mutex ID '%u')\n",__func__, &_hdl->mtx_hdl);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}

/* *******************************************************************
 * \brief	Unlock of an mutex object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in] :	the handle of a mutex object to lock
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_unlock (mutex_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully (Mutex ID '%u')\n",__func__, &_hdl->mtx_hdl);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}

/* *******************************************************************
 * \brief	Trylock of an mutex object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in] :	the handle of a mutex object to trylock
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_trylock (mutex_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_trylock(&_hdl->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		switch (ret) {
			case -EEXEC_DEADLK:
				ret = -ESTD_BUSY;
				goto ERR_0;
				break;
			default:
				goto ERR_0;
		}
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully (Mutex ID '%u')\n", __func__, &_hdl->mtx_hdl);
	return EOK;

	ERR_0:

	if (ret == -ESTD_BUSY)
		msg (LOG_LEVEL_warning, LIB_THREAD_MODULE_ID, "%s(): Mutex already locked (Mutex ID '%u')\n",__func__, &_hdl->mtx_hdl);
	else
		msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__,ret );
	return ret;
}



/* *******************************************************************
 * \brief	Initialization of a signal object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a signal object
 * 										to initialize
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_init (signal_hdl_t *_hdl)
{
	signal_hdl_t hdl;
	pthread_condattr_t cond_attr;
	pthread_mutexattr_t	mtx_attr;

	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_condattr_init(&cond_attr);
	if( ret != EOK)	{
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	ret = pthread_mutexattr_init(&mtx_attr);
	if( ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	ret = pthread_condattr_setclock(&cond_attr,CLOCK_MONOTONIC);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_2;
	}

	ret = pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_2;
	}

	hdl = malloc (sizeof (struct signal_hdl_attr));
	if(hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_1;
	}

	ret = pthread_cond_init(&hdl->cond_hdl, &cond_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_3;
	}

	ret = pthread_mutex_init(&hdl->mtx_hdl, &mtx_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_4;
	}

	ret = pthread_condattr_destroy(&cond_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_5;
	}

	ret = pthread_mutexattr_destroy(&mtx_attr);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_5;
	}

	hdl->number_of_waiting_threads = 0;
	hdl->number_of_outstanding_signals = 0;
	hdl->destroy_active = 0;

	*_hdl = hdl;
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully (Signal ID '%u')\n",__func__, &hdl->cond_hdl);
	return EOK;

	ERR_5:
	pthread_mutex_destroy(&hdl->mtx_hdl);

	ERR_4:
	pthread_cond_destroy(&hdl->cond_hdl);

	ERR_3:
	free(hdl);

	ERR_2:
	pthread_mutexattr_destroy(&mtx_attr);

	ERR_1:
	pthread_condattr_destroy(&cond_attr);

	ERR_0:
	if(_hdl != NULL) {
		*_hdl = NULL;
	}
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret);

	return ret;

}

/* *******************************************************************
 * \brief	Destroys an signal object again
 * ---------
 * \remark	All waiting threads on a signal_wait condition unblocks with EPERM
 * 			It can be used for a worker thread termination
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a signal object
 * 										to destroy
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_destroy (signal_hdl_t *_hdl)
{
	int ret, tmp_number_of_outstanding_signals;
	unsigned int signal_id;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL) 	{
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&(*_hdl)->mtx_hdl);
	if (ret != 0) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	/*Check if signal this signal destroying is actual active */
	if ((*_hdl)->destroy_active > 0) {
		ret = -ESTD_BUSY;
		goto ERR_1;
	}

	(*_hdl)->destroy_active = 1;
	tmp_number_of_outstanding_signals = (*_hdl)->number_of_outstanding_signals;
	(*_hdl)->number_of_outstanding_signals = 0;

	ret = pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		(*_hdl)->destroy_active = 0;
		(*_hdl)->number_of_outstanding_signals = tmp_number_of_outstanding_signals;
		goto ERR_0;
	}

	if (tmp_number_of_outstanding_signals > 0) {
		ret = pthread_cond_broadcast(&(*_hdl)->cond_hdl);
		if (ret != 0) {
			ret = convert_std_errno(ret);
			pthread_mutex_lock(&(*_hdl)->mtx_hdl);
			(*_hdl)->number_of_outstanding_signals = tmp_number_of_outstanding_signals;
			(*_hdl)->destroy_active = 0;
			goto ERR_1;
		}

		pthread_mutex_lock(&(*_hdl)->mtx_hdl);
		while ((*_hdl)->number_of_waiting_threads) {
			/* check the number of threads still waiting */
			pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
			/* wait briefly to give the waiting threads some execution time */
			lib_thread__msleep(5);
			pthread_mutex_lock(&(*_hdl)->mtx_hdl);
		}
	}
	else {
		ret = pthread_mutex_lock(&(*_hdl)->mtx_hdl);
		if (ret != 0) {
			ret = convert_std_errno(ret);
			goto ERR_0;
		}
	}

    ret = pthread_cond_destroy(&(*_hdl)->cond_hdl);
	if (ret != 0) {
		ret = convert_std_errno(ret);
		(*_hdl)->destroy_active = 0;
		goto ERR_1;
	}

	ret = pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
	if (ret != 0) {
		ret = convert_std_errno(ret);
		(*_hdl)->destroy_active = 0;
		goto ERR_0;
	}

	ret = pthread_mutex_destroy(&(*_hdl)->mtx_hdl);
	if (ret != 0) {
		ret = convert_std_errno(ret);
		(*_hdl)->destroy_active = 0;
		goto ERR_0;
	}

	free(*_hdl);
	*_hdl = NULL;
    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): successfully\n",__func__);
	return EOK;

	ERR_1:
	pthread_mutex_unlock(&(*_hdl)->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}

/* *******************************************************************
 * \brief	Emit a signal
 * ---------
 * \remark	If a signal is emitted without any waiting thread the signal
 * 			gets lost
 * ---------
 *
 * \param	_hdl			[in] : 	handle to a signal object to trigger
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_send (signal_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	if (_hdl->destroy_active) {
		ret = -ESTD_PERM;
		goto ERR_1;
	}	/* check whether there is any pending signal */

	if (_hdl->number_of_outstanding_signals > 0) {
		/* decrement number of pending signals */
		_hdl->number_of_outstanding_signals--;

		/* unlock signal mutex */
		ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
		if (ret != 0){	/* should never happen */
			ret = convert_std_errno(ret);
			/* increment number of pending signals again as we are not able to unlock the signal's mutex (which is mandatory for actually processing the signal) */
			_hdl->number_of_outstanding_signals++;
			return ret;
		}

		/* send signal */
		ret = pthread_cond_signal(&_hdl->cond_hdl);
		if (ret != 0){
			ret = convert_std_errno(ret);
			/* increment number of pending signals again as we are not able to send the signal */
			pthread_mutex_lock(&_hdl->mtx_hdl);
			_hdl->number_of_outstanding_signals++;
			pthread_mutex_unlock(&_hdl->mtx_hdl);
			return ret;
		}
	}else{
		/* nobody is waiting for this signal -> just unlock signal mutex */
		ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
		if (ret != 0){	/* should never happen */
			ret = convert_std_errno(ret);
			return ret;
		}
	}

	/* Check if somebody is waiting for signal */


    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully \n",__func__);
	return EOK;

	ERR_1:
	pthread_mutex_unlock(&_hdl->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;

}

/* *******************************************************************
 * \brief	Waiting for an signal
 * ---------
 * \remark	The calling thread blocks unit the corresponding signal is triggered.
 *
 * ---------
 *
 * \param	_hdl			[in] : 	handle to a signal object to wait
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_wait (signal_hdl_t _hdl)
{
	int ret, mtx_ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret != EOK) {
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	if (_hdl->destroy_active) {
		pthread_mutex_unlock(&_hdl->mtx_hdl);
		ret = -ESTD_PERM;
	}
	else {
		_hdl->number_of_waiting_threads++;
		_hdl->number_of_outstanding_signals++;

		/*Set a cleanup handler to threat the thread cancellation */
		pthread_cleanup_push(&lib_thread__signal_pthread_cancel_handler, _hdl);

        /* A check if the thread unblocking was no spurious wakeup*/
		while (1)
		{
			ret = pthread_cond_wait(&_hdl->cond_hdl, &_hdl->mtx_hdl);
			if (ret != EOK) {
				ret = convert_std_errno(ret);
				_hdl->number_of_waiting_threads--;
				_hdl->number_of_outstanding_signals--;
				break;
			}

			/* ensure that no spurious wakeup occurred (which are very well possible according to the standard) */
			if (_hdl->number_of_waiting_threads != _hdl->number_of_outstanding_signals) {
				break;
			}

		}
		pthread_cleanup_pop(0);
	}

	_hdl->number_of_waiting_threads--;

	if (_hdl->destroy_active) {
		ret = -ESTD_PERM;
	}

	/* unlock signal mutex */
	mtx_ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
	if (mtx_ret != 0){	/* should never happen */
		ret = convert_std_errno(mtx_ret);
		goto ERR_0;
	}

	switch (ret) {
		case EOK :
            msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): unblocked successfully \n",__func__);
			break;
		case -ESTD_PERM :
            msg (LOG_LEVEL_debug_prio_2, LIB_THREAD_MODULE_ID, "%s(): unblocked destroyed \n",__func__);
			break;
		default :
			goto ERR_0;
	}

	return ret;

	ERR_1:
	pthread_mutex_unlock(&_hdl->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;

}

/* *******************************************************************
 * \brief	Waiting for an signal with an time limit
 * ---------
 * \remark	The calling thread blocks unit the corresponding signal is triggered or
 * 			the time runs out
 *
 * ---------
 *
 * \param	_hdl			[in] : 	handle to a signal object to wait
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_timedwait (signal_hdl_t _hdl, unsigned int _milliseconds)
{
	int ret, mtx_ret;
	struct timespec actual_time;
	unsigned int signal_id;

	if (_hdl == NULL)
		return -EPAR_NULL;

	if (_milliseconds == LIB_THREAD__TIMEOUT_INFINITE)
		return lib_thread__signal_wait(_hdl);

	ret = lib_thread__convert_relative2abstime(CLOCK_MONOTONIC,_milliseconds, &actual_time);
	if(ret != 0) {
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret != EOK)	{
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	if (_hdl->destroy_active) {
		pthread_mutex_unlock(&_hdl->mtx_hdl);
		ret = -ESTD_PERM;
	}
	else {
		_hdl->number_of_waiting_threads++;
		_hdl->number_of_outstanding_signals++;
		pthread_cleanup_push(&lib_thread__signal_pthread_cancel_handler, _hdl);

		signal_id = &_hdl->cond_hdl;

		/* A check if the thread unblocking was no spurious wakeup*/
		while (1)
		{
			ret = pthread_cond_timedwait(&_hdl->cond_hdl, &_hdl->mtx_hdl,&actual_time);
			if (ret != EOK) {
				ret = convert_std_errno(ret);
				_hdl->number_of_waiting_threads--;
				_hdl->number_of_outstanding_signals--;
				break;
			}

			/* ensure that no spurious wakeup occurred (which are very well possible according to the standard) */
			if (_hdl->number_of_waiting_threads != _hdl->number_of_outstanding_signals) {
				break;
			}
		}
		pthread_cleanup_pop(0);
	}

	_hdl->number_of_waiting_threads--;

	if (_hdl->destroy_active) {
		ret = -ESTD_PERM;
	}

	/* unlock signal mutex */
	mtx_ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
	if (mtx_ret != EOK){	/* should never happen */
		ret = convert_std_errno(mtx_ret);
		goto ERR_0;
	}

	switch (ret) {
		case EOK :
			msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s(): unblocked successfully (Signal ID '%u')\n",__func__, signal_id);
			break;
		case -ESTD_PERM :
			msg (LOG_LEVEL_debug_prio_2, LIB_THREAD_MODULE_ID, "%s(): unblocked destroyed (Signal ID '%u')\n",__func__, signal_id);
			break;
		case -EEXEC_TO :
			msg (LOG_LEVEL_debug_prio_2, LIB_THREAD_MODULE_ID, "%s(): unblocked timeout (Signal ID '%u')\n",__func__, signal_id);
			break;
		default :
			goto ERR_0;
	}

	return ret;

	ERR_1:
	pthread_mutex_unlock(&_hdl->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );

	return ret;
}

/* *******************************************************************
 * \brief	Initialization of a semaphore object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a semaphore object
 * 										to initialize
 * 			_count					 :  Initial semaphore count value;
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_init (sem_hdl_t *_hdl, int _count)
{
	int ret;
	sem_hdl_t hdl;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	hdl = malloc(sizeof (struct sem_hdl_attr));
	if (hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	/* initialize semaphore */
	if (sem_init(&hdl->sem_hdl, 0, _count) != 0) {
		ret = convert_std_errno(errno);
		goto ERR_1;
	}

	*_hdl = hdl;
    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__sem_init :  successfully\n");
	return EOK;

	ERR_1:
	free(hdl);
	if (_hdl != NULL) {
		*_hdl = NULL;
	}

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	Destroys a semaphore again
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a semaphore object
 * 										to destroy
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_destroy (sem_hdl_t *_hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	if (sem_destroy(&(*_hdl)->sem_hdl) != 0) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	free(*_hdl);
	*_hdl = NULL;
    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully\n",__func__);


	return EOK;


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}


/* *******************************************************************
 * \brief	Incrementation of a semaphore,
 * ---------
 * \remark	To increment the value of a semaphore
 * ---------
 *
 * \param	_hdl				[in] :	handle of a semaphore object
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_post (sem_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* try to lock on semaphore */
	if (sem_post(&_hdl->sem_hdl) != 0) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully\n",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	Decrement semaphore and if count <=0 calling thread is blocked
 * ---------
 * \remark	If the value of the semaphore is negative, the calling process blocks;
 * 			one of the blocked processes wakes up when another process calls sem_post
 * ---------
 *
 * \param	_hdl				[in] :	handle of a semaphore object
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_wait (sem_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* try to lock on semaphore */
	if (sem_wait(&_hdl->sem_hdl) != 0) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully\n",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s() : failed with retval %i\n", __func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	Decrement semaphore and if count <=0 calling thread is blocked
 * ---------
 * \remark	If the value of the semaphore is negative, the calling process blocks;
 * 			one of the blocked processes wakes up when another process calls sem_post
 * ---------
 *
 * \param	_hdl				[in] :	handle of a semaphore object
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_timedwait (sem_hdl_t _hdl, int _milliseconds)
{
	int ret;
	struct timespec actual_time;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_milliseconds == LIB_THREAD__TIMEOUT_INFINITE) {
		return lib_thread__sem_wait(_hdl);
	}

	ret = lib_thread__convert_relative2abstime(CLOCK_REALTIME, _milliseconds, &actual_time);
	if (ret < EOK) {
		goto ERR_0;
	}

	if (sem_timedwait(&_hdl->sem_hdl, &actual_time) != 0) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s() : failed with retval %i\n", __func__, ret );
	return ret;
}

/* *******************************************************************
 * \brief	Decrement semaphore and if count <=0 return with error
 * ---------
 * \remark	If the value of the semaphore is negative, the calling process blocks;
 * 			one of the blocked processes wakes up when another process calls sem_post
 * ---------
 *
 * \param	_hdl[in] :	handle of a semaphore object
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_trywait (sem_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* try to lock on semaphore */
	if (sem_trywait(&_hdl->sem_hdl) != 0) {
		ret = convert_std_errno(errno);
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s():  successfully\n",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *************************************************************************//**
 * \brief	Create and initialize a new condvar object
 * ---------
 * \remark	On successful creation, a reference to the condvar's handle is stored in
 * 			_hdl.
 * ---------
 * \param	*_hdl [out]	pointer to handle of the cond object (will be allocated; only valid on successful return)
 * \return	EOK				Success
 *			-EPAR_NULL		NULL pointer specified for _cond
 *			-ESTD_BUSY		_cond is registered and not yet destroyed.
 *			-ESTD_NOMEM		High-level OSes only: Insufficient memory available to initialize the condvar
 *			-ESTD_AGAIN		High-level OSes only: Insufficient system resources available to initialize the condvar
 * ****************************************************************************/
int lib_thread__cond_init(cond_hdl_t *_hdl)
{
	int ret;
	cond_hdl_t hdl;
	pthread_condattr_t	cnd_attr;	/* condition attribute object */

	/* check arguments */
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* initialize condition attribute */
	ret = pthread_condattr_init(&cnd_attr);
	if (ret != 0){
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	/* set clock source */
	ret = pthread_condattr_setclock(&cnd_attr, CLOCK_MONOTONIC);
	if (ret != 0){	/* should never happen */
		ret = convert_std_errno(ret);
		goto ERR_1;
	}

	/* create handle on heap */
	hdl = malloc(sizeof(struct condvar_hdl_attr));
	if (hdl == NULL){
		ret = convert_std_errno(errno);
		goto ERR_1;
	}

	/* initialize handle */
	ret = pthread_cond_init(&hdl->cond, &cnd_attr);
	if (ret != 0) {
		ret = convert_std_errno(ret);
		goto ERR_2;
	}
	*_hdl = hdl;
    msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "%s() :  successfully\n",__func__);
	return EOK;

	ERR_2:
	free(hdl);

	ERR_1:
	pthread_condattr_destroy(&cnd_attr);

	ERR_0:

	if(_hdl != NULL) {
		_hdl = NULL;
	}

	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *************************************************************************//**
 * \brief	Destroy a condvar which is not waited on
 *
 * Calling this function does not affect scheduling behavior.
 * Once successfully destroyed, the condvar's handle is deleted from memory.
 * Further usage of the handle on interface function calls other than
 * lib_thread__cond_init() will result in an error.
 *
 * WARNINGS:
 *	-	If the condvar gets destroyed by another thread while still waiting on
 *		it, the behavior of the waiting call(s) is undefined!
 *	-	On high-level OSes, this function must not be called from within an
 *		interrupt context.
 *
 * \param	*_cond	[in/out]	pointer to handle of the cond object (is only destroyed on successful return)
 * \return	EOK				Success
 *			-EPAR_NULL		NULL pointer specified for _cond
*			-ESTD_BUSY		There are tasks blocking on _cond
 *			-ESTD_INVAL		_cond is invalid
 * ****************************************************************************/
int lib_thread__cond_destroy(cond_hdl_t *_hdl)
{
	int ret;

	/* check argument */
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	/* destroy */
	ret = pthread_cond_destroy(&(*_hdl)->cond);
	if (ret != 0){
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	/* free and invalidate condvar handle */
	free(*_hdl);
	*_hdl = NULL;
	return EOK;

	ERR_0:

	if(_hdl != NULL) {
		_hdl = NULL;
	}

	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

/* *************************************************************************//**
 * \brief	Unblock a thread waiting on a conditional variable
 *
 * This function unblocks a thread waiting on the condition variable.
 * If more than one thread is blocked, the OS-scheduling policy shall identify
 * the thread with the highest priority and unblock this thread. Whenever the
 * threads return at lib_thread__cond_wait() or lib_thread__cond_timedwait(),
 * it is guaranteed that they own the associated mutex.
 *
 * This function may be called whether or not the associated mutex is held by the
 * calling thread.
 * This function shall have no effect, if currently no thread blocks on _cond.
 *
 * WARNINGS:
 *	-	This function must not be called from within an interrupt context.
 *
 * \param	*_cond	[in]	pointer to handle of the cond object
 * \return	EOK				Success
 *			-EPAR_NULL		NULL pointer specified for _cond
 *			-ESTD_INVAL		_cond is invalid
 * ****************************************************************************/
int lib_thread__cond_signal(cond_hdl_t _hdl)
{
	int ret;

	/* check argument */
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* signal */
	ret = pthread_cond_signal(&_hdl->cond);
	if (ret != 0){
		ret = convert_std_errno(ret);
		goto ERR_0;
	}
	return EOK;

	ERR_0:

	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s() : failed with retval %i\n",__func__, ret );
	return ret;
}

/* *************************************************************************//**
 * \brief	Unblock all threads waiting on a conditional variable
 *
 * This function unblocks all threads currently waiting on the condition variable.
 * Whenever the threads return at lib_thread__cond_wait() or lib_thread__cond_timedwait(),
 * it is guaranteed that they own the associated mutex.
 *
 * This function may be called whether or not the associated mutex is held by the
 * calling thread.
 * This function shall have no effect, if currently no thread blocks on _cond.
 *
 * WARNINGS:
 *	-	This function must not be called from within an interrupt context.
 *
 * \param	*_cond	[in]	pointer to handle of the cond object
 * \return	EOK				Success
 *			-EPAR_NULL		NULL pointer specified for _cond
 *			-ESTD_INVAL		_cond is invalid
 * ****************************************************************************/
int lib_thread__cond_broadcast(cond_hdl_t _hdl)
{
	int ret;

	/* check argument */
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_cond_broadcast(&(_hdl)->cond);
	if (ret != 0){
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s() : failed with retval %i\n",__func__, ret );
	return ret;
}

/* *************************************************************************//**
 * \brief	Block on a conditional variable
 *
 * This function blocks on a condition variable. This function shall be called
 * with a mutex locked by the calling thread (otherwise -ESTD_PERM will be returned).
 * The function will cause the calling thread to block on the condition variable
 * and release the mutex immediately. Upon function return, the mutex is locked
 * by the calling thread.
 *
 * WARNINGS:
 *	-	If the condvar gets destroyed by another thread while still waiting on
 *		it, the behavior of the waiting call(s) is undefined!
 *	-	If the mutex gets destroyed during this call, the behavior is undefined!
 *	-	This function must not be called from within an interrupt context.
 *
 * \param	*_cond	[in]	pointer to handle of the cond object
 * 			*_mutex	[in]	pointer to handle of the mutex object
 * \return	EOK				Success
 *			-EPAR_NULL		NULL pointer specified for _cond or _mutex
 *			-ESTD_INVAL		_cond or _mutex is invalid or
 * 	 	 	 	 	 	 	different mutexes where supplied for concurrent
 *	 	 	 	 	 	 	lib_thread__cond_timedwait() or lib_thread__cond_wait()
 *	 	 	 	 	 	 	function calls using the same condition variable.
 *			-ESTD_PERM		The mutex was not owned by the thread during at the time
 *							of the call
 *			-ESTD_ACCES		OSEK only: Function is called from within a prohibited context
 * ****************************************************************************/
int lib_thread__cond_wait(cond_hdl_t _hdl, mutex_hdl_t _mtx)
{
	int ret;

	/* check argument */
	if ((_hdl == NULL) || (_mtx == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* wait */
	ret = pthread_cond_wait(&(_hdl)->cond, &(_mtx)->mtx_hdl);
	if (ret != 0){
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s() : failed with retval %i\n",__func__, ret );
	return ret;
}

/* *************************************************************************//**
 * \brief	Block on a conditional variable for a specific time
 *
 * This function blocks on a condition variable. This function shall be called
 * with a mutex locked by the calling thread (otherwise -ESTD_PERM will be returned).
 * The function will cause the calling thread to block on the condition variable
 * and release the mutex immediately. Upon function return, the mutex is locked
 * by the calling thread.
 * The function will return -EEXEC_TO whenever _tmoms passes before _cond is
 * signaled or broadcasted. User may call this function with
 * _tmoms set to LIB_THREAD__TIMEOUT_INFINITE. In this case the function will behave
 * as lib_thread__cond_wait.
 *
 * WARNINGS:
 *	-	If the condvar gets destroyed by another thread while still waiting on
 *		it, the behavior of the waiting call(s) is undefined!
 *	-	If the mutex gets destroyed during this call, the behavior is undefined!
 *	-	This function must not be called from within an interrupt context.
 *
 * \param	*_cond	[in]	pointer to handle of the cond object
 * 			*_mutex	[in]	pointer to handle of the mutex object
 * \return	EOK				Success
 * 			-EEXEC_TO		_tmoms has passed
 *			-EPAR_NULL		NULL pointer specified for _cond or _mutex
 *			-ESTD_INVAL		_cond or _mutex is invalid or
 * 	 	 	 	 	 	 	different mutexes where supplied for concurrent
 *	 	 	 	 	 	 	lib_thread__cond_timedwait() or lib_thread__cond_wait()
 *	 	 	 	 	 	 	function calls using the same condition variable.
 *			-ESTD_PERM		The mutex was not owned by the thread during at the time
 *							of the call
 * ****************************************************************************/
int lib_thread__cond_timedwait(cond_hdl_t _hdl, mutex_hdl_t _mtx, int _tmoms)
{
	struct timespec cur_time;
	int ret;

	if ((_hdl == NULL) || (_mtx == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_tmoms == LIB_THREAD__TIMEOUT_INFINITE)
		return lib_thread__cond_wait(_hdl, _mtx);


	/* get current time */
	ret = lib_thread__convert_relative2abstime(CLOCK_MONOTONIC, _tmoms, &cur_time);
	if (ret != 0) {
		goto ERR_0;
	}

	/* timedwait */
	ret = pthread_cond_timedwait(&(_hdl)->cond, &(_mtx)->mtx_hdl, &cur_time);
	if (ret != 0){
		ret = convert_std_errno(ret);
		goto ERR_0;
	}

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "%s() : failed with retval %i\n",__func__, ret );
	return ret;
}



static char* lib_thread__strsched(enum process_sched _sched)
{
	switch (_sched)
	{
		case PROCESS_SCHED_other : return "SCHED_OTHER";
		case PROCESS_SCHED_fifo  : return "SCHED_FIFO";
		case PROCESS_SCHED_rr    : return "SCHED_RR";
		case PROCESS_SCHED_batch : return "SCHED_BATCH";
		case PROCESS_SCHED_idle	 : return "SCHED_IDLE";
		default: return NULL;

	}
}

void lib_thread__signal_pthread_cancel_handler(void *_hdl)
{
	signal_hdl_t sgn_hdl = (signal_hdl_t)_hdl;

	sgn_hdl->number_of_waiting_threads--;

	pthread_mutex_unlock(&sgn_hdl->mtx_hdl);

}

/* *************************************************************************//**
 * \brief	function for all *timedwait interfaces to convert relative tmoms to abs. timespec structure
 * \param	_clock_src		 	the clock source for conversion (CLOCK_REALTIME or CLOCK_MONOTONIC)
 * 			_tmoms				timeout in ms
 * 		    *_timespec [out]	timespec as output
 * \return	EOK			Success
 *			EPAR_NULL	NULL pointer specified for _thread or _start_routine (or _name on OSEK)
 *			ESTD_EINVAL	_clock_src does not specify a known clock.
 *			ESTD_RANGE	The number of seconds will not fit in an object of type time_t
 * ****************************************************************************/
static int lib_thread__convert_relative2abstime(const int _clock_src, const unsigned _tmoms, struct timespec *_timespec)
{

	/* param check */
	if (_timespec == NULL){
		return -EPAR_NULL;
	}

	/* get current time */
	if (clock_gettime(_clock_src, _timespec) != 0) {
		return convert_std_errno(errno);
	}

	/* calculate absolute timeout value */
	_timespec->tv_nsec += ((int)_tmoms % 1000) * 1000000;
	if (_timespec->tv_nsec > 1000000000){
		_timespec->tv_nsec -= 1000000000;
		_timespec->tv_sec++;
	}
	_timespec->tv_sec += (int)_tmoms / 1000;

	return EOK;
}
