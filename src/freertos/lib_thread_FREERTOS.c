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
#include <stdint.h>
#include <errno.h>
#include <string.h>

/* system */
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

/* framework */
#include <lib_convention__errno.h>
#include <lib_convention__macro.h>
#include <lib_log.h>
#include <lib_list.h>

/* project */
#include "lib_thread.h" /* interface  */


/* *******************************************************************
 * defines
 * ******************************************************************/
#define M_LIB_THREAD__SIG_DESTROY		UINT32_MAX-1
#define M_LIB_THREAD__SEM_VALUE_MAX 	255
#define M_LIB_THREAD__MODULE_ID 		"LIB_THD"
#define M_LIB_THREAD__TIMEOUT_INFINITE  -1

/* *******************************************************************
 * custom data types (e.g. enumerations, structures, unions)
 * ******************************************************************/

enum mtx_mode {
	MTX_MODE_normal,
	MTX_MODE_recursive,
	MTX_MODE_CNT
};

/***********************************/
/* helper structures 			   */
/***********************************/
struct thunk_task_attr {
	SemaphoreHandle_t sem_finish;
	thread_worker_t *worker;
	unsigned int expired_thread_id;
	void *arg;
	void *retval;
};

struct signal_wait_node {
	struct list_node node;
	xTaskHandle rtos_thread_hdl;
	unsigned int rtos_thread_prio;
};

/***********************************/
/* OPAQUE INTERFACE STRUCTURES     */
/***********************************/

/* structure which defines the opaque thread handle*/
struct thread_hdl_attr {
	TaskHandle_t rtos_thread_hdl;
	struct thunk_task_attr *thunk_attr;
	const char *thread_name;
};

/* structure which defines the opaque mutex handle*/
struct mutex_hdl_attr {
	SemaphoreHandle_t rtos_mtx_hdl;
	enum mtx_mode mode;
};

/* structure which defines the opaque signal handle */
struct signal_hdl_attr {
	struct queue_attr  signal_waiter_list;
	//QueueHandle_t rtos_sgn_hdl;
	unsigned int destroy;
};

/* structure which defines the opaque semphore handle */
struct sem_hdl_attr {
	SemaphoreHandle_t rtos_sem_hdl;
};

struct condvar_hdl_attr {
	SemaphoreHandle_t cond_sem_hdl;
	SemaphoreHandle_t mtx_waiting_threads_hdl;
	int number_of_waiting_threads;
};


/* *******************************************************************
 * static function declarations
 * ******************************************************************/
static int lib_thread__mutex_mode_init (mutex_hdl_t *_hdl, enum mtx_mode _mode);
static void thunk_lib_thread__taskprocessing(void * _arg);
static int signal_waiter__sort_insert(struct queue_attr *_queue, struct signal_wait_node *_to_insert);
static struct signal_wait_node* signal_waiter__get_top(struct queue_attr *_queue);
static int signal_waiter__remove(struct queue_attr *_queue, struct signal_wait_node *_to_remove);

