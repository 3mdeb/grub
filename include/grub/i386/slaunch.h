/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Oracle and/or its affiliates.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Main secure launch definitions header file.
 */

#ifndef GRUB_I386_SLAUNCH_H
#define GRUB_I386_SLAUNCH_H 1

/* Secure launch platform types. */
#define SLP_NONE	0
#define SLP_INTEL_TXT	1
#define SLP_AMD_SKINIT	2

#define GRUB_SLAUNCH_TPM_EVT_LOG_SIZE	(8 * GRUB_PAGE_SIZE)

#ifndef ASM_FILE

#include <grub/i386/linux.h>
#include <grub/types.h>
#include <grub/i386/relocator.h>

#define GRUB_SL_BOOTPARAMS_OFFSET	0x12c
#define GRUB_SL_ZEROPAGE_OFFSET	0x14
#define GRUB_SL_EVENTLOG_ADDR_OFFSET	0x18
#define GRUB_SL_EVENTLOG_SIZE_OFFSET	0x1c

struct grub_slaunch_params
{
  struct linux_kernel_params *params;
  grub_uint32_t mle_start;
  grub_uint32_t mle_size;
  void *mle_ptab_mem;
  grub_uint64_t mle_ptab_target;
  grub_uint32_t mle_ptab_size;
  grub_uint32_t mle_header_offset;
  grub_uint64_t ap_wake_block;
  grub_uint32_t sinit_acm_base;
  grub_uint32_t sinit_acm_size;
  grub_uint64_t tpm_evt_log_base;
  grub_uint32_t tpm_evt_log_size;
  grub_addr_t real_mode_target;
  grub_addr_t prot_mode_target;
  struct grub_relocator *relocator;
};

struct grub_slaunch_module
{
  struct grub_slaunch_module *next;
  grub_uint8_t *addr;
  grub_addr_t target;
  grub_size_t size;
};

struct grub_slaunch_module *grub_slaunch_get_modules (void);

grub_err_t grub_slaunch_boot_skinit (struct grub_slaunch_params *slparams);
grub_err_t grub_slaunch_mb2_boot (struct grub_relocator *rel, struct grub_relocator32_state state);

extern grub_uint32_t grub_slaunch_platform_type (void);
extern void *grub_slaunch_module (void);


#endif /* ASM_FILE */

#endif /* GRUB_I386_SLAUNCH_H */
