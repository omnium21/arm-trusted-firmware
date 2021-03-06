/*
 * Copyright (c) 2013-2014, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <arch_helpers.h>
#include <console.h>
#include <platform.h>
#include <psci_private.h>
#include <runtime_svc.h>
#include <debug.h>
#include <context_mgmt.h>

/*******************************************************************************
 * PSCI frontend api for servicing SMCs. Described in the PSCI spec.
 ******************************************************************************/
int psci_cpu_on(unsigned long target_cpu,
		unsigned long entrypoint,
		unsigned long context_id)

{
	int rc;
	unsigned int start_afflvl, end_afflvl;

	/* Determine if the cpu exists of not */
	rc = psci_validate_mpidr(target_cpu, MPIDR_AFFLVL0);
	if (rc != PSCI_E_SUCCESS) {
		goto exit;
	}

	/*
	 * To turn this cpu on, specify which affinity
	 * levels need to be turned on
	 */
	start_afflvl = MPIDR_AFFLVL0;
	end_afflvl = get_max_afflvl();
	rc = psci_afflvl_on(target_cpu,
			    entrypoint,
			    context_id,
			    start_afflvl,
			    end_afflvl);

exit:
	return rc;
}

unsigned int psci_version(void)
{
	return PSCI_MAJOR_VER | PSCI_MINOR_VER;
}

int psci_cpu_suspend(unsigned int power_state,
		     unsigned long entrypoint,
		     unsigned long context_id)
{
	int rc;
	unsigned long mpidr;
	unsigned int target_afflvl, pstate_type;

	/* TODO: Standby states are not supported at the moment */
	pstate_type = psci_get_pstate_type(power_state);
	if (pstate_type == 0) {
		rc = PSCI_E_INVALID_PARAMS;
		goto exit;
	}

	/* Sanity check the requested state */
	target_afflvl = psci_get_pstate_afflvl(power_state);
	if (target_afflvl > MPIDR_MAX_AFFLVL) {
		rc = PSCI_E_INVALID_PARAMS;
		goto exit;
	}

	mpidr = read_mpidr();
	rc = psci_afflvl_suspend(mpidr,
				 entrypoint,
				 context_id,
				 power_state,
				 MPIDR_AFFLVL0,
				 target_afflvl);

exit:
	if (rc != PSCI_E_SUCCESS)
		assert(rc == PSCI_E_INVALID_PARAMS);
	return rc;
}

int psci_cpu_off(void)
{
	int rc;
	unsigned long mpidr;
	int target_afflvl = get_max_afflvl();

	mpidr = read_mpidr();

	/*
	 * Traverse from the highest to the lowest affinity level. When the
	 * lowest affinity level is hit, all the locks are acquired. State
	 * management is done immediately followed by cpu, cluster ...
	 * ..target_afflvl specific actions as this function unwinds back.
	 */
	rc = psci_afflvl_off(mpidr, MPIDR_AFFLVL0, target_afflvl);

	/*
	 * The only error cpu_off can return is E_DENIED. So check if that's
	 * indeed the case.
	 */
	assert (rc == PSCI_E_SUCCESS || rc == PSCI_E_DENIED);

	return rc;
}

int psci_affinity_info(unsigned long target_affinity,
		       unsigned int lowest_affinity_level)
{
	int rc = PSCI_E_INVALID_PARAMS;
	unsigned int aff_state;
	aff_map_node *node;

	if (lowest_affinity_level > get_max_afflvl())
		return rc;

	node = psci_get_aff_map_node(target_affinity, lowest_affinity_level);
	if (node && (node->state & PSCI_AFF_PRESENT)) {

		/*
		 * TODO: For affinity levels higher than 0 i.e. cpu, the
		 * state will always be either ON or OFF. Need to investigate
		 * how critical is it to support ON_PENDING here.
		 */
		aff_state = psci_get_state(node);

		/* A suspended cpu is available & on for the OS */
		if (aff_state == PSCI_STATE_SUSPEND) {
			aff_state = PSCI_STATE_ON;
		}

		rc = aff_state;
	}

	return rc;
}

/* Unimplemented */
int psci_migrate(unsigned int target_cpu)
{
	return PSCI_E_NOT_SUPPORTED;
}

/* Unimplemented */
unsigned int psci_migrate_info_type(void)
{
	return PSCI_TOS_NOT_PRESENT;
}

unsigned long psci_migrate_info_up_cpu(void)
{
	/*
	 * Return value of this currently unsupported call depends upon
	 * what psci_migrate_info_type() returns.
	 */
	return PSCI_E_SUCCESS;
}

/* Unimplemented */
void psci_system_off(void)
{
	assert(0);
}

/* Unimplemented */
void psci_system_reset(void)
{
	assert(0);
}

/*******************************************************************************
 * PSCI top level handler for servicing SMCs.
 ******************************************************************************/
uint64_t psci_smc_handler(uint32_t smc_fid,
			  uint64_t x1,
			  uint64_t x2,
			  uint64_t x3,
			  uint64_t x4,
			  void *cookie,
			  void *handle,
			  uint64_t flags)
{
	uint64_t rc;

	switch (smc_fid) {
	case PSCI_VERSION:
		rc = psci_version();
		break;

	case PSCI_CPU_OFF:
		rc = __psci_cpu_off();
		break;

	case PSCI_CPU_SUSPEND_AARCH64:
	case PSCI_CPU_SUSPEND_AARCH32:
		rc = __psci_cpu_suspend(x1, x2, x3);
		break;

	case PSCI_CPU_ON_AARCH64:
	case PSCI_CPU_ON_AARCH32:
		rc = psci_cpu_on(x1, x2, x3);
		break;

	case PSCI_AFFINITY_INFO_AARCH32:
	case PSCI_AFFINITY_INFO_AARCH64:
		rc = psci_affinity_info(x1, x2);
		break;

	case PSCI_MIG_AARCH32:
	case PSCI_MIG_AARCH64:
		rc = psci_migrate(x1);
		break;

	case PSCI_MIG_INFO_TYPE:
		rc = psci_migrate_info_type();
		break;

	case PSCI_MIG_INFO_UP_CPU_AARCH32:
	case PSCI_MIG_INFO_UP_CPU_AARCH64:
		rc = psci_migrate_info_up_cpu();
		break;

	case PSCI_SYSTEM_OFF:
		psci_system_off();
		assert(0);

	case PSCI_SYSTEM_RESET:
		psci_system_reset();
		assert(0);

	default:
		rc = SMC_UNK;
		WARN("Unimplemented psci call -> 0x%x \n", smc_fid);
	}

	SMC_RET1(handle, rc);
}