/* *******************************************************************
 * \brief	Initialization of the lib_thread
 * ---------
 * \remark  The call of the init routine is only be allowed at the main
 * 			routine of a procesM_LIB_THREAD__MODULE_IDs
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
	return 0;
}

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
int lib_thread__create (thread_hdl_t *_hdl, thread_worker_t *_worker, void *_arg, int _relative_priority, const char *_thread_name)
{
	unsigned long prio;
	int ret;
	struct thread_hdl_attr *hdl;
	struct thunk_task_attr *thunk_attr;
	const char *thread_name = "noName";

	if ((_hdl == NULL) || (_worker == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if(xTaskGetCurrentTaskHandle()==NULL) {
		prio = _relative_priority;
	}
	else {
		/* get the priority of the current task */
		prio =  uxTaskPriorityGet(NULL) +_relative_priority;
		if (prio > configMAX_PRIORITIES) {
			ret = -EPAR_RANGE;
			goto ERR_0;
		}
	}

	/* allocate memory in order to store handles and further parameters */
	hdl = (struct thread_hdl_attr*)pvPortMalloc(sizeof(struct thread_hdl_attr));
	thunk_attr = (struct thunk_task_attr*)pvPortMalloc(sizeof(struct thunk_task_attr));
	if((hdl == NULL) || (thunk_attr == NULL)){
		ret = -ESTD_NOMEM;
		goto ERR_1;
	}

	thunk_attr->sem_finish = xSemaphoreCreateCounting(255, 0);
	if(thunk_attr->sem_finish == NULL) {
		ret = -EAGAIN;
	}
	else {
		thunk_attr->arg = _arg;
		thunk_attr->worker = _worker;
		thunk_attr->expired_thread_id = 0;
		hdl->thunk_attr = thunk_attr;
	}

	if(_thread_name != NULL) {
		thread_name = _thread_name;
	}

	ret = (int)xTaskCreate(&thunk_lib_thread__taskprocessing, thread_name, configMINIMAL_STACK_SIZE, (void*)thunk_attr, prio, &hdl->rtos_thread_hdl);
	switch (ret) {
		case pdPASS 								: ret = EOK; break;
		case errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY	: ret = -ESTD_NOMEM; break;
		default										: ret = -ESTD_FAULT; break;
	}

	hdl->thread_name = _thread_name;
	*_hdl = hdl;
	return EOK;

	ERR_1:

	if (hdl != NULL) {
		vPortFree(hdl);
	}

	if(thunk_attr != NULL) {
		vPortFree(thunk_attr);
	}

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s(): failed with retval %i",__func__, ret );

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
int lib_thread__join (thread_hdl_t *_hdl, void **_ret_val)
{
	TaskHandle_t current_thd;
	int expired_thread_id = 0;
	int ret;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL){
		ret = -ESTD_SRCH;
		goto ERR_0;
	}

	current_thd = xTaskGetCurrentTaskHandle();
	/*check if current running context tries to join */
	if((*_hdl)->rtos_thread_hdl == current_thd) {
		ret = -EEXEC_DEADLK;
		goto ERR_0;
	}

	if ((*_hdl)->rtos_thread_hdl == NULL) {
		ret = -ESTD_FAULT;
		goto ERR_0;
	}

	ret = xSemaphoreTake((*_hdl)->thunk_attr->sem_finish, portMAX_DELAY);
	switch (ret) {
		case pdPASS : ret = EOK; break;
		case pdFALSE : ret = -ESTD_FAULT; break;
	}

	if (ret < EOK) {
		goto ERR_0;
	}

	if(_ret_val != NULL) {
		*_ret_val = (*_hdl)->thunk_attr->retval;
	}

	vSemaphoreDelete((*_hdl)->thunk_attr->sem_finish);
	expired_thread_id= (*_hdl)->thunk_attr->expired_thread_id;
	vPortFree((*_hdl)->thunk_attr);
	vPortFree(*_hdl);
	*_hdl = NULL;

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "join(): successful (Thread ID '%u')", expired_thread_id);

	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "join(): failed with errror code %i", ret);
	return ret;
}

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
int lib_thread__cancel(thread_hdl_t _hdl)
{
	/* *******************************************************************
	 * >>>>>	locals 			<<<<<<
	 * ******************************************************************/
	int ret;
	unsigned int canceled_thread_id;

	/* check argument */
	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* cancel thread */
	vTaskSuspendAll ();
	if(_hdl->thunk_attr->expired_thread_id != 0) {
		ret = -ESTD_BUSY;
		goto ERR_0;
	}

	_hdl->thunk_attr->expired_thread_id = (unsigned int)_hdl->rtos_thread_hdl;
	vTaskDelete(_hdl->rtos_thread_hdl);
	_hdl->rtos_thread_hdl = NULL;
	xTaskResumeAll ();

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "cancel(): successul (Thread ID '%u')",_hdl->thunk_attr->expired_thread_id);

	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "cancel(): failed with errror code %i", ret);
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
	int ret = EOK;
	int name_length;
	const char *name;

	if ((_hdl == NULL) || (_name == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	name = _hdl->thread_name;
	if(name == NULL) {
		ret = -ESTD_NOENT;
		goto ERR_0;
	}

	name_length = strlen(name) +1;
	if(name_length <= 1) {
		ret = -ESTD_NOENT;
		goto ERR_0;
	}

	if(name_length > _maxlen) {
		ret = -ESTD_RANGE;
		goto ERR_0;
	}

	strncpy(_name, name, (_maxlen-1));
	_name[_maxlen-1] = 0;  // Ensure a null termination of the string

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "getname(): successully");
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "getname(): failed with errror code %i", ret);
	return ret;
}

