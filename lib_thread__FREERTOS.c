/* ****************************************************************************************************
 * lib_thread__FREERTOS.c within the following project: lib_thread_FREERTOS
 *	
 *  compiler:   GNU Tools ARM Embedded (4.7.201xqx)
 *  target:     Cortex Mx
 *  author:		schmied
 * ****************************************************************************************************/

/* ****************************************************************************************************/

/*
 *	******************************* change log *******************************
 *  date			user			comment
 * 	16 Apr 2016		Thomas			- creation of lib_thread__FREERTOS.c
 * 	12 Feb 2018		Thoma			- Adjust to new errno style
 *  
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
#include <lib_log.h>

/* project */
//#include <stm32f1xx.h>
#include "lib_thread.h" /* interface  */


/* *******************************************************************
 * defines
 * ******************************************************************/
#define SEM_VALUE_MAX 				255
#define M_LIB_THREAD__MODULE_ID 	"LIB_THD"

/* *******************************************************************
 * custom data types (e.g. enumerations, structures, unions)
 * ******************************************************************/

enum sgn_dequeue_attr {
	DEQUEUE_ATTR_rcv,
	DEQUEUE_ATTR_destroy
};


//extern void* pxCurrentTCB;
//static inline unsigned int isInterrupt()
//{
//    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0 ;
//}
/* *******************************************************************
 * static function declarations
 * ******************************************************************/

struct thread_hdl_attr {
	TaskHandle_t rtos_thd_handle;
	struct thunk_task_attr *thunk_attr;
};

struct thunk_task_attr {
	SemaphoreHandle_t sem_finish;
	thread_worker_t *worker;
	unsigned int expired_thread_id;
	void *arg;
	void *retval;
};

/* *******************************************************************
 * \brief	structure which defines the opaque mutex handle
 * ---------
 * \remark	in the world of FREERTOS a mutex corresponds to a binary
 * 			semaphore which comprises of an priorirty inheritance
 * 			mechanism
 * ******************************************************************/
struct mutex_hdl_attr {
	SemaphoreHandle_t rtos_mtx_hdl;
};

/* *******************************************************************
 * \brief	structure which defines the opaque signal handle
 * ---------
 * ******************************************************************/
struct signal_hdl_attr {
	QueueHandle_t rtos_sgn_hdl;
	unsigned int destroy;
};

/* *******************************************************************
 * \brief	structure which defines the opaque semphore handle
 * ---------
 * ******************************************************************/
struct sem_hdl_attr {
	SemaphoreHandle_t rtos_sem_hdl;
};

/* *******************************************************************
 * (static) variables declarations
 * ******************************************************************/

