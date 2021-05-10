/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2015, Linaro Limited
 * Copyright (c) 2014, STMicroelectronics International N.V.
 */

#ifndef TEE_ENTRY_STD_H
#define TEE_ENTRY_STD_H

/* Main Rushmore Function */
#define OPTEE_SMC_FUNCID_RUSHMORE 99
#define OPTEE_SMC_RUSHMORE \
    OPTEE_SMC_STD_CALL_VAL(OPTEE_SMC_FUNCID_RUSHMORE)

/* Thread Function */
#define OPTEE_SMC_FUNCID_RUSHMORE_THREAD 98
#define OPTEE_SMC_RUSHMORE_THREAD \
    OPTEE_SMC_STD_CALL_VAL(OPTEE_SMC_FUNCID_RUSHMORE_THREAD)

#include <kernel/thread.h>
#include <optee_msg.h>

/*
 * Standard call entry, __weak, overridable. If overridden should call
 * __tee_entry_std() at the end in order to handle the standard functions.
 *
 * These functions are called in a normal thread context.
 */
uint32_t tee_entry_std(struct optee_msg_arg *arg, uint32_t num_params);
uint32_t __tee_entry_std(struct optee_msg_arg *arg, uint32_t num_params);

/* Get list head for sessions opened from non-secure */
void nsec_sessions_list_head(struct tee_ta_session_head **open_sessions);

#endif /* TEE_ENTRY_STD_H */
