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
#define LIB_THREAD_MODULE_ID 		"LIB_THREAD"


/* *******************************************************************
 * custom data types (e.g. enumerations, structures, unions)
 * ******************************************************************/
struct thread_hdl_attr
{
	pthread_t thread_hdl;
};

struct mutex_hdl_attr
{
	pthread_mutex_t	mtx_hdl;
};

struct signal_hdl_attr
{
	pthread_cond_t cond_hdl;
	pthread_mutex_t mtx_hdl;
	unsigned int number_of_waiting_threads;
	unsigned int number_of_outstanding_signals;
	unsigned int destroy_active;
};

struct sem_hdl_attr
{
	sem_t sem_hdl;
};

/* *******************************************************************
 * static function declarations
 * ******************************************************************/
static char* lib_thread__strsched(enum process_sched _sched);
void lib_thread__signal_pthread_cancel_handler(void *_hdl);


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

	struct sched_param	param;

	pid_t	pid;

	/* Get ID of actual process */
	pid = getpid();


	ret = sched_getscheduler(pid);
	if (ret == -1)
	{
		ret = -errno;
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
		default:
		{
			ret = -EINVAL;
			goto ERR_0;
		}
	}

	ret = sched_setscheduler(pid, (int)sched, &param);
	if(ret == -1)
	{
		ret = -errno;
		goto ERR_0;
	}

	ret = sched_getscheduler(pid);
	if(ret == -1)
	{
		ret = -errno;
		goto ERR_0;
	}

	if(ret != (int)_sched)
	{
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__init :  failed policy set (Policy from %s to %s with prio %u)\n", lib_thread__strsched(old_sched), lib_thread__strsched(_sched), _pcur);
		return -EACCES;
	}
	else
	{
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__init :  successfully (Policy from %s to %s with prio %u)\n", lib_thread__strsched(old_sched), lib_thread__strsched(_sched), _pcur);
		return 0;
	}


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__init : failed with retval %i\n", ret );

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
	int ret;
	int prio_min, prio_max, thread_prio;
	struct sched_param	priority_param;
	enum process_sched  sched;

	pthread_attr_t thread_attr;
	thread_hdl_t hdl;

	if ((_hdl == NULL) || (_worker == NULL))
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* Request of the adjusted scheduling parameters of the calling thread or process */
	ret = pthread_getschedparam(pthread_self(), (int*)&sched, &priority_param);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	prio_min = sched_get_priority_min(sched);
	prio_max = sched_get_priority_max(sched);
	if((prio_min == -1) || (prio_max == -1))
	{
		ret = -errno;
		goto ERR_0;
	}

	/* Priority check is only necessary if PROCESS_SCHED_fifo or PROCESS_SCHED_rr is selected during the init */
	if (prio_min == prio_max)
	{
		thread_prio = prio_min;
	}
	else
	{
		thread_prio = priority_param.__sched_priority + _relative_priority;
		if ((thread_prio < prio_min ) || (thread_prio > prio_max ))
		{
			ret = -ERANGE;
			goto ERR_0;
		}
	}

	ret = pthread_attr_init(&thread_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	/*Specifies that the scheduling policy and associated attributes are to be set to the corresponding values from this attribute object*/
	ret = pthread_attr_setinheritsched(&thread_attr, PTHREAD_EXPLICIT_SCHED);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_1;
	}

	/* Set scheduling policy at attribute object */
	ret = pthread_attr_setschedpolicy(&thread_attr, (int)sched);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_1;
	}

	priority_param.__sched_priority = thread_prio;
	ret = pthread_attr_setschedparam(&thread_attr, &priority_param);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_1;
	}

	hdl = malloc(sizeof (struct thread_hdl_attr));
	if(hdl == NULL)
	{
		ret = -ENOMEM;
		goto ERR_1;
	}

	/* Creation of thread */
	ret = pthread_create(&hdl->thread_hdl, &thread_attr, _worker, _arg);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_2;
	}

	ret = pthread_attr_destroy(&thread_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_3;
	}

	if (_thread_name != NULL)
	{
		pthread_setname_np(hdl->thread_hdl,_thread_name);
	}


	if (_thread_name == NULL)
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__create :  successfully (Thread ID '%u' Name '<NO NAME>' prio: '%u')\n", hdl->thread_hdl, thread_prio);
	else
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__create :  successfully (Thread ID '%u' Name '%s' prio: '%u')\n", hdl->thread_hdl, _thread_name, thread_prio);


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

	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__create : failed with retval %i\n", ret );

	*_hdl = NULL;
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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL)
	{
		ret = -ENOENT;
		goto ERR_0;
	}

	ret = pthread_join((*_hdl)->thread_hdl, _ret_val);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	threadid = (*_hdl)->thread_hdl;

	free(*_hdl);
	(*_hdl) = NULL;

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__join :  successfully (Thread ID '%u')\n", threadid);

	return EOK;


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__join : failed with retval %i\n", ret );

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
	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* cancel thread */
	ret = pthread_cancel(_hdl->thread_hdl);
	if (ret != EOK){
		ret = -ret;
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__cancel :  successfully (Thread ID '%u')\n", _hdl->thread_hdl);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__cancel : failed with retval %i\n", ret );

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
	int ret;

	if ((_hdl == NULL) || (_name == NULL))
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_getname_np(_hdl->thread_hdl,_name, _maxlen);
	if (ret != 0)
	{
		/* Mapping of the return vales to the more common on of the libpthread */

		if (ret == EINVAL)  ret = -ERANGE;
		if (ret == ENOENT)  ret = -ESRCH;


		return -ret;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__getname :  successfully (Thread ID '%u')\n", _hdl->thread_hdl);

	return EOK;


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__getname : failed with retval %i\n", ret );

	return ret;


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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}


	ret = pthread_mutexattr_init(&mutex_attr);
	if (ret != EOK)
	{
		ret = -ret;
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
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_1;
	}

	ret = pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_1;
	}

	hdl = malloc (sizeof(struct mutex_hdl_attr));
	if (hdl == NULL)
	{
		ret = -ENOMEM;
		goto ERR_1;
	}

	ret = pthread_mutex_init(&hdl->mtx_hdl,&mutex_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_2;
	}

	ret = pthread_mutexattr_destroy(&mutex_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	*_hdl = hdl;
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__mutex_init :  successfully (Mutex ID '%u')\n", (unsigned int)&hdl->mtx_hdl);

	return EOK;


	ERR_2:
	free(hdl);


	ERR_1:
	pthread_mutexattr_destroy(&mutex_attr);


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__mutex_init : failed with retval %i\n", ret );

	return -ret;


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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL)
	{
		ret = -ENOENT;
		goto ERR_0;
	}

	/*Check if mutex is locked*/
	ret = pthread_mutex_trylock(&(*_hdl)->mtx_hdl);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	ret = pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	mutex_id = (unsigned int)&(*_hdl)->mtx_hdl;

	ret = pthread_mutex_destroy(&(*_hdl)->mtx_hdl);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	free(*_hdl);
	*_hdl = NULL;
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__mutex_destroy :  successfully (Mutex ID '%u')\n", mutex_id);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__mutex_destroy : failed with retval %i\n", ret );

	return -ret;
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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__mutex_lock :  successfully (Mutex ID '%u')\n", &_hdl->mtx_hdl);

	return EOK;


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__mutex_lock : failed with retval %i\n", ret );

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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__mutex_unlock :  successfully (Mutex ID '%u')\n", &_hdl->mtx_hdl);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__mutex_unlock : failed with retval %i\n", ret );

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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__mutex_trylock : failed with retval %i\n", ret );
		return ret;
	}

	ret = pthread_mutex_trylock(&_hdl->mtx_hdl);
	if (ret != EOK)
	{
		msg (LOG_LEVEL_warning, LIB_THREAD_MODULE_ID, "lib_thread__mutex_trylock : Mutex already locked (Mutex ID '%u')\n", &_hdl->mtx_hdl);
		ret = -ret;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__mutex_trylock : successfully (Mutex ID '%u')\n", &_hdl->mtx_hdl);
	return EOK;
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
	if (ret < EOK)
	{
		msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__msleep : failed with retval %i\n", -errno );
		return -errno;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__msleep : successfully\n" );
	return EOK;
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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_condattr_init(&cond_attr);
	if( ret != EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	ret = pthread_mutexattr_init(&mtx_attr);
	if( ret != EOK)
	{
		ret = -ret;
		goto ERR_1;
	}


	ret = pthread_condattr_setclock(&cond_attr,CLOCK_MONOTONIC);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_2;
	}

	ret = pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_ERRORCHECK);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_2;
	}


	hdl = malloc (sizeof (struct signal_hdl_attr));
	if(hdl == NULL)
	{
		ret = -ENOMEM;
		goto ERR_1;
	}

	ret = pthread_cond_init(&hdl->cond_hdl, &cond_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_3;
	}

	ret = pthread_mutex_init(&hdl->mtx_hdl, &mtx_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_4;
	}

	ret = pthread_condattr_destroy(&cond_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_5;
	}

	ret = pthread_mutexattr_destroy(&mtx_attr);
	if (ret != EOK)
	{
		ret = -ret;
		goto ERR_5;
	}

	hdl->number_of_waiting_threads = 0;
	hdl->number_of_outstanding_signals = 0;
	hdl->destroy_active = 0;
	*_hdl = hdl;

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__signal_init :  successfully (Signal ID '%u')\n", &hdl->cond_hdl);

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
	*_hdl = NULL;
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__signal_init : failed with retval %i\n", ret );

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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL)
	{
		ret = -ENOENT;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&(*_hdl)->mtx_hdl);
	if (ret != 0)
	{
		ret = -ret;
		goto ERR_0;
	}

	/*Check if signal this signal destroying is actual active */
	if ((*_hdl)->destroy_active > 0)
	{
		ret = -EBUSY;
		goto ERR_1;
	}

	(*_hdl)->destroy_active = 1;
	tmp_number_of_outstanding_signals = (*_hdl)->number_of_outstanding_signals;
	(*_hdl)->number_of_outstanding_signals = 0;


