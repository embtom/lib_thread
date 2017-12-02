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
#include <limits.h>


/* system */
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <fcntl.h>

/* project*/
#include "lib_convention__errno.h"
#include "lib_thread.h"
#include "lib_log.h"
#include "lib_thread_wakeup.h"

/* *******************************************************************
 * defines
 * ******************************************************************/
#define SIGMIN		SIGRTMIN
#define SIGMAX		SIGRTMAX

#define M_DEV_TIMER_UNUSED			0
#define M_DEV_TIMER_CONFIRMED		1
#define M_DEV_TIMER_REQUESTED		2

#define M_DEV_LIB_THREAD_WHP_MODULE_ID 		"LIB_THD_WHP"
#define M_DEV_MAP_INITIALZED	0xABCDABCD



/* *******************************************************************
 * custom data types (e.g. enumerations, structures, unions)
 * ******************************************************************/
struct signals_region {
	pthread_mutex_t mmap_mtx;
	unsigned int initialized;
	int used_signals_max_cnt;
	int used_signals_data;
};

/* *******************************************************************
 * (static) variables declarations
 * ******************************************************************/
static const int clock_src = CLOCK_MONOTONIC;
static struct signals_region *s_wakeup_signals_mmap = NULL;
static ssize_t s_wakeup_signals_mmap_size;
static int s_shm_fd = -1;
static unsigned s_wakeup_init_calls = 0;
static int *s_local_used_signals = NULL;

#ifdef _WIN32
int lib_thread__wakeup_init(void)
{
	if (s_wakeup_init_calls == 0) {
		used_signals_max_cnt = 0;
	}
	/* increment initialization count */
	s_wakeup_init_calls++;
	return EOK;
}
#else

/* wakeup handle object structure */
struct internal_wakeup {
#ifdef _WIN32
	unsigned		interval;	// timeout interval in ms
#else	/* any fully POSIX-compliant OS */
	timer_t			timer_id;	// timeout timer ID
	int				sig_ind;	// signal index, i.e. "relative" signal number
	sigset_t		sigset;		// the signal set on which to be woken up
#endif	/* _WIN32 or any fully POSIX-compliant OS */
};

/* *******************************************************************
 * Global Functions
 * ******************************************************************/

int lib_thread__wakeup_init(void)
{
	pthread_mutexattr_t mutex_attr;
	int i, ret_man, ret = EOK;
	int shm_oflag = 0, shm_fd;
	char shm_name[NAME_MAX]= {0};
	int shm_open_loop = 0;
	unsigned int signals_max_cnt = SIGMAX - SIGMIN + 1;
	sigset_t sigset;
	struct signals_region *wakeup_signals_mmap;
	ssize_t wakeup_signals_size;

	if (s_wakeup_signals_mmap != NULL) {
		/* check whether component is not (yet) initialized (any more) */
		s_wakeup_init_calls++;
		return EOK;
	}
	wakeup_signals_size = sizeof(struct signals_region) + signals_max_cnt * sizeof(int);

	/* creation of a IPC object:									*/
	/*	O_CREAT : 	Create the object if not exits					*/
	/* 	O_EXCL	:	If object already exits exit with EEXIST		*/
	/*	O_RDWR	: 	Read write access 								*/
	sprintf(&shm_name[0],"/%s_%u",program_invocation_short_name,getpid());

	do {

		shm_fd = shm_open(&shm_name[0], shm_oflag | O_RDWR, S_IRUSR | S_IWUSR);
		shm_oflag = O_CREAT | O_EXCL;
		if (shm_fd < 0) {
			ret = -errno;
		}

		if ((ret == EOK) && (shm_open_loop > 0)) {
			ret = ftruncate(shm_fd, wakeup_signals_size);
			if(ret != EOK) {
				close(shm_fd);
				ret = -EEXEC_FAILINIT;
			}
		}
		shm_open_loop++;
		ret = EOK;
	} while ((shm_fd < 0)&&(shm_open_loop < 2));

	if (ret < EOK) {
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::shm_open with %i",ret);
		return ret;
	}

	wakeup_signals_mmap = (struct signals_region *)mmap(NULL,wakeup_signals_size,PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (wakeup_signals_mmap == MAP_FAILED) {
		close(shm_fd);
		return -ENXIO;
	}

	s_local_used_signals = calloc(signals_max_cnt,sizeof(int));
	if(s_local_used_signals == NULL) {
		ret = -ENOMEM;
		close(shm_fd);
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::malloc error with %i",ret);
		return ret;
	}

	if (wakeup_signals_mmap->initialized == M_DEV_MAP_INITIALZED) {
		s_wakeup_init_calls++;
		s_shm_fd = shm_fd;
		s_wakeup_signals_mmap = wakeup_signals_mmap;
		s_wakeup_signals_mmap_size = wakeup_signals_size;
		return EOK;
	}

	memset((void*)wakeup_signals_mmap,0,wakeup_signals_size);

	pthread_mutexattr_init(&mutex_attr);
	pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&wakeup_signals_mmap->mmap_mtx, &mutex_attr);
	wakeup_signals_mmap->used_signals_max_cnt = signals_max_cnt;

	/*
	 * Block all reserved real-time signals so they can be used for the timers.
	 * This has to be done in main() before any threads are created so they all inherit the same mask.
	 * Doing it later is subject to race conditions.
	 */

	/* reset signal set */
	if (sigemptyset(&sigset) != 0){
		/* should actually never happen */
		ret = -errno;
		close(shm_fd);
		free(s_local_used_signals);
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigemptyset with %i",ret);
		return ret;
	}

	/* add all reserved real-time signals to the signal set */
	for (i = SIGMIN; i <= SIGMAX; i++)
	{
		if (sigaddset(&sigset, i) != 0){
			/* should actually never happen */
			ret = -errno;
			close(shm_fd);
			free(s_local_used_signals);
			msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigaddsetwith ret %i",ret);
			return ret;
		}
	}

	/* block on the configured signal set */
	ret = pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	if (ret != 0){
		ret = -errno;
		/* should actually never happen */
		close(shm_fd);
		free(s_local_used_signals);
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::pthread_sigmask", ret);
		return ret;
	}

	wakeup_signals_mmap->initialized = M_DEV_MAP_INITIALZED;
	s_shm_fd = shm_fd;
	s_wakeup_signals_mmap = wakeup_signals_mmap;
	s_wakeup_signals_mmap_size = wakeup_signals_size;

	/* increment initialization count */
	s_wakeup_init_calls=1;

	return EOK;
}

