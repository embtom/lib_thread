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
 *
 * \param	_hdl			[in] :	handle for the thread to be canceled
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__getname(thread_hdl_t _hdl, char * _name, int _maxlen);

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
 *
 * \param	_hdl			[in] :	the handle of a mutex object to lock
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__mutex_lock (mutex_hdl_t _hdl);

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
int lib_thread__mutex_unlock (mutex_hdl_t _hdl);

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
int lib_thread__mutex_trylock (mutex_hdl_t _hdl);

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
int lib_thread__signal_init (signal_hdl_t *_hdl);

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
int lib_thread__signal_destroy (signal_hdl_t *_hdl);

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
int lib_thread__signal_send (signal_hdl_t _hdl);

/* *******************************************************************
 * \brief	Waiting for an signal
 * ---------
 * \remark	The calling thread blocks unit the corresponding signal get be triggered.
 *
 * ---------
 *
 * \param	_hdl			[in] : 	handle to a signal object to wait
 *
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
 *
 * \param	_hdl			[in] : 	handle to a signal object to wait
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_wait_timedwait (signal_hdl_t _hdl, unsigned int _milliseconds);

/* *******************************************************************
 * \brief	Initialization of a semaphore object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a semaphore object
 * 										to initialize
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_init (sem_hdl_t *_hdl, int _count);

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
int lib_thread__sem_destroy (sem_hdl_t *_hdl);

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
int lib_thread__sem_post (sem_hdl_t _hdl);

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
int lib_thread__sem_wait (sem_hdl_t _hdl);

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
int lib_thread__sem_trywait (sem_hdl_t _hdl);


int lib_thread__msleep (unsigned int _milliseconds);

#ifdef __cplusplus
}
#endif

#endif /* LIB_THREAD_H_ */
