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
 */

#include <grub/loader.h>
#include <grub/memory.h>
#include <grub/normal.h>
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/cpu/relocator.h>
#include <grub/i386/cpuid.h>
#include <grub/i386/msr.h>
#include <grub/i386/mmio.h>
#include <grub/i386/slaunch.h>
#include <grub/i386/txt.h>

GRUB_MOD_LICENSE ("GPLv3+");

static grub_uint32_t slp = SLP_NONE;
static struct grub_slaunch_module *modules = NULL, *modules_last = NULL;
static struct grub_relocator *relocator = NULL;

struct grub_slaunch_module*
grub_slaunch_get_modules( void)
{
  return modules;
}

static void *slaunch_module = NULL;

grub_uint32_t
grub_slaunch_platform_type (void)
{
  return slp;
}

void *
grub_slaunch_module (void)
{
  return slaunch_module;
}

static grub_err_t
grub_slaunch_add_module (void *addr, grub_addr_t target, grub_size_t size)
{
  struct grub_slaunch_module *newmod;

  newmod = grub_malloc (sizeof (*newmod));
  if (!newmod)
    return grub_errno;
  newmod->addr = (grub_uint8_t*)addr;
  newmod->target = target;
  newmod->size = size;
  newmod->next = 0;

  if (modules_last)
    modules_last->next = newmod;
  else
    modules = newmod;
  modules_last = newmod;

  return GRUB_ERR_NONE;
}

static void
grub_slaunch_free (void)
{
  struct grub_slaunch_module *cur, *next;

  for (cur = modules; cur; cur = next)
    {
      next = cur->next;
      grub_free (cur);
    }
  modules = NULL;
  modules_last = NULL;

  grub_relocator_unload (relocator);
  relocator = NULL;
}