#endif

/* ************************************************************************//**
 * \brief	Cleanup wakeup component of lib_thread
 *
 * WARNINGS:
 *	-	This function is not thread-safe towards any function within this
 *		component.
 *
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			--EBUSY		There are still some wakeup objects currently in use
 * ****************************************************************************/
#ifdef _WIN32
int lib_thread__wakeup_cleanup(void)
{
	int ret;
	if (s_wakeup_init_calls == 0){
		ret = EEXEC_NOINIT;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"");
		return ret;
	}

	if (s_wakeup_init_calls == 1) {
		/* check whether there are any signals left which are currently in use */
		if (used_signals_max_cnt > 0){
			ret = -EBUSY;
			msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"");
			return ret;
		}
	}
	/* decrement initialization count */
	s_wakeup_init_calls--;
	return EOK;
}
#else

int lib_thread__wakeup_cleanup(void)
{
	int ret;
	int *used_signals;
	char shm_name[NAME_MAX]= {0};

	/* check whether component is properly initialized */
	if ((s_wakeup_signals_mmap == NULL) || (s_wakeup_init_calls == 0)) {
		ret = -EEXEC_NOINIT;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"");
		return ret;
	}

	/* check whether component should be unloaded */
	if (s_wakeup_init_calls == 1) {
		int i;

		used_signals = &s_wakeup_signals_mmap->used_signals_data;

		pthread_mutex_lock(&s_wakeup_signals_mmap->mmap_mtx);
		/* check whether there are any signals left which are currently in use */
		for (i = 0; i < s_wakeup_signals_mmap->used_signals_max_cnt; i++) {
			if (s_local_used_signals[i] != 0) {
				pthread_mutex_unlock(&s_wakeup_signals_mmap->mmap_mtx);
				ret = -EBUSY;
				msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"Wakeup");
				return ret;
			}
		}
		s_wakeup_init_calls = 0;
		pthread_mutex_unlock(&s_wakeup_signals_mmap->mmap_mtx);
		free(s_local_used_signals);
		munmap((void*)s_wakeup_signals_mmap,s_wakeup_signals_mmap_size);
		s_wakeup_signals_mmap_size = 0;
		close(s_shm_fd);
		s_shm_fd = -1;
		sprintf(&shm_name[0],"/%s_%u",program_invocation_short_name,getpid());
		shm_unlink(&shm_name[0]);
		return EOK;
	}

	/* decrement initialization count */
	s_wakeup_init_calls--;
	return EOK;
}