/* *******************************************************************
 * \brief	The calling task sleeps for a defined time
 * ---------
 * \remark
 * ---------
 * \param	_hdl			:	milliseconds to sleep
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__msleep (unsigned int _milliseconds)
{
	vTaskDelay( _milliseconds / portTICK_PERIOD_MS );
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
	return lib_thread__mutex_mode_init (_hdl, MTX_MODE_normal);
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
int lib_thread__mutex_recursive_init (mutex_hdl_t *_hdl)
{
	return lib_thread__mutex_mode_init (_hdl, MTX_MODE_recursive);
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
	int ret;
	unsigned int mtx_id;
	TaskHandle_t taskhandle;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	taskENTER_CRITICAL();

	mtx_id = (unsigned int)(*_hdl)->rtos_mtx_hdl;
	/* check, whether this mutex is still hold by another thread, quit if so */
	taskhandle = xSemaphoreGetMutexHolder((*_hdl)->rtos_mtx_hdl);
	if (taskhandle != NULL) {
		taskEXIT_CRITICAL();
		return -ESTD_BUSY;
	}

	vSemaphoreDelete((*_hdl)->rtos_mtx_hdl);
	(*_hdl)->rtos_mtx_hdl = NULL;
	vPortFree((*_hdl));
	*_hdl = NULL;
	taskEXIT_CRITICAL();

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "mutex_destroy(): successul (ID:'%u')",mtx_id);
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "mutex_destroy(): failed with errror code %i", ret);
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
	TaskHandle_t current_thd, mtx_owner_thd;
	BaseType_t isInterrupt;
	uint32_t lock;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_hdl->rtos_mtx_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if (isInterrupt) {
		return -EEXEC_INVCXT;;
	}

	taskENTER_CRITICAL();
	if (_hdl->mode == MTX_MODE_normal) {
		current_thd = xTaskGetCurrentTaskHandle();
		mtx_owner_thd = xSemaphoreGetMutexHolder(_hdl->rtos_mtx_hdl);
		if(current_thd==mtx_owner_thd) {
			ret = -EEXEC_DEADLK;
			goto ERR_1;
		}
	}
	/* try to obtain the mutex, in case it is not  */
	taskEXIT_CRITICAL();

	switch (_hdl->mode)
	{
		case MTX_MODE_normal:
			ret = (int)xSemaphoreTake(_hdl->rtos_mtx_hdl, portMAX_DELAY);
			break;
		case MTX_MODE_recursive:
			ret = (int)xSemaphoreTakeRecursive(_hdl->rtos_mtx_hdl, portMAX_DELAY);
			break;
		default:
			ret = -ESTD_FAULT;
			goto ERR_0;
	}

	switch (ret) {
		case pdPASS : ret = EOK; break;
		case pdFALSE : ret = -ESTD_FAULT; break;
	}

	if (ret < EOK) {
		goto ERR_0;
	}

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "mutex_lock(): successul (ID:'%u')", _hdl->rtos_mtx_hdl);

	return EOK;

	ERR_1:
	taskEXIT_CRITICAL();

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "mutex_lock(): failed with errror code %i", ret);
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
	TaskHandle_t mtx_owner_thd, current_thd;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_hdl->rtos_mtx_hdl == NULL) {
		ret = -ESTD_PERM;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if (isInterrupt) {
		return -EEXEC_INVCXT;;
	}

	current_thd = xTaskGetCurrentTaskHandle();
	mtx_owner_thd = xSemaphoreGetMutexHolder(_hdl->rtos_mtx_hdl);

	if(current_thd != mtx_owner_thd) {
		ret = -ESTD_PERM;
		goto ERR_0;
	}

	switch (_hdl->mode)
	{
		case MTX_MODE_normal:
			ret = (int)xSemaphoreGive(_hdl->rtos_mtx_hdl);
			break;
		case MTX_MODE_recursive:
			ret = (int)xSemaphoreGiveRecursive(_hdl->rtos_mtx_hdl);
			break;
		default:
			ret = -ESTD_FAULT;
			goto ERR_0;
	}

	switch (ret) {
		case pdPASS : ret = EOK; break;
		case pdFALSE : ret = -ESTD_PERM; break;
	}
	if (ret < EOK) {
		goto ERR_0;
	}

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "mutex_unlock(): successul (ID:'%u')", _hdl->rtos_mtx_hdl);
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "mutex_unlock(): failed with errror code %i", ret);
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
 * 			+ '-ESTD_BUSY', in case it is already locked
 * ******************************************************************/