static grub_err_t
grub_cmd_slaunch (grub_command_t cmd __attribute__ ((unused)),
		  int argc __attribute__ ((unused)),
		  char *argv[] __attribute__ ((unused)))
{
  grub_uint32_t manufacturer[3];
  grub_uint32_t eax, ebx, ecx, edx;
  grub_uint64_t msr_value;
  grub_err_t err;

  if (!grub_cpu_is_cpuid_supported ())
    return grub_error (GRUB_ERR_BAD_DEVICE, N_("CPUID is unsupported"));

  err = grub_cpu_is_msr_supported ();

  if (err != GRUB_ERR_NONE)
    return grub_error (err, N_("MSRs are unsupported"));

  grub_cpuid (0, eax, manufacturer[0], manufacturer[2], manufacturer[1]);

  if (!grub_memcmp (manufacturer, "GenuineIntel", 12))
    {
      err = grub_txt_init ();

      if (err != GRUB_ERR_NONE)
	return err;

      slp = SLP_INTEL_TXT;
    }
  else if (!grub_memcmp (manufacturer, "AuthenticAMD", 12))
    {

      grub_cpuid (GRUB_AMD_CPUID_FEATURES, eax, ebx, ecx, edx);
      if (! (ecx & GRUB_SVM_CPUID_FEATURE) )
        return grub_error (GRUB_ERR_BAD_DEVICE, N_("CPU does not support AMD SVM"));

      /* Check whether SVM feature is disabled in BIOS */
      msr_value = grub_rdmsr (GRUB_MSR_AMD64_VM_CR);
      if (msr_value & GRUB_MSR_SVM_VM_CR_SVM_DISABLE)
        return grub_error (GRUB_ERR_BAD_DEVICE, N_("BIOS has AMD SVM disabled"));

      slp = SLP_AMD_SKINIT;
    }
  else
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, N_("CPU is unsupported"));

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_slaunch_module (grub_command_t cmd __attribute__ ((unused)),
			 int argc, char *argv[])
{
  grub_file_t file;
  grub_ssize_t size;
  grub_err_t err;
  grub_relocator_chunk_t ch;
  void *slaunch_module_addr = NULL;
  grub_addr_t slaunch_module_target;

  if (!argc)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

  if (slp == SLP_NONE)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("secure launch not enabled"));

  grub_errno = GRUB_ERR_NONE;

  if (! relocator)
  {
    relocator = grub_relocator_new ();
    if (! relocator)
      return grub_errno;
  }

  file = grub_file_open (argv[0], GRUB_FILE_TYPE_SLAUNCH_MODULE);

  if (file == NULL)
    return grub_errno;

  size = grub_file_size (file);

  if (!size)
    {
      grub_error (GRUB_ERR_BAD_ARGUMENT, N_("file size is zero"));
      goto fail;
    }

  err = grub_relocator_alloc_chunk_align (relocator, &ch,
					  0, (0xffffffff - size) + 1,
					  size > 0x10000 ? size : 0x10000, /* Alloc at least 64k */
					  0x10000,	/* SLB must be 64k aligned */
					  GRUB_RELOCATOR_PREFERENCE_LOW, 1);
  if (err)
    goto fail;

  slaunch_module_addr = get_virtual_current_address (ch);
  slaunch_module_target = get_physical_target_address (ch);

  err = grub_slaunch_add_module (slaunch_module_addr,
				 slaunch_module_target,
				 size);
  if (err)
    goto fail;

  if (grub_file_read (file, slaunch_module_addr, size) != size)
    {
      if (grub_errno == GRUB_ERR_NONE)
	grub_error (GRUB_ERR_FILE_READ_ERROR, N_("premature end of file: %s"),
		    argv[0]);
      goto fail;
    }

  if (slp == SLP_INTEL_TXT)
    {
      if (!grub_txt_is_sinit_acmod (slaunch_module, size))
	{
	  grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("it does not look like SINIT ACM"));
	  goto fail;
	}

      if (!grub_txt_acmod_match_platform (slaunch_module))
	{
	  grub_error (GRUB_ERR_BAD_FILE_TYPE, N_("SINIT ACM does not match platform"));
	  goto fail;
	}
    }

  grub_file_close (file);

  return GRUB_ERR_NONE;

 fail:
  grub_error_push ();

  grub_free (slaunch_module);
  grub_file_close (file);

  slaunch_module = NULL;

  grub_error_pop ();

  return grub_errno;
}

static grub_err_t
grub_cmd_slaunch_state (grub_command_t cmd __attribute__ ((unused)),
			int argc __attribute__ ((unused)),
			char *argv[] __attribute__ ((unused)))
{
  if (slp == SLP_NONE)
    grub_printf ("Secure launcher: Disabled\n");
  else if (slp == SLP_INTEL_TXT)
    {
      grub_printf ("Secure launcher: Intel TXT\n");
      grub_txt_state_show ();
    }
  else if (slp == SLP_AMD_SKINIT)
    {
      grub_printf ("Secure launcher: AMD SKINIT\n");
    }
  return GRUB_ERR_NONE;
}

static grub_command_t cmd_slaunch, cmd_slaunch_module, cmd_slaunch_state;

GRUB_MOD_INIT (slaunch)
{
  cmd_slaunch = grub_register_command ("slaunch", grub_cmd_slaunch,
				       NULL, N_("Enable secure launcher"));
  cmd_slaunch_module = grub_register_command ("slaunch_module", grub_cmd_slaunch_module,
					      NULL, N_("Secure launcher module command"));
  cmd_slaunch_state = grub_register_command ("slaunch_state", grub_cmd_slaunch_state,
					     NULL, N_("Display secure launcher state"));
}

GRUB_MOD_FINI (slaunch)
{
  grub_slaunch_free ();
  grub_unregister_command (cmd_slaunch_state);
  grub_unregister_command (cmd_slaunch_module);
  grub_unregister_command (cmd_slaunch);

  if (slp == SLP_INTEL_TXT)
    grub_txt_shutdown ();
}