#endif

/* ************************************************************************//**
 * \brief	Create a new wakeup object with the specified interval
 *
 * The utilized time base is a strict monotonic system clock, and hence is not
 * affected when e.g. adjusting the local date and time of the system.
 * On successful creation, a reference to the wakeup object handle is stored in
 * _wu_obj.
 *
 * WARNINGS:
 *	-	If _wu_obj already points to a valid handle, the reference to that
 *		handle will be overwritten. This must be avoided by the user, as it
 *		would result in unrecoverable residual data in the memory.
 *	-	This function is not thread-safe towards any function within this
 *		component.
 *	-	This function must not be called from within an interrupt context.
 *	-	Per thread, only one wakeup object at a time is usable on OSEK.
 *
 * \param	*_wu_obj	[out]	pointer to handle of the wakeup object (will be allocated; only valid on successful return)
 * \param	_interval			wakeup interval in ms (must not be 0)
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_interval is 0
 *			-ESTD_NOMEM		High-level OSes only: Insufficient memory available to initialize the wakeup object
 *			-ESTD_AGAIN		High-level OSes except for Windows: All available signals or timers are currently in use
 * ****************************************************************************/
#ifdef _WIN32
int lib_thread__wakeup_create(wakeup_hdl_t *DECL_RESTRICT _wu_obj, unsigned _interval)
{
	/* check arguments */
	if (_wu_obj == NULL){
		ret = EPAR_NULL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", _wu_obj");
		return ret;
	}
	if (_interval == 0){
		ret = EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", _interval");
		return ret;
	}

	/* create thread handle on heap */
	*_wu_obj = malloc(sizeof(**_wu_obj));
	if (*_wu_obj == NULL){
		ret = -errno;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::malloc");
		return ret;
	}

	/* initialize wakeup object */
	(*_wu_obj)->interval = _interval;

	/* increment number of used signals */
	used_signals_max_cnt++;
	/* any fully POSIX-compliant OS */

	return EOK;
#else

int lib_thread__wakeup_create(wakeup_hdl_t *_wu_obj, unsigned _interval)
{
	int i, ret;
	struct itimerspec	itval;	/* timeout value structure */
	struct sigevent		sigev;	/* sigevent structure for storing the event type, the signal number and the value pointer */
	int *used_signals;

	/* check whether component is properly initialized */
	if (s_wakeup_signals_mmap == NULL) {
		ret = -EEXEC_NOINIT;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"not initialized");
		return ret;
	}

	/* check arguments */
	if (_wu_obj == NULL){
		ret = -EPAR_NULL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", _wu_obj");
		return ret;
	}
	if (_interval == 0){
		ret = -EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", _interval");
		return ret;
	}

	/* create thread handle on heap */
	*_wu_obj = malloc(sizeof(**_wu_obj));
	if (*_wu_obj == NULL){
		ret = -errno;;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::malloc");
		return ret;
	}

	pthread_mutex_lock(&s_wakeup_signals_mmap->mmap_mtx);

	used_signals = &s_wakeup_signals_mmap->used_signals_data;
	for (i = 0; i < s_wakeup_signals_mmap->used_signals_max_cnt; i++){
		if (used_signals[i] == M_DEV_TIMER_UNUSED){
			/* free slot found -> set signal as used and break loop */
			break;
		}
	}

	if(i >= s_wakeup_signals_mmap->used_signals_max_cnt) {
		/* maximum number of concurrently manageable signals has been reached -> return error */
		pthread_mutex_unlock(&s_wakeup_signals_mmap->mmap_mtx);
		ret = -EAGAIN;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", used_signals");
		free(*_wu_obj);
		return ret;
	}

	used_signals[i] = M_DEV_TIMER_REQUESTED;
	pthread_mutex_unlock(&s_wakeup_signals_mmap->mmap_mtx);


	/* initialize sigevent structure in the wakeup object */
	sigev.sigev_notify = SIGEV_SIGNAL;
	sigev.sigev_signo = i + SIGMIN;
	sigev.sigev_value.sival_ptr = &((*_wu_obj)->timer_id);

	/* create the signal mask that will be used in lib_thread__wakeup_wait */
	if (sigemptyset(&((*_wu_obj)->sigset)) != 0) {
		/* should actually never happen */
		used_signals[i] = M_DEV_TIMER_UNUSED;
		ret = -errno;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigemptyset");
		free(*_wu_obj);
		return ret;
	}

	if (sigaddset(&((*_wu_obj)->sigset), sigev.sigev_signo) != 0) {
		/* should actually never happen */
		used_signals[i] = M_DEV_TIMER_UNUSED;
		ret = -errno;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigaddset");
		free(*_wu_obj);
		return ret;
	}

	/* create a timer that will generate the signal we have chosen */
	if (timer_create(clock_src, &sigev, &((*_wu_obj)->timer_id)) != 0){
		/* error occurred -> remove signal from signal set and set signal as unused */
		used_signals[i] = M_DEV_TIMER_UNUSED;
		ret = -errno;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::timer_create");
		free(*_wu_obj);
		return ret;
	}

	/* set timer with the specified periodic timeout, firing immediately for the first time */
	itval.it_value.tv_sec		= 0;
	itval.it_value.tv_nsec		= 1;
	itval.it_interval.tv_sec	= (int)_interval / 1000;
	itval.it_interval.tv_nsec	= ((int)_interval % 1000) * 1000000;
	if (timer_settime((*_wu_obj)->timer_id, 0, &itval, NULL) != 0){
		/* should actually never happen */
		used_signals[i] = M_DEV_TIMER_UNUSED;
		ret = -errno;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::timer_settime");
		timer_delete((*_wu_obj)->timer_id);
		free(*_wu_obj);
		return ret;
	}

	/* remember that this signal is used */
	used_signals[i] = M_DEV_TIMER_CONFIRMED;
	s_local_used_signals[i] = M_DEV_TIMER_CONFIRMED;
	(*_wu_obj)->sig_ind = i;

	msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"Timer ID %i created\n", i);

	return EOK;
}

