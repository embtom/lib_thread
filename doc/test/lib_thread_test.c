#include <lib_thread.h>
#include "lib_thread_test.h"
#include <lib_log.h>
#include <lib_convention__errno.h>
#include <errno.h>

struct thd_arg
{
	thread_hdl_t thd_hdl;
	signal_hdl_t sgn_hdl;
	mutex_hdl_t mtx_hdl;
};


static struct thd_arg signal_sender_arg;
static struct thd_arg signal_waiter_arg[2];



static char ch;

static void *print_stern (void *_arg);
static void *print_minus (void *_arg);
static void *signal_sender (void *_arg);
static void *signal_waiter (void *_arg);


void lib_thread__test_init (void){

	int ret;
	void *ret_val;
	static thread_hdl_t	hdl_print_minus, hdl_print_stern = LIB_THREAD__THD_INITIALIZER;
	static mutex_hdl_t hdl_mutex;

	/* MUTEX_TEST */
	ret = lib_thread__mutex_init(&hdl_mutex);
	ret = lib_thread__create (&hdl_print_stern, &print_stern, &hdl_mutex, -1, "print_stern");
	ret = lib_thread__create (&hdl_print_minus, &print_minus, &hdl_mutex, 1, "print_minus");
	lib_thread__join(&hdl_print_stern, NULL);
	lib_thread__join(&hdl_print_minus, NULL);
	lib_thread__mutex_destroy(&hdl_mutex);



	/* SIGNAL_TEST */
	ret = lib_thread__signal_init(&(signal_sender_arg.sgn_hdl));
	ret = lib_thread__mutex_init(&(signal_sender_arg.mtx_hdl));

	signal_waiter_arg[0].sgn_hdl = signal_sender_arg.sgn_hdl;
	signal_waiter_arg[0].mtx_hdl = signal_sender_arg.mtx_hdl;
	signal_waiter_arg[1].sgn_hdl = signal_sender_arg.sgn_hdl;
	signal_waiter_arg[1].mtx_hdl = signal_sender_arg.mtx_hdl;

	//MUTEX is requiered, because the thread id necessaris created
	lib_thread__mutex_lock(signal_sender_arg.mtx_hdl);
	ret = lib_thread__create (&signal_sender_arg.thd_hdl, &signal_sender, &signal_sender_arg, 3, "signal_sender");
 	ret = lib_thread__create (&signal_waiter_arg[0].thd_hdl, &signal_waiter, &signal_waiter_arg[0], 1, "signal_waiter_1" );
//	ret = lib_thread__create (&signal_waiter_arg[1].thd_hdl, &signal_waiter, &signal_waiter_arg[1], 2, "signal_waiter_2" );
	lib_thread__mutex_unlock(signal_sender_arg.mtx_hdl);




	lib_thread__join(&signal_sender_arg.thd_hdl,&ret_val);
	lib_thread__signal_destroy(&signal_sender_arg.sgn_hdl);
	lib_thread__join(&signal_waiter_arg[0].thd_hdl, NULL);
//	lib_thread__join(&signal_waiter_arg[1].thd_hdl, NULL);


}



static void *print_stern (void *_arg)
{
	mutex_hdl_t mtx = *((mutex_hdl_t*)(_arg));

//	lib_thread__mutex_trylock (mtx);
	lib_thread__mutex_lock(mtx);
	ch = '*';
	lib_thread__msleep(1);
	msg(LOG_LEVEL_info, "main", "%c", ch);
	lib_thread__mutex_unlock(mtx);
	return NULL;
}

static void *print_minus (void *_arg)
{
	mutex_hdl_t mtx = *((mutex_hdl_t*)(_arg));

	lib_thread__mutex_trylock(mtx);
	//lib_thread__mutex_lock(mtx);
	ch = '-';
	lib_thread__msleep(1);
	msg(LOG_LEVEL_info, "main", "%c", ch);
	lib_thread__mutex_unlock(mtx);
	return NULL;
}


static void* signal_sender (void *_arg)
{
	int count = 0;

	struct thd_arg *arg = (struct thd_arg*)_arg;
	msg(LOG_LEVEL_info, "main", "Test_worker\n");

	while (count < 10)
	{
		lib_thread__msleep(1000);
		lib_thread__signal_send(arg->sgn_hdl);

		msg(LOG_LEVEL_info, "main", "Test_worker count %u \n", count);
		count++;
	}

	return NULL;
}

static void *signal_waiter (void *_arg)
{
	int ret;
	//signal_hdl_t sgn = *((signal_hdl_t*)(_arg));
	char name [30] = {0};

	struct thd_arg *arg = (struct thd_arg*)_arg;
	int count = 0;

	lib_thread__mutex_lock(arg->mtx_hdl);

	lib_thread__getname(arg->thd_hdl, &name[0], sizeof(name));

	lib_thread__mutex_unlock(arg->mtx_hdl);

	//printf("signal_waiter,%s\n",name);
	//while (lib_thread__signal_wait(arg->sgn_hdl) == EOK)
	while (ret = lib_thread__signal_wait_timedwait(arg->sgn_hdl,5000), ret == EOK)

	{
		msg(LOG_LEVEL_info, "main", "%s:signal_waiter %u\n",name,count);
		count++;
	}

	if (ret == -EEXEC_TO)
	{
		msg(LOG_LEVEL_info, "main","%s:signal_waiter returns timeout\n",name);
	}

	if (ret == -EPERM)
	{
		msg(LOG_LEVEL_info, "main","%s:signal_waiter returns signal destroyed\n",name);
	}


	return NULL;


}