int lib_thread__mutex_trylock (mutex_hdl_t _hdl)
{
	int ret;
	TaskHandle_t mtx_owner_thd;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		return -EPAR_NULL;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if (isInterrupt) {
		return -EEXEC_INVCXT;
	}

	taskENTER_CRITICAL();
	mtx_owner_thd = xSemaphoreGetMutexHolder(_hdl->rtos_mtx_hdl);
	if(mtx_owner_thd != NULL) {
		taskEXIT_CRITICAL();
		return -ESTD_BUSY;
	}

	taskEXIT_CRITICAL();
	switch (_hdl->mode)
	{
		case MTX_MODE_normal:
			ret = (int)xSemaphoreTake(_hdl->rtos_mtx_hdl, portMAX_DELAY);
			break;
		case MTX_MODE_recursive:
			ret = (int)xSemaphoreTakeRecursive(_hdl->rtos_mtx_hdl, portMAX_DELAY);
			break;
		default:
			ret = -ESTD_FAULT;
			goto ERR_0;
	}

	switch (ret) {
		case pdPASS : ret = EOK; break;
		case pdFALSE : ret = -ESTD_PERM; break;
	}
	if (ret < EOK) {
		goto ERR_0;
	}

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "mutex_trylock(): successul (ID:'%u')", _hdl->rtos_mtx_hdl);
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "mutex_trylock(): failed with error code %i", ret);
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
	int ret;
	signal_hdl_t sgn_hdl;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	sgn_hdl = (signal_hdl_t)pvPortMalloc(sizeof(struct signal_hdl_attr));
	if (sgn_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	ret = lib_list__init(&sgn_hdl->signal_waiter_list, NULL);
	if (ret < EOK) {
		goto ERR_0;
	}

	sgn_hdl->destroy = 0;
	*_hdl = sgn_hdl;

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "signal_init(): successfully");
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "signal_init(): failed with error code %i", ret);
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
	struct signal_wait_node *wakeup;
	int ret= EOK;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL){
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	(*_hdl)->destroy = 1;
	do {
		wakeup = signal_waiter__get_top(&(*_hdl)->signal_waiter_list);
		if (wakeup != NULL) {
			ret = xTaskNotify(wakeup->rtos_thread_hdl, M_LIB_THREAD__SIG_DESTROY, eSetValueWithOverwrite );
			if (ret == pdPASS) {
				ret = signal_waiter__remove(&(*_hdl)->signal_waiter_list, wakeup);
				vPortFree(wakeup);
			}
		}
	}while(wakeup != NULL);

	ret = lib_list__emty(&(*_hdl)->signal_waiter_list, 0, NULL);
	if (ret == 0) {
		ret = -ESTD_FAULT;
		goto ERR_1;
	}

	vPortFree(*_hdl);
	(*_hdl) = NULL; /* destroy the address hold by the handle */
	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "signal_destroy(): successfully");
	return EOK;

	ERR_1:
	(*_hdl)->destroy = 0;

	ERR_0:
	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "signal_destroy(): failed with errror code %i", ret);
	return ret;
}