#endif

/* ************************************************************************//**
 * \brief	Destroy an initialized wakeup object
 *
 * Once successfully destroyed, the wakeup object handle is deleted from memory.
 * Further usage of the handle on interface function calls other than
 * lib_thread__wakeup_create() will result in an error.
 *
 * WARNINGS:
 *	-	If the wakeup object gets destroyed while still waited on by another
 *		thread, the behavior of the destruction call is undefined.
 *	-	This function is not thread-safe towards any function within this
 *		component.
 *	-	This function must not be called from within an interrupt context.
 *
 * \param	*_wu_obj	[in/out]	pointer to handle of the wakeup object (is only destroyed on successful return)
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_wu_obj is invalid
 * ****************************************************************************/
#ifdef _WIN32
int lib_thread__wakeup_destroy(wakeup_hdl_t *_wu_obj)
{
	int ret;

	/* check whether component is properly initialized */
	if (s_wakeup_init_calls == 0){
		ret = EEXEC_NOINIT;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"");
		return ret;
	}

	/* check arguments */
	if (_wu_obj == NULL){
		ret = EPAR_NULL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", _wu_obj");
		return ret;
	}

	if (*_wu_obj == NULL){
		ret = EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", *_wu_obj");
		return ret;
	}

	/* check number of currently used signals */
	if (used_signals_max_cnt == 0){
		/* No signal currently in use.
		 * This error can actually only happen if a destroy call is executed on an actually invalid wakeup object
		 * which is not detected as such. This is possible if the wakeup object has never been used before.
		 */
		ret = EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", *_wu_obj");
		*_wu_obj = NULL;
		return ret;
	}

	/* decrement number of used signals */
	used_signals_max_cnt--;

	/* free and invalidate wakeup object handle */
	free(*_wu_obj);
	*_wu_obj = NULL;

	return EOK;
}
#else

