/*
 * This file is part of the EMBTOM project
 * Copyright (c) 2018-2019 Thomas Willetal 
 * (https://github.com/tom3333)
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

#ifndef _LIB_THREAD_WAKEUP_H_
#define _LIB_THREAD_WAKEUP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct internal_wakeup* wakeup_hdl_t;	  /* opaque pointer to a wakeup object handle */

/* ************************************************************************//**
 * \brief Init of the wakeup component
 *
 * Attention:
 * At the POSIX environment have to be called at the start of the "main"
 * because the signal mask are modified
 *
 * \return EOK, if successful, < EOK if not successful
 * ****************************************************************************/
int lib_thread__wakeup_init(void);

/* ************************************************************************//**
 * \brief Cleanup wakeup component
 * 
 * \return EOK, if successful, < EOK if not successful
 * ****************************************************************************/
int lib_thread__wakeup_cleanup(void);

/* ************************************************************************//**
 * \brief Create a new wakeup object with the specified interval
 *
 * \param	*_wu_obj[out]	:   pointer to a wakeup object handle
 * \param	_interval			wakeup interval in ms (must not be 0)
 * \return	EOK				:   Success
 *			-EEXEC_NOINIT	:   Component not initialized
 *			-EPAR_NULL		:   NULL pointer specified for _wu_obj
 *			-ESTD_INVAL	    :   _interval is 0
 *			-ESTD_NOMEM		:   Insufficient memory available to initialize the wakeup object
 *			-ESTD_AGAIN		:   Not enougth timer resources available
 * ****************************************************************************/
int lib_thread__wakeup_create(wakeup_hdl_t *_wu_obj, unsigned _interval);

/* ************************************************************************//**
 * \brief	Destroy an initialized wakeup object
 *
 * \param	*_wu_obj[in/out] :	pointer to a wakeup object handle
 * \return	EOK				 :  Success
 *			-EEXEC_NOINIT	 :  Component not initialized
 *			-EPAR_NULL		 :  NULL pointer for _wu_obj
 *			-ESTD_INVAL		 :  _wu_obj is invalid
 * ****************************************************************************/
int lib_thread__wakeup_destroy(wakeup_hdl_t *_wu_obj);

/* ************************************************************************//**
 * \brief	A new cyclic wait interval is set to the wakeup object
 *
 * \param	_wu_obj	[in/out] :  pointer to a wakeup object handle
 * \return	EOK				 :  Success
 *			-EEXEC_NOINIT	 :  Component not initialized
 *			-EPAR_NULL		 :  NULL pointer for _wu_obj
 *			-PAR_NULL		 :  _wu_obj is invalid
 * ****************************************************************************/
int lib_thread__wakeup_setinterval(wakeup_hdl_t _wu_obj, unsigned _interval);

/* ************************************************************************//**
 * \brief	Wait on the specified wakeup object
 *
 * This function waits until the specified wakeup object's timeout interval
 * has elapsed. This function should be called at the top of a while or 
 * for loop.
 * If the wakeup object will be destroyed the wakeup wait routine unblocks
 * with -ESTD_INTR
 *
 * \param	*_wu_obj [in/out]:	pointer to a wakeup object handle
 * \return	EOK				 :  Success
 *			-EEXEC_NOINIT	 :  Component not (yet) initialized (any more)
 *			-EPAR_NULL		 :  NULL pointer for _wu_obj
 *			-ESTD_INTR		 :  The wakeup object is destroyed
 * ****************************************************************************/
int lib_thread__wakeup_wait(wakeup_hdl_t _wu_obj);

#ifdef __cplusplus
}
#endif

#endif /* _LIB_THREAD_WAKEUP_H_ */