static void thunk_lib_thread__taskprocessing(void * _arg);


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
	/* TODO @ BS: remove just return  */
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
	/* *******************************************************************
	 * >>>>>	locals 												<<<<<<
	 * ******************************************************************/
	unsigned long prio;
	int ret;
	struct thread_hdl_attr *hdl;
	struct thunk_task_attr *thunk_attr;

	/* *******************************************************************
	 * >>>>>	start of code section								<<<<<<
	 * ******************************************************************/
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
	hdl = pvPortMalloc(sizeof(struct thread_hdl_attr));
	thunk_attr = pvPortMalloc(sizeof(struct thunk_task_attr));
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

	ret = (int)xTaskCreate(&thunk_lib_thread__taskprocessing, _thread_name, configMINIMAL_STACK_SIZE, (void*)thunk_attr, prio, &hdl->rtos_thd_handle);
	switch (ret) {
		case pdPASS 								: ret = EOK; break;
		case errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY	: ret = -ESTD_NOMEM; break;
		default										: ret = -ESTD_FAULT; break;
	}

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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	TaskHandle_t current_thd;
	int expired_thread_id = 0;
	int ret;

	/* *******************************************************************
	 * >>>>>	start of code section			<<<<<<
	 * ******************************************************************/
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
	if((*_hdl)->rtos_thd_handle == current_thd) {
		ret = -EEXEC_DEADLK;
		goto ERR_0;
	}

	if ((*_hdl)->rtos_thd_handle == NULL) {
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

	_hdl->thunk_attr->expired_thread_id = (unsigned int)_hdl->rtos_thd_handle;
	vTaskDelete(_hdl->rtos_thd_handle);
	_hdl->rtos_thd_handle = NULL;
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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret = EOK;
	int name_length;
	char *name;
	/* *******************************************************************
	 * >>>>>	start of code section			<<<<<<
	 * ******************************************************************/
	if ((_hdl == NULL) || (_name == NULL)) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	name = pcTaskGetTaskName(_hdl->rtos_thd_handle);
	if(name == NULL) {
		ret = -ESTD_SRCH;
		goto ERR_0;
	}

	name_length = strlen(name) +1;
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
	/* *******************************************************************
	 * >>>>>	locals	<<<<<<
	 * ******************************************************************/
	int ret;
	struct mutex_hdl_attr *mtx_hdl;

	/* *******************************************************************
	 * >>>>>	start of code section / check functions' arguments 	<<<<<<
	 * ******************************************************************/
	if (_hdl == NULL) {
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	mtx_hdl = pvPortMalloc(sizeof(struct mutex_hdl_attr));
	if (mtx_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	/* alloc memory in order to create a mutex */
	mtx_hdl->rtos_mtx_hdl = xSemaphoreCreateMutex();
	if (mtx_hdl->rtos_mtx_hdl == NULL) {
		/* cleanup memory */
		ret = -ESTD_FAULT;
		goto ERR_1;
	}

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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret;
	unsigned int mtx_id;
	TaskHandle_t taskhandle;

	/* *******************************************************************
	 * >>>>>	start of code section / check functions' arguments	<<<<<<
	 * ******************************************************************/
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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret;
	TaskHandle_t current_thd, mtx_owner_thd;

	/* *******************************************************************
	 * >>>>>	start of code section								<<<<<<
	 * ******************************************************************/
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_hdl->rtos_mtx_hdl == NULL) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	taskENTER_CRITICAL();
	current_thd = xTaskGetCurrentTaskHandle();
	mtx_owner_thd = xSemaphoreGetMutexHolder(_hdl->rtos_mtx_hdl);
	if(current_thd==mtx_owner_thd) {
		ret = -EEXEC_DEADLK;
		goto ERR_1;
	}

	/* try to obtain the mutex, in case it is not  */
	taskEXIT_CRITICAL();
	ret = (int)xSemaphoreTake(_hdl->rtos_mtx_hdl, portMAX_DELAY);
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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret;
	TaskHandle_t mtx_owner_thd, current_thd;

	/* *******************************************************************
	 * >>>>>	start of code section			<<<<<<
	 * ******************************************************************/
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_hdl->rtos_mtx_hdl == NULL) {
		ret = -ESTD_PERM;
		goto ERR_0;
	}

	current_thd = xTaskGetCurrentTaskHandle();
	mtx_owner_thd = xSemaphoreGetMutexHolder(_hdl->rtos_mtx_hdl);

	if(current_thd != mtx_owner_thd) {
		ret = -ESTD_PERM;
		goto ERR_0;
	}

	ret = (int)xSemaphoreGive(_hdl->rtos_mtx_hdl);
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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret;
	TaskHandle_t mtx_owner_thd;
	/* *******************************************************************
	 * >>>>>	start of code section			<<<<<<
	 * ******************************************************************/
	if (_hdl == NULL){
		return -EPAR_NULL;
	}

	taskENTER_CRITICAL();
	mtx_owner_thd = xSemaphoreGetMutexHolder(_hdl->rtos_mtx_hdl);
	if(mtx_owner_thd != NULL) {
		taskEXIT_CRITICAL();
		return -ESTD_BUSY;
	}

	ret = (int)xSemaphoreTake(_hdl->rtos_mtx_hdl, portMAX_DELAY);
	switch (ret) {
		case pdPASS : ret = EOK; break;
		case pdFALSE : ret = -ESTD_PERM; break;
	}
	if (ret < EOK) {
		goto ERR_0;
	}

	taskEXIT_CRITICAL();

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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret;
	signal_hdl_t sgn_hdl;

	/* *******************************************************************
	 * >>>>>	start of code section								<<<<<<
	 * ******************************************************************/
	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}


	sgn_hdl = pvPortMalloc(sizeof(struct signal_hdl_attr));
	if (sgn_hdl == NULL) {
		ret = -ESTD_NOMEM;
	}

	sgn_hdl->rtos_sgn_hdl = xQueueCreate(5, sizeof(uint32_t));
	if(sgn_hdl->rtos_sgn_hdl == NULL) {
		ret = -ESTD_NOMEM;
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
	int ret= EOK;
	int ret_val;
	enum sgn_dequeue_attr dequeue_attr;

	if (_hdl == NULL){
		return -EPAR_NULL;
	}
	if (*_hdl == NULL){
		return -ESTD_INVAL;
	}

	dequeue_attr = DEQUEUE_ATTR_destroy;
	ret_val= xQueueSend( (*_hdl)->rtos_sgn_hdl , &dequeue_attr, 0);
	if (ret_val == pdPASS ) {
		ret = EOK;
	}
	else {
		ret = -ESTD_FAULT;
	}

	if(ret == EOK) {
		vQueueDelete((*_hdl)->rtos_sgn_hdl);
		/* cleanup memory */
		vPortFree(*_hdl);
		(*_hdl) = NULL; /* destroy the address hold by the handle */
	}

	if (ret == EOK) {
		msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "signal_destroy(): successul");
	} else {
		msg(LOG_LEVEL_info, M_LIB_THREAD__MODULE_ID, "signal_destroy(): failed with errror code %i", ret);
	}
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
	/* *******************************************************************
	 * >>>>>	locals 	<<<<<<
	 * ******************************************************************/
	int ret;
	enum sgn_dequeue_attr dequeue_attr;
	//uint32_t rcv_status =2;
	/* *******************************************************************
	 * >>>>>	start of code section			<<<<<<
	 * ******************************************************************/
	if (_hdl == NULL){
		return -EPAR_NULL;
	}

	if (_hdl->destroy) {
		return -ESTD_PERM;
	}

	/* the signal has already been created, hence just give it! */
	dequeue_attr = DEQUEUE_ATTR_rcv;
	ret = xQueueSend( _hdl->rtos_sgn_hdl , &dequeue_attr, 0);
	if (ret != pdPASS ) {
		ret = -ESTD_FAULT;
	}
	else {
		ret = EOK;
	}

	return EOK;
}

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
int lib_thread__signal_wait (signal_hdl_t _hdl)
{
	int ret_val, ret;
	enum sgn_dequeue_attr dequeue_attr;

	if (_hdl == NULL){
		return -EPAR_NULL;
	}

	if (_hdl->destroy) {
		return -ESTD_PERM;
	}

	ret_val = xQueueReceive( _hdl->rtos_sgn_hdl, &dequeue_attr, portMAX_DELAY);
	switch(ret_val){
		case pdPASS : ret = EOK; break;
		case errQUEUE_EMPTY: ret = -EEXEC_TO; break;
		default : ret = -ESTD_FAULT; break;

	}

	if ((_hdl->destroy)||(dequeue_attr == DEQUEUE_ATTR_destroy)) {
		return -ESTD_PERM;
	}

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
	enum sgn_dequeue_attr dequeue_attr;

	if (_hdl == NULL){
		return -EPAR_NULL;
	}

	if (_hdl->destroy) {
		return -ESTD_PERM;
	}

	ret_val = xQueueReceive( _hdl->rtos_sgn_hdl, &dequeue_attr, ( portTickType )_milliseconds / portTICK_PERIOD_MS);
	switch(ret_val){
		case pdPASS : ret = EOK; break;
		case errQUEUE_EMPTY: ret = -EEXEC_TO; break;
		default : ret = -ESTD_FAULT; break;

	}

	if ((_hdl->destroy)||(dequeue_attr == DEQUEUE_ATTR_destroy)) {
		return -ESTD_PERM;
	}

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
 *
 * ---------
 * \return	'0', if successful, < '0' if not successful
 * ******************************************************************/
int lib_thread__sem_init (sem_hdl_t *_hdl, int _count)
{
	int ret;
	sem_hdl_t sem_hdl;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if (_count > SEM_VALUE_MAX) {
		ret = -ESTD_INVAL;
		goto ERR_0;
	}

	sem_hdl = pvPortMalloc(sizeof(struct sem_hdl_attr));
	if (sem_hdl == NULL) {
		ret = -ESTD_NOMEM;
		goto ERR_0;
	}

	sem_hdl->rtos_sem_hdl = xSemaphoreCreateCounting(SEM_VALUE_MAX, _count);
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
int lib_thread__sem_post (sem_hdl_t _hdl)
{
	int ret;
	BaseType_t xHigherPriorityTaskWoken;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	if(portNVIC_INT_CTRL_REG & portVECTACTIVE_MASK) {
		 ret = xSemaphoreGiveFromISR( _hdl->rtos_sem_hdl, &xHigherPriorityTaskWoken );
	}
	else {
		ret = xSemaphoreGive(_hdl->rtos_sem_hdl);
	}

	if (ret != pdPASS ) {
		ret = -ESTD_FAULT;
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug_prio_1, M_LIB_THREAD__MODULE_ID, "%s():  successfully\n",__func__);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
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

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
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

    msg (LOG_LEVEL_debug_prio_1, M_LIB_THREAD__MODULE_ID, "%s():  successfully\n",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s() : failed with retval %i\n", __func__, ret );
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
	int ret, ret_val;

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
	}

	ret_val = xSemaphoreTake(_hdl->rtos_sem_hdl, _milliseconds / portTICK_PERIOD_MS);
	switch(ret_val) {
		case pdPASS : ret = EOK; break;
		case pdFAIL : ret = -EEXEC_TO; break;
		default : ret = -ESTD_FAULT; break;
	}

	if (ret < EOK) {
		goto ERR_0;
	}

    msg (LOG_LEVEL_debug_prio_1, M_LIB_THREAD__MODULE_ID, "%s():  successfully\n",__func__);

	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s() : failed with retval %i\n", __func__, ret );
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

	if (_hdl == NULL){
		ret = -EPAR_NULL;
		goto ERR_0;
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

	msg (LOG_LEVEL_debug_prio_1, M_LIB_THREAD__MODULE_ID, "%s(): successfully\n",__func__);
	return EOK;

	ERR_0:
	msg (LOG_LEVEL_error, M_LIB_THREAD__MODULE_ID, "%s(): failed with retval %i\n",__func__, ret );
	return ret;
}

int lib_thread__msleep (unsigned int _milliseconds)
{
	vTaskDelay( _milliseconds / portTICK_PERIOD_MS );
	return EOK;
}









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