int lib_thread__wakeup_destroy(wakeup_hdl_t *_wu_obj)
{
	int ret, i;
	sigset_t	sigset;
	int *used_signals;

	/* check whether component is properly initialized */
	if ((s_wakeup_signals_mmap == NULL) || (s_wakeup_init_calls == 0)) {
		ret = EEXEC_NOINIT;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"");
		return ret;
	}

	/* check arguments */
	if (_wu_obj == NULL){
		ret = EPAR_NULL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", _wu_obj");
		return ret;
	}

	if (*_wu_obj == NULL){
		ret = EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", *_wu_obj");
		return ret;
	}

	pthread_mutex_lock(&s_wakeup_signals_mmap->mmap_mtx);

	used_signals = &s_wakeup_signals_mmap->used_signals_data;
	/* check for a free slot in the signal list */
	for (i = 0; i < s_wakeup_signals_mmap->used_signals_max_cnt; i++){
		if (used_signals[i] != 0){
			/* used slot found -> break */
			break;
		}
	}

	if (i >= s_wakeup_signals_mmap->used_signals_max_cnt) {
		/* No signal currently in use.
		 * This error can actually only happen if a destroy call is executed on an actually invalid wakeup object
		 * which is not detected as such. This is possible if the wakeup object has never been used before.
		 */
		pthread_mutex_unlock(&s_wakeup_signals_mmap->mmap_mtx);
		ret = EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,", *_wu_obj");
		*_wu_obj = NULL;
		return ret;
	}

	pthread_mutex_unlock(&s_wakeup_signals_mmap->mmap_mtx);

	/* delete timer associated with the specified wakeup object */
	if (timer_delete((*_wu_obj)->timer_id) != 0){
		/* should actually never happen, hence we'll just continue */
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::timer_delete");
	}

	/* check whether a signal from this wakeup object's timer is pending */
	if (sigpending(&sigset) != 0){
		/* should actually never happen, hence we'll just continue */
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigpending");
	}else{
		int sig;

		switch (sigismember(&sigset, (*_wu_obj)->sig_ind + SIGMIN)){
			case 0:		/* signal not pending -> just continue */
				break;
			case 1:		/* signal pending -> "wait" for it */
				/* wait for specified alarm signal to be raised */
				ret = sigwait(&sigset, &sig);
				if (ret != 0){
					/* Should actually never happen, except if a signal is received.
					 * In that case, the error will be reported but not actually handled,
					 * as not all high-level OSes are able to report that error.
					 */
					msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigwait");
				}
				break;
			default:	/* error occurred (should actually never happen) */
				msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigismember");
				break;
		}
	}

		/* set signal as unused in signal list */
	used_signals[(*_wu_obj)->sig_ind] = 0;
	s_local_used_signals[(*_wu_obj)->sig_ind] = 0;


	msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"Timer ID %i destroyed\n", (*_wu_obj)->sig_ind);

	/* free and invalidate wakeup object handle */
	free(*_wu_obj);
	*_wu_obj = NULL;

	return EOK;
}
#endif


/* ************************************************************************//**
 * \brief	Wait on the specified wakeup object
 *
 * This function waits until the specified wakeup object's timeout interval
 * has elapsed. Since this is not synchronized with the actual function call,
 * the call may unblock virtually instantly, particularly when being executed
 * for the first time. Hence, the user should call this function at the top of
 * a while or for loop (rather than the bottom).
 * This function employs a cancellation point on waiting for the wakeup object.
 *
 * WARNINGS:
 *	-	If the wakeup object gets destroyed by another thread while still
 *		waiting on it, the behavior of the waiting call(s) is undefined.
 *	-	This function is not thread-safe towards any function within this
 *		component.
 *	-	This function must not be called from within an interrupt context.
 *
 * \param	*_wu_obj	[in/out]	pointer to handle of the wakeup object
 * \return	EOK				Success
 *			-EEXEC_NOINIT	Component not (yet) initialized (any more)
 *			-EPAR_NULL		NULL pointer specified for _wu_obj
 *			-EINVAL		_wu_obj is invalid
 *			-ESTD_AGAIN		OSEK only: Alarm timer for this thread is already in use
 *			-ESTD_ACCES		OSEK only: Function is called from within a prohibited context
 * ****************************************************************************/
int lib_thread__wakeup_wait(wakeup_hdl_t *_wu_obj)
{
	int ret;


#ifndef _WIN32
	/* check whether component is properly initialized */
	if (s_wakeup_init_calls == 0){
		ret = EEXEC_NOINIT;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"");
		return ret;
	}
#endif

	/* check arguments */
	if (_wu_obj == NULL){
		ret = -EPAR_NULL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"_wu_obj");
		return ret;
	}
	if (*_wu_obj == NULL){
		ret = -EINVAL;
		msg (LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"*_wu_obj");
		return ret;
	}

#ifdef _WIN32
	/* sleep for registered amount of time */
	lib_thread__msleep((*_wu_obj)->interval);
#else	/* any fully POSIX-compliant OS */
	{
		int sig;

		/* wait for specified alarm signal to be raised */
		ret = sigwait(&((*_wu_obj)->sigset), &sig);
		if (ret != 0){
			/* Should actually never happen, except if a signal is received.
			 * In that case, the error will be reported but not actually handled,
			 * as not all high-level OSes are able to report that error.
			 */
			msg(LOG_LEVEL_error, M_DEV_LIB_THREAD_WHP_MODULE_ID,"::sigwait");
		}
	}
#endif	/* _WIN32 or any fully POSIX-compliant OS */


	return EOK;
}