//	pthread_mutex_unlock(&(*_hdl)->mtx_hdl);

	if (tmp_number_of_outstanding_signals > 0)
	{
		ret = pthread_cond_broadcast(&(*_hdl)->cond_hdl);
		if (ret != 0)
		{
			(*_hdl)->number_of_outstanding_signals = tmp_number_of_outstanding_signals;
			(*_hdl)->destroy_active = 0;
			goto ERR_1;
		}

		while ((*_hdl)->number_of_waiting_threads)
		{
			/* check the number of threads still waiting */

			pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
			/* wait briefly to give the waiting threads some execution time */
			lib_thread__msleep(5);
		//fixme check if lock is necesarry.
		//	pthread_mutex_lock(&(*_hdl)->mtx_hdl);
		}
	}

	ret = pthread_mutex_lock(&(*_hdl)->mtx_hdl);
	if (ret != 0)
	{
		ret = -ret;
		goto ERR_1;
	}

	signal_id = &(*_hdl)->cond_hdl;

	ret = pthread_cond_destroy(&(*_hdl)->cond_hdl);
	if (ret != 0)
	{
		ret = -ret;
		(*_hdl)->destroy_active = 0;
		goto ERR_1;
	}

	ret = pthread_mutex_unlock(&(*_hdl)->mtx_hdl);
	if (ret != 0)
	{
		ret = -ret;
		(*_hdl)->destroy_active = 0;
		goto ERR_0;
	}

	ret = pthread_mutex_destroy(&(*_hdl)->mtx_hdl);
	if (ret != 0)
	{
		ret = -ret;
		(*_hdl)->destroy_active = 0;
		goto ERR_0;
	}


	free(*_hdl);
	*_hdl = NULL;
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__signal_destroy :  successfully (Signal ID '%u')\n", signal_id);



	return EOK;


	ERR_1:
	pthread_mutex_unlock(&(*_hdl)->mtx_hdl);

	ERR_0:

	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__signal_init : failed with retval %i\n", ret );

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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret < EOK)
	{
		goto ERR_0;
	}

	if (_hdl->destroy_active)
	{
		ret = -EPERM;
		goto ERR_1;
	}


	/* Check if somebody is waiting for signal */
	if (_hdl->number_of_outstanding_signals > 0)
	{
		/* Decrement outstanding signal counter, because one open signal slot could be served */
		_hdl->number_of_outstanding_signals--;
		ret = pthread_cond_signal(&_hdl->cond_hdl);
		if (ret != EOK)
		{
			/*Increment it again, because trigger slot was not successfully served */
			_hdl->number_of_outstanding_signals++;
			ret = -ret;
			goto ERR_1;
		}
	}


	ret = pthread_mutex_unlock(&_hdl->mtx_hdl);
	if (ret < EOK)
	{
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__signal_send :  successfully (Signal ID '%u')\n", (unsigned int)&_hdl->cond_hdl);

	return EOK;


	ERR_1:
	pthread_mutex_unlock(&_hdl->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__signal_send : failed with retval %i\n", ret );

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
	int ret;
	unsigned int signal_id;

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret < EOK)
	{
		ret = -ret;
		goto ERR_0;
	}

	if (_hdl->destroy_active)
	{
		pthread_mutex_unlock(&_hdl->mtx_hdl);
		ret = -EPERM;
	}
	else
	{
		_hdl->number_of_waiting_threads++;
		_hdl->number_of_outstanding_signals++;

		/*Set a cleanup handler to threat the thread cancellation */
		pthread_cleanup_push(&lib_thread__signal_pthread_cancel_handler, _hdl);

		signal_id = &_hdl->cond_hdl;

		/* A check if the thread unblocking was no spurious wakeup*/
		while (1)
		{

			ret = pthread_cond_wait(&_hdl->cond_hdl, &_hdl->mtx_hdl);
			if (ret != EOK)
			{
				_hdl->number_of_waiting_threads--;
				_hdl->number_of_outstanding_signals--;
				pthread_mutex_unlock(&_hdl->mtx_hdl);
				break;
			}

			if (_hdl->destroy_active)
			{
				pthread_mutex_unlock(&_hdl->mtx_hdl);
			//	_hdl->number_of_waiting_threads--;
				ret = -EPERM;
				break;
			}

		}

		pthread_cleanup_pop(0);
	}

	if (ret == -EPERM)
	{
		msg (LOG_LEVEL_debug_prio_2, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait :  unblocked destroyed (Signal ID '%u')\n", signal_id);
	}
	else
	{
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait :  unblocked successfully (Signal ID '%u')\n", signal_id);
	}

	return EOK;


	ERR_1:
	pthread_mutex_unlock(&_hdl->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait : failed with retval %i\n", ret );

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
int lib_thread__signal_wait_timedwait (signal_hdl_t _hdl, unsigned int _milliseconds)
{
	int ret = EOK;
	struct timespec actual_time;
	unsigned int signal_id;

	if (_hdl == NULL)
		return -EPAR_NULL;

	ret = clock_gettime(CLOCK_MONOTONIC, &actual_time);
	if(ret != 0)
	{
		ret = -errno;
		goto ERR_0;
	}

	actual_time.tv_nsec += (_milliseconds % 1000) * 1000000;
	if (actual_time.tv_nsec > 1000000000)
	{
		actual_time.tv_nsec -= 1000000000;
		actual_time.tv_sec++;
	}
	actual_time.tv_sec += _milliseconds / 1000;


	ret = pthread_mutex_lock(&_hdl->mtx_hdl);
	if (ret < EOK)
	{
		goto ERR_0;
	}

	if (_hdl->destroy_active)
	{
		pthread_mutex_unlock(&_hdl->mtx_hdl);
		ret = -EPERM;
	}
	else
	{
		_hdl->number_of_waiting_threads++;
		_hdl->number_of_outstanding_signals++;

		pthread_cleanup_push(&lib_thread__signal_pthread_cancel_handler, _hdl);

		signal_id = &_hdl->cond_hdl;

		while (1)
		{
			ret = pthread_cond_timedwait(&_hdl->cond_hdl, &_hdl->mtx_hdl,&actual_time);
			if (ret != EOK)
			{
				ret = -ret;
				if (ret == -ETIMEDOUT)
				{
					// A signal was triggered and is pending, but the threas awaked by out running time.
					if (_hdl->number_of_outstanding_signals != _hdl->number_of_waiting_threads)
					{
						ret = EOK;
						pthread_mutex_unlock(&_hdl->mtx_hdl);
						break;
					}
				}

				_hdl->number_of_waiting_threads--;
				_hdl->number_of_outstanding_signals--;
				pthread_mutex_unlock(&_hdl->mtx_hdl);
				break;
			}
			else
			{
				_hdl->number_of_waiting_threads--;
				break;
			}

			if (_hdl->destroy_active)
			{

				pthread_mutex_unlock(&_hdl->mtx_hdl);
			//	_hdl->number_of_waiting_threads--;
				ret = -EPERM;
				break;
				//goto ERR_0;
			}

		}
		//while (_hdl->number_of_outstanding_signals == _hdl->number_of_waiting_threads);

		pthread_cleanup_pop(0);
	}


	if (ret == -ETIMEDOUT)
	{
		msg (LOG_LEVEL_debug_prio_2, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait_timedwait :  unblocked timeout (Signal ID '%u')\n", signal_id);
	}
	else if (ret == -EPERM)
	{
		msg (LOG_LEVEL_debug_prio_2, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait_timedwait :  unblocked destroyed (Signal ID '%u')\n", signal_id);
	}
	else
	{
		msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait_timedwait :  unblocked successfully (Signal ID '%u')\n", signal_id);
	}
	return ret;

	ERR_1:
	pthread_mutex_unlock(&_hdl->mtx_hdl);

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__signal_wait_timedwait : failed with retval %i\n", ret );

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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	hdl = malloc(sizeof (struct sem_hdl_attr));
	if (hdl == NULL)
	{
		ret = -ENOMEM;
		goto ERR_0;
	}

	/* initialize semaphore */
	if (sem_init(&hdl->sem_hdl, 0, _count) != 0)
	{
		ret = -errno;
		goto ERR_1;
	}

	*_hdl = hdl;
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__sem_init :  successfully (Semaphore ID '%u')\n", (unsigned int)&(*_hdl)->sem_hdl);

	return EOK;


	ERR_1:
	free(hdl);
	*_hdl = NULL;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__sem_init : failed with retval %i\n", ret );
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
	unsigned int sem_id;

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL)
	{
		ret = -ENOENT;
		goto ERR_0;
	}

	sem_id = (unsigned int)&(*_hdl)->sem_hdl;

	if (sem_destroy(&(*_hdl)->sem_hdl) != 0)
	{
		ret = -errno;
		goto ERR_0;
	}


	free(*_hdl);
	*_hdl = NULL;
	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__sem_destroy :  successfully (Semaphore ID '%u')\n", sem_id);


	return EOK;


	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__sem_destroy : failed with retval %i\n", ret );
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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* try to lock on semaphore */
	if (sem_post(&_hdl->sem_hdl) != 0)
	{
		ret = -errno;
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__sem_post :  successfully (Semaphore ID '%u')\n", (unsigned int)&_hdl->sem_hdl);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__sem_post : failed with retval %i\n", ret );
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

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* try to lock on semaphore */
	if (sem_wait(&_hdl->sem_hdl) != 0)
	{
		ret = -errno;
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__sem_wait :  successfully (Semaphore ID '%u')\n", (unsigned int)&_hdl->sem_hdl);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__sem_wait : failed with retval %i\n", ret );
	return ret;
}

/* *******************************************************************
 * \brief	Decrement semaphore and if count <=0 return with error
 * ---------
 * \remark	If the value of the semaphore is negative, the calling process blocks;
 * 			one of the blocked processes wakes up when another process calls sem_post
 * ---------
 *
 * \param	_hdl			[in] :	handle of a semaphore object
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_trywait (sem_hdl_t _hdl)
{
	int ret;

	if (_hdl == NULL)
	{
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* try to lock on semaphore */
	if (sem_trywait(&_hdl->sem_hdl) != 0)
	{
		ret = -errno;
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug_prio_1, LIB_THREAD_MODULE_ID, "lib_thread__sem_trywait :  successfully (Semaphore ID '%u')\n", (unsigned int)&_hdl->sem_hdl);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, LIB_THREAD_MODULE_ID, "lib_thread__sem_trywait : failed with retval %i\n", ret );
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