/* *******************************************************************
 * \brief	Emit a signal
 * ---------
 * \remark	If a signal is emitted without any waiting thread the signal
 * 			gets lost
 *
 * Now, a signal can be designed as follows:
 * - when a thread waits for a signal, it creates a queue. In case another
 * thread emits this signal, it posts into this particular queue.
 *
 * Hence, we need a queue, a list sorting component. Enqueuing starts the
 * scheduler, which looks into the list of waiting threads. And executes
 * each of these threads. Dispatching the thread will remove it from the
 * list of waiting threads.
 *
 * We should check, whether the signal is send from within an ISR context!
 *
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
	struct signal_wait_node *wakeup;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_hdl->destroy) {
		return -ESTD_PERM;
	}

	wakeup = signal_waiter__get_top(&_hdl->signal_waiter_list);
	if (wakeup == NULL) {
		return EOK;
	}

	ret = xTaskNotify(wakeup->rtos_thread_hdl, 0xfff, eSetValueWithoutOverwrite );
	if (ret != pdPASS) {
		ret = -ESTD_BUSY;
		goto ERR_0;
	}

	ret = signal_waiter__remove(&_hdl->signal_waiter_list, wakeup);
	vPortFree(wakeup);
	if (ret != EOK) {
		goto ERR_0;
	}

	return ret;

	ERR_0:
	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "signal_send(): failed with errror code %i", ret);
	return ret;

}

/* *******************************************************************
 * \brief	Waiting for an signal
 * ---------
 * \remark	The calling thread blocks unit the corresponding signal get be triggered.
 *
 * ---------
 *xGetCurrentTaskHandle
 * \param	_hdl			[in] : 	handle to a signal object to wait
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__signal_wait (signal_hdl_t _hdl)
{
	int ret_val, ret;
	uint32_t notify_value;
	struct signal_wait_node *sgn_wait_node;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if(isInterrupt) {
		return -EEXEC_INVCXT;
	}

//	if (_hdl->destroy) {
//		return -ESTD_PERM;
//	}

	sgn_wait_node = (struct signal_wait_node *)pvPortMalloc(sizeof(struct signal_wait_node));
	if (sgn_wait_node == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	sgn_wait_node->rtos_thread_prio = uxTaskPriorityGet(NULL);
	sgn_wait_node->rtos_thread_hdl = xTaskGetCurrentTaskHandle();


	ret = signal_waiter__sort_insert(&_hdl->signal_waiter_list,sgn_wait_node);
	if (ret < EOK) {
		goto ERR_1;
	}

	/* Wait to be notified of an interrupt. */
	ret = xTaskNotifyWait( pdFALSE, ULONG_MAX, &notify_value, portMAX_DELAY );
	if (ret != pdPASS) {
		ret = -EHAL_ERROR;
		goto ERR_0;
	}

	if (notify_value == M_LIB_THREAD__SIG_DESTROY) {
		return -ESTD_PERM;
	}
	else {
		return EOK;
	}

	ERR_1:
	vPortFree(sgn_wait_node);

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s() : failed with retval %i", __func__, ret );
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
	int ret_val, ret;
	uint32_t notify_value;
	struct signal_wait_node *sgn_wait_node;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if(isInterrupt) {
		return -EEXEC_INVCXT;
	}

