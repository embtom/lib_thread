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


#ifndef LIB_THREAD_H_
#define LIB_THREAD_H_

#ifdef __cplusplus
template<class T, void(T::*mem_fn)()>
void* thunk_ThreadCreate(void* p)
{
	/* call member function at address "mem_fn" by dereferencing "p", which is a (type-casted) pointer to an object of class "T"... */
	((T*)(p)->*mem_fn)();
	return 0;
}
#endif



#ifdef __cplusplus
extern "C" {
#endif

/* *******************************************************************
 * includes
 * ******************************************************************/

/* *******************************************************************
 * defines
 * ******************************************************************/
#define LIB_THREAD__THD_INITIALIZER {NULL}
#define LIB_THREAD__MTX_INITIALIZER {NULL}
#define LIB_THREAD__SGN_INITIALIZER {NULL}
#define LIB_THREAD__SEM_INITIALIZER {NULL}

#define LIB_THREAD__TIMEOUT_INFINITE	-1


/* *******************************************************************
 * custom data types (e.g. enumerations, structures, unions)
 * ******************************************************************/
enum process_sched
{
	PROCESS_SCHED_other,
	PROCESS_SCHED_fifo,
	PROCESS_SCHED_rr,
	PROCESS_SCHED_batch,
	PROCESS_SCHED_idle
};


typedef struct thread_hdl_attr *thread_hdl_t;
typedef struct mutex_hdl_attr *mutex_hdl_t;
typedef struct signal_hdl_attr *signal_hdl_t;
typedef struct sem_hdl_attr *sem_hdl_t;
typedef struct condvar_hdl_attr *cond_hdl_t;

typedef void* (thread_worker_t)(void *);

/* *******************************************************************
 * function declarations
 * ******************************************************************/

/* *******************************************************************
 * \brief	Initialization of the lib_thread
 * ---------
 * \remark  The call of the init routine is only be allowed at the main
 * 			routine of a process
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
int lib_thread__init(enum process_sched _sched, int _pcur);

/* *******************************************************************
 * \brief	Creation of thread worker
 * ---------
 * \remark
 * ---------
 *
 * \param	*_hdl			[out] :		pointer to a handle for the created thread
 * \param	_worker			[in]  : 	pointer to a worker routine
 * \param	*_arg			[in]  :		pointer to a worker routine argument
 * \param	*_relative_priority	  : 	relative priority to the parent thread/process
 * 										The parameter is only relevant if PROCESS_SCHED_fifo or PROCESS_SCHED_rr
 * 										is selected.
 * \param	*_thread_name	[in]  :		pointer to the thread name is optional
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__create (thread_hdl_t *_hdl, thread_worker_t *_worker, void *_arg, int _relative_priority, const char *_thread_name);

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
int lib_thread__join (thread_hdl_t *_hdl, void **_ret_val);

/* *******************************************************************
 * \brief	Request to cancel a working thread
 * ---------
 * \remark  The execution of a thread stops only at cancellation points
 * ---------
 *
 * \param	_hdl			[in/out] :	handle for the thread to be canceled
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__cancel(thread_hdl_t _hdl);

/* *******************************************************************
 * \brief	Get the thread name
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in] :	handle for the thread to be canceled
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__getname(thread_hdl_t _hdl, char * _name, int _maxlen);

/* *******************************************************************
 * \brief	The calling task sleeps for a defined time
 * ---------
 * \remark
 * ---------
 * \param	_hdl			:	milliseconds to sleep
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__msleep (unsigned int _milliseconds);

/* *******************************************************************
 * \brief	Initialization of a mutex object
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in/out] :	pointer to the handle of a mutex object
 * 										to initialize
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_init (mutex_hdl_t *_hdl);

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
int lib_thread__mutex_destroy (mutex_hdl_t *_hdl);

/* *******************************************************************
 * \brief	Look of an mutex
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in] :	the handle of a mutex object to lock
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_lock (mutex_hdl_t _hdl);

/* *******************************************************************
 * \brief	Unlock of an mutex object
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in] :	the handle of a mutex object to lock
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_unlock (mutex_hdl_t _hdl);

/* *******************************************************************
 * \brief	Trylock of an mutex object
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in] :	the handle of a mutex object to trylock
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_trylock (mutex_hdl_t _hdl);

/* *******************************************************************
 * \brief	Initialization of a signal object
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in/out] :	pointer to the handle of a signal object
 * 										to initialize
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_init (signal_hdl_t *_hdl);

/* *******************************************************************
 * \brief	Destroys an signal object again
 * ---------
 * \remark	All waiting threads on a signal_wait condition unblocks with EPERM
 * 			It can be used for a worker thread termination
 * ---------
 * \param	_hdl			[in/out] :	pointer to the handle of a signal object
 * 										to destroy
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_destroy (signal_hdl_t *_hdl);

/* *******************************************************************
 * \brief	Emit a signal
 * ---------
 * \remark	If a signal is emitted without any waiting thread the signal
 * 			gets lost
 * ---------
 * \param	_hdl			[in] : 	handle to a signal object to trigger
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_send (signal_hdl_t _hdl);

/* *******************************************************************
 * \brief	Waiting for an signal
 * ---------
 * \remark	The calling thread blocks unit the corresponding signal get be triggered.
 *
 * ---------
 * \param	_hdl			[in] : 	handle to a signal object to wait
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_wait (signal_hdl_t _hdl);

/* *******************************************************************
 * \brief	Waiting for an signal with an time limit
 * ---------
 * \remark	The calling thread blocks unit the corresponding signal is triggered or
 * 			the time runs out
 *
 * ---------
 * \param	_hdl			[in] : 	handle to a signal object to wait
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_timedwait (signal_hdl_t _hdl, unsigned int _milliseconds);

/* *******************************************************************
 * \brief	Initialization of a semaphore object
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in/out] :	pointer to the handle of a semaphore object
 * 										to initialize
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_init (sem_hdl_t *_hdl, int _count);

/* *******************************************************************
 * \brief	Destroys a semaphore again
 * ---------
 * \remark
 * ---------
 * \param	_hdl			[in/out] :	pointer to the handle of a semaphore object
 * 										to destroy
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_destroy (sem_hdl_t *_hdl);

/* *******************************************************************
 * \brief	Incrementation of a semaphore,
 * ---------
 * \remark	To increment the value of a semaphore
 * ---------
 * \param	_hdl				[in] :	handle of a semaphore object
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_post (sem_hdl_t _hdl);

/* *******************************************************************
 * \brief	Decrement semaphore and if count <=0 calling thread is blocked
 * ---------
 * \remark	If the value of the semaphore is negative, the calling process blocks;
 * 			one of the blocked processes wakes up when another process calls sem_post
 * ---------
 * \param	_hdl				[in] :	handle of a semaphore object
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_wait (sem_hdl_t _hdl);

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
int lib_thread__sem_timedwait (sem_hdl_t _hdl, int _milliseconds);

/* *******************************************************************
 * \brief	Decrement semaphore and if count <=0 return with error
 * ---------
 * \remark	If the value of the semaphore is negative, the calling process blocks;
 * 			one of the blocked processes wakes up when another process calls sem_post
 * ---------
 * \param	_hdl			[in] :	handle of a semaphore object
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_trywait (sem_hdl_t _hdl);

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
int lib_thread__cond_init(cond_hdl_t *_hdl);

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
int lib_thread__cond_destroy(cond_hdl_t *_hdl);

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
int lib_thread__cond_signal(cond_hdl_t _hdl);

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
int lib_thread__cond_broadcast(cond_hdl_t _hdl);

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
int lib_thread__cond_wait(cond_hdl_t _hdl, mutex_hdl_t _mtx);

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
int lib_thread__cond_timedwait(cond_hdl_t _hdl, mutex_hdl_t _mtx, int _tmoms);


#ifdef __cplusplus
}
#endif

#endif /* LIB_THREAD_H_ */