//	if (_hdl->destroy) {
//		current_task->ulNotifiedValue = 0;
//		return -ESTD_PERM;
//	}

	sgn_wait_node = (struct signal_wait_node *)pvPortMalloc(sizeof(struct signal_wait_node));
	if (sgn_wait_node == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	sgn_wait_node->rtos_thread_prio = uxTaskPriorityGet(NULL);
	sgn_wait_node->rtos_thread_hdl = xTaskGetCurrentTaskHandle();


	ret = signal_waiter__sort_insert(&_hdl->signal_waiter_list,sgn_wait_node);
	if (ret < EOK) {
		goto ERR_1;
	}

	/* Wait to be notified of an interrupt. */
	ret = xTaskNotifyWait( pdFALSE, ULONG_MAX, &notify_value, pdMS_TO_TICKS(_milliseconds));
	switch(ret)
	{
		case pdFAIL :
			signal_waiter__remove(&_hdl->signal_waiter_list, sgn_wait_node);
			ret = -EEXEC_TO;
			break;
		case pdPASS:
			if (notify_value == M_LIB_THREAD__SIG_DESTROY) {
				ret = -ESTD_PERM;
			}
			else {
				ret = EOK;
			}
			break;

		default:
			signal_waiter__remove(&_hdl->signal_waiter_list, sgn_wait_node);
			ret = -EHAL_ERROR;
			goto ERR_0;
	}
	return ret;

	ERR_1:
	vPortFree(sgn_wait_node);

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s() : failed with retval %i", __func__, ret );
	return ret;}

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
int lib_thread__sem_init (sem_hdl_t *_hdl, int _count)
{
	int ret;
	sem_hdl_t sem_hdl;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_count > M_LIB_THREAD__SEM_VALUE_MAX) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	sem_hdl = (sem_hdl_t)pvPortMalloc(sizeof(struct sem_hdl_attr));
	if (sem_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	sem_hdl->rtos_sem_hdl = xSemaphoreCreateCounting(M_LIB_THREAD__SEM_VALUE_MAX, _count);
	if (sem_hdl->rtos_sem_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_1;
	}

	*_hdl = sem_hdl;
	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "sem_init(): successul");
	return EOK;

	ERR_1:
	vPortFree(*_hdl);
	ERR_0:
	if (_hdl != NULL) {
		*_hdl = NULL;
	}
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "sem_init(): failed with errror code %i", ret);
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
	int ret = 0;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL){
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	vSemaphoreDelete((*_hdl)->rtos_sem_hdl);
	vPortFree(*_hdl);
	(*_hdl) = NULL;

	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "sem_destroy(): successul");
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "sem_destroy(): failed with errror code %i", ret);
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
int lib_thread__sem_timedwait (sem_hdl_t _hdl, int _milliseconds)
{
	int ret, ret_val;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if(isInterrupt) {
		return -EEXEC_INVCXT;
	}

	ret_val = xSemaphoreTake(_hdl->rtos_sem_hdl, pdMS_TO_TICKS(_milliseconds)); //_milliseconds / portTICK_PERIOD_MS
	switch(ret_val) {
		case pdPASS : ret = EOK; break;
		case pdFAIL : ret = -EEXEC_TO; break;
		default : ret = -ESTD_FAULT; break;
	}

	if (ret < EOK) {
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug, M_LIB_THREAD__MODULE_ID, "%s():  successfully",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s() : failed with retval %i", __func__, ret );
	return ret;

}

int lib_thread__sem_post (sem_hdl_t _hdl)
{
	int ret;
	BaseType_t xHigherPriorityTaskWoken;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if(isInterrupt) {
		ret = xSemaphoreGiveFromISR( _hdl->rtos_sem_hdl, &xHigherPriorityTaskWoken );
	}
	else {
		ret = xSemaphoreGive(_hdl->rtos_sem_hdl);
	}

	if (ret != pdPASS ) {
		ret = -ESTD_FAULT;
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug, M_LIB_THREAD__MODULE_ID, "%s():  successfully",__func__);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s(): failed with retval %i",__func__, ret );
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
	int ret, ret_val;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if(isInterrupt) {
		return -EEXEC_INVCXT;
	}

	ret_val = xSemaphoreTake(_hdl->rtos_sem_hdl, portMAX_DELAY);
	switch(ret_val){
		case pdPASS : ret = EOK; break;
		case errQUEUE_EMPTY: ret = -ESTD_FAULT; break;
		default : ret = -ESTD_FAULT; break;
	}

	if (ret < EOK) {
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug, M_LIB_THREAD__MODULE_ID, "%s():  successfully",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s() : failed with retval %i", __func__, ret );
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
	int ret, ret_val;
	BaseType_t isInterrupt;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if (isInterrupt) {
		return -EEXEC_INVCXT;;
	}

	ret_val = xSemaphoreTake(_hdl->rtos_sem_hdl, 0);
	switch(ret_val){
		case pdPASS : ret = EOK; break;
		case errQUEUE_EMPTY: ret = -ESTD_AGAIN; break;
		default : ret = -ESTD_AGAIN; break;
	}

	if (ret < EOK) {
		goto ERR_0;
	}

	msg (LOG_LEVEL_debug, M_LIB_THREAD__MODULE_ID, "%s(): successfully",__func__);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s(): failed with retval %i",__func__, ret );
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
 *			-ESTD_NOMEM		Insufficient memory available to initialize the condvar
 *			-ESTD_AGAIN		Insufficient system resources available to initialize the condvar
 * ****************************************************************************/
int lib_thread__cond_init(cond_hdl_t *_hdl)
{
	int ret;
	struct condvar_hdl_attr *cond_hdl;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	cond_hdl = (struct condvar_hdl_attr*)pvPortMalloc(sizeof(struct condvar_hdl_attr));
	if (cond_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	cond_hdl->mtx_waiting_threads_hdl = xSemaphoreCreateMutex();
	if (cond_hdl->mtx_waiting_threads_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_1;
	}

	cond_hdl->cond_sem_hdl = xSemaphoreCreateCounting(M_LIB_THREAD__SEM_VALUE_MAX, 0);
	if (cond_hdl->cond_sem_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_2;
	}
	cond_hdl->number_of_waiting_threads = 0;

	*_hdl = cond_hdl;
	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "cond_init(): successul (ID:'%u')", 0);
	return EOK;

	ERR_2:
	vSemaphoreDelete(cond_hdl->mtx_waiting_threads_hdl);

	ERR_1:
	vPortFree(cond_hdl);

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "mutex_init(): failed with errror code %i", ret);
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

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (*_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	if ((*_hdl)->number_of_waiting_threads> 0) {
		ret = -ESTD_BUSY;
		goto ERR_0;
	}

	vSemaphoreDelete((*_hdl)->mtx_waiting_threads_hdl);
	vSemaphoreDelete((*_hdl)->cond_sem_hdl);
	vPortFree((*_hdl));
	*_hdl = NULL;
	return EOK;

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "sem_destroy(): failed with errror code %i", ret);
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

	/* check arguments for validity */
	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	/* MUTEX LOCK */
	xSemaphoreTake(_hdl->mtx_waiting_threads_hdl, portMAX_DELAY );
	if(_hdl->number_of_waiting_threads > 0) {
		xSemaphoreGive(_hdl->cond_sem_hdl);
	}
	/* MUTEX UNLOCK */
	xSemaphoreGive(_hdl->mtx_waiting_threads_hdl);

	return EOK;


	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "cond_signal(): failed with errror code %i", ret);
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

	/* check arguments for validity */
	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	while (_hdl->number_of_waiting_threads > 0) {

		//MTX lock
		ret = xSemaphoreTake(_hdl->mtx_waiting_threads_hdl, portMAX_DELAY );
		if (ret != pdPASS) {
			ret = -ESTD_FAULT;
			goto ERR_0;
		}

		if(_hdl->number_of_waiting_threads > 0) {
			ret = xSemaphoreGive(_hdl->cond_sem_hdl);
			if (ret != pdPASS) {
				ret = -ESTD_FAULT;
				goto ERR_1;
			}
			_hdl->number_of_waiting_threads--;
		}
		//MTX unlock
		xSemaphoreGive(_hdl->mtx_waiting_threads_hdl);
	}

	return EOK;

	ERR_1:
	xSemaphoreGive(_hdl->mtx_waiting_threads_hdl);

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "cond_signal(): failed with errror code %i", ret);
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
	return lib_thread__cond_timedwait(_hdl, _mtx, LIB_THREAD__TIMEOUT_INFINITE);
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
	int ret, ret2;
	TickType_t timeout;
	BaseType_t isInterrupt;

	/* check arguments for validity */
	if ((_hdl == NULL) || (_mtx == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	isInterrupt = xPortIsInsideInterrupt();
	if (isInterrupt) {
		return -EEXEC_INVCXT;;
	}
	//TODO check initialized

	/* evaluate specified timeout */
	switch (_tmoms){
		case 0:
			/* zero timeout specified -> return immediate timeout error */
			return -EEXEC_TO;
		case LIB_THREAD__TIMEOUT_INFINITE:
			/* infinite timeout specified -> just continue */
			timeout = portMAX_DELAY;
			break;
		default:
			timeout = pdMS_TO_TICKS(_tmoms);
			break;
	}

	ret = xSemaphoreTake(_hdl->mtx_waiting_threads_hdl, portMAX_DELAY);
	if (ret != pdPASS) {
		goto ERR_0;
	}
	_hdl->number_of_waiting_threads++;
	xSemaphoreGive(_hdl->mtx_waiting_threads_hdl);

	ret = lib_thread__mutex_unlock(_mtx);
	if (ret < EOK) {
		xSemaphoreTake(_hdl->mtx_waiting_threads_hdl, portMAX_DELAY);
		_hdl->number_of_waiting_threads--;
		xSemaphoreGive(_hdl->mtx_waiting_threads_hdl);
		goto ERR_0;
	}

	ret = xSemaphoreTake(_hdl->cond_sem_hdl, timeout);
	switch(ret) {
		case pdPASS : ret = EOK; break;
		case pdFAIL : ret = -EEXEC_TO; break;
		default : ret = -ESTD_FAULT;
	}

	/*Lock of the entangled condition mutex again */
	lib_thread__mutex_lock(_mtx);

	ret2 = xSemaphoreTake(_hdl->mtx_waiting_threads_hdl, portMAX_DELAY);
	_hdl->number_of_waiting_threads--;
	xSemaphoreGive(_hdl->mtx_waiting_threads_hdl);
	if(ret2 !=pdPASS) {
		ret = -ESTD_FAULT;
		goto ERR_0;
	}

	if ((ret == EOK) || (ret == -EEXEC_TO)) {
		return ret;
	}

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "cond_timedwait(): failed with errror code %i", ret);
	return ret;
}
/* *******************************************************************
 * static function definitions
 * ******************************************************************/

/* *******************************************************************
 * \brief	Initialization of a mutex object
 * ---------
 * \remark
 * ---------
 *
 * \param	_hdl			[in/out] :	pointer to the handle of a mutex object
 * 										to initialize
 * \param   _mode					 :  selects if normal or recursive mutex is used
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
static int lib_thread__mutex_mode_init (mutex_hdl_t *_hdl, enum mtx_mode _mode)
{
	int ret;
	struct mutex_hdl_attr *mtx_hdl;

	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_mode >= MTX_MODE_CNT) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	mtx_hdl = (struct mutex_hdl_attr*)pvPortMalloc(sizeof(struct mutex_hdl_attr));
	if (mtx_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	/* alloc memory in order to create a mutex */
	mtx_hdl->rtos_mtx_hdl = xSemaphoreCreateMutex();
	if (mtx_hdl->rtos_mtx_hdl == NULL) {
		/* cleanup memory */
		ret = -ESTD_NOMEM;
		goto ERR_1;
	}

	mtx_hdl->mode = _mode;
	*_hdl = mtx_hdl;
	msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "mutex_init(): successul (ID:'%u')", mtx_hdl->rtos_mtx_hdl);
	return EOK;

	ERR_1:
	vPortFree(mtx_hdl);

	ERR_0:
	msg(LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "mutex_init(): failed with errror code %i", ret);
	return ret;
}


/* *******************************************************************
 * \brief	Thunk for FREERTOS thread processing
 * ---------
 * \remark
 * ---------
 * \parm  _arg [IN / OUT]	:	pass of the thread attributes
 * ---------
 * \return	void
 * ******************************************************************/
static void thunk_lib_thread__taskprocessing(void * _arg)
{
	struct thunk_task_attr * arg = (struct thunk_task_attr*)_arg;
	void *retval;

	retval = (*arg->worker)(arg->arg);
	//vTaskSuspendAll ();
	taskENTER_CRITICAL();
	arg->retval = retval;
	arg->expired_thread_id = (unsigned int)xTaskGetCurrentTaskHandle();
	xSemaphoreGive(arg->sem_finish);
	//xTaskResumeAll ();
	vTaskDelete(NULL);
	taskEXIT_CRITICAL();
}

static int signal_waiter__sort_insert(struct queue_attr *_queue, struct signal_wait_node *_to_insert)
{
	struct list_node *start, *end, *itr_node;
	struct signal_wait_node *itr_data;
	int ret;

	if ((_queue == NULL) || (_to_insert == NULL)) {
		return -EPAR_NULL;
	}

	start = ITR_BEGIN(_queue,0,NULL);
	end = ITR_END(_queue,0,NULL);

	/* First entry*/
	if (start == NULL) {
		ret = lib_list__add_after(_queue,&_queue->head,&_to_insert->node,0,NULL);
		return ret;
	}

	/* Second entry*/
	if (start == end) {
		itr_data = (struct signal_wait_node*)GET_CONTAINER_OF(start,struct signal_wait_node,node);
		if (itr_data->rtos_thread_prio < _to_insert->rtos_thread_prio) {
			ret = lib_list__add_before(_queue,&itr_data->node,&_to_insert->node,0,NULL);
		}
		else {
			ret = lib_list__add_after(_queue,&itr_data->node,&_to_insert->node,0,NULL);
		}
		return ret;

	}

	for (itr_node = start; itr_node != end; ITR_NEXT(_queue, &itr_node, 0, NULL))
	{
		itr_data = (struct signal_wait_node*)GET_CONTAINER_OF(itr_node,struct signal_wait_node,node);
		if (itr_data->rtos_thread_prio < _to_insert->rtos_thread_prio) {
			ret = lib_list__add_before(_queue,&itr_data->node,&_to_insert->node,0,NULL);
			return ret;
		}
	}

	/*last entry*/
	itr_data = (struct signal_wait_node*)GET_CONTAINER_OF(itr_node,struct signal_wait_node,node);
	if (itr_data->rtos_thread_prio < _to_insert->rtos_thread_prio) {
		ret = lib_list__add_before(_queue,&itr_data->node,&_to_insert->node,0,NULL);
	}
	else {
		ret = lib_list__add_after(_queue,&itr_data->node,&_to_insert->node,0,NULL);
	}
	return ret;
}

static struct signal_wait_node* signal_waiter__get_top(struct queue_attr *_queue)
{
	int ret;
	struct list_node *itr_node;
	static struct signal_wait_node *itr_node_data;

	ret = lib_list__get_begin(_queue ,&itr_node, 0, NULL);
	if(ret < EOK) {
		return NULL;
	}

	itr_node_data = (struct signal_wait_node*)GET_CONTAINER_OF(itr_node,struct signal_wait_node,node);
	return itr_node_data;
}

static int signal_waiter__remove(struct queue_attr *_queue, struct signal_wait_node *_to_remove)
{
	int ret;

	if (_to_remove == NULL) {
		return -EPAR_NULL;
	}

	ret = lib_list__delete(_queue, &_to_remove->node, 0, NULL);
	return ret;
}


