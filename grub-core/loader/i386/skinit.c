/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (c) 2019 Oracle and/or its affiliates. All rights reserved.
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
#include <grub/i386/skinit.h>
#include <grub/i386/tpm.h>
#include <grub/crypto.h>

#define LZ_TAG_CLASS_MASK       0xF0

/* Tags with no particular class */
#define LZ_TAG_NO_CLASS         0x00
#define LZ_TAG_END              0x00
#define LZ_TAG_UNAWARE_OS       0x01
#define LZ_TAG_TAGS_SIZE        0x0F  /* Always first */

/* Tags specifying kernel type */
#define LZ_TAG_BOOT_CLASS       0x10
#define LZ_TAG_BOOT_LINUX       0x10
#define LZ_TAG_BOOT_MB2         0x11

/* Tags specific to TPM event log */
#define LZ_TAG_EVENT_LOG_CLASS  0x20
#define LZ_TAG_EVENT_LOG        0x20
#define LZ_TAG_LZ_HASH          0x21

struct lz_tag_hdr {
  grub_uint8_t type;
  grub_uint8_t len;
} __attribute__ (( packed ));

struct lz_tag_tags_size {
  struct lz_tag_hdr hdr;
  grub_uint16_t size;
} __attribute__ (( packed ));

struct lz_tag_boot_linux {
  struct lz_tag_hdr hdr;
  grub_uint32_t zero_page;
} __attribute__ (( packed ));

struct lz_tag_boot_mb2 {
  struct lz_tag_hdr hdr;
  grub_uint32_t mbi;
	grub_uint32_t kernel_entry;
  grub_uint32_t kernel_size;
} __attribute__ (( packed ));

struct lz_tag_evtlog {
  struct lz_tag_hdr hdr;
  grub_uint32_t address;
  grub_uint32_t size;
} __attribute__ (( packed ));

struct lz_tag_hash {
  struct lz_tag_hdr hdr;
  grub_uint16_t algo_id;
  grub_uint8_t digest[];
} __attribute__ (( packed ));

static inline struct lz_tag_tags_size *get_bootloader_data_addr (
          struct grub_slaunch_params *slparams)
{
  return (struct lz_tag_tags_size *)(slparams->lz_base + slparams->lz_size);
}

static inline void *next_tag(struct lz_tag_tags_size *tags)
{
  return (void *)(((grub_uint8_t *)tags) + tags->size);
}

grub_err_t
grub_skinit_boot_prepare (struct grub_slaunch_params *slparams)
{
  void *lz_base = (void *)(grub_addr_t) slparams->lz_base;
  grub_memset (lz_base, 0, GRUB_SKINIT_SLB_SIZE);
  grub_memcpy (lz_base, grub_slaunch_module(), slparams->lz_size);

  struct lz_tag_tags_size *tags = get_bootloader_data_addr (slparams);
  grub_uint32_t *apic = (grub_uint32_t *)0xfee00300ULL;

  /* Tags header */
  tags->hdr.type = LZ_TAG_TAGS_SIZE;
  tags->hdr.len = sizeof(struct lz_tag_tags_size);
  tags->size = sizeof(struct lz_tag_tags_size);

  /* Hashes of LZ */
  {
    grub_uint8_t buff[GRUB_CRYPTO_MAX_MD_CONTEXT_SIZE];  /* SHA1 ctx is smaller */
    struct lz_tag_hash *h = next_tag(tags);
    h->hdr.type = LZ_TAG_LZ_HASH;
    h->hdr.len = sizeof(struct lz_tag_hash) + GRUB_MD_SHA256->mdlen;
    h->algo_id = 0x000B;
    GRUB_MD_SHA256->init(buff);
    GRUB_MD_SHA256->write(buff, lz_base, slparams->lz_size);
    GRUB_MD_SHA256->final(buff);
    grub_memcpy(h->digest, GRUB_MD_SHA256->read(buff), GRUB_MD_SHA256->mdlen);
    tags->size += h->hdr.len;

    h = next_tag(tags);
    h->hdr.type = LZ_TAG_LZ_HASH;
    h->hdr.len = sizeof(struct lz_tag_hash) + GRUB_MD_SHA1->mdlen;
    h->algo_id = 0x0004;
    GRUB_MD_SHA1->init(buff);
    GRUB_MD_SHA1->write(buff, lz_base, slparams->lz_size);
    GRUB_MD_SHA1->final(buff);
    grub_memcpy(h->digest, GRUB_MD_SHA1->read(buff), GRUB_MD_SHA1->mdlen);
    tags->size += h->hdr.len;
  }

  /* Boot protocol data */
  struct lz_tag_boot_linux *b = next_tag(tags);
  b->hdr.type = LZ_TAG_BOOT_LINUX;
  b->hdr.len = sizeof(struct lz_tag_boot_linux);
  b->zero_page = (grub_uint32_t)slparams->params;
  tags->size += b->hdr.len;

  if (slparams->tpm_evt_log_size != 0) {
    struct lz_tag_evtlog *e = next_tag(tags);
    e->hdr.type = LZ_TAG_EVENT_LOG;
    e->hdr.len = sizeof(struct lz_tag_evtlog);
    e->address = slparams->tpm_evt_log_base;
    e->size = slparams->tpm_evt_log_size;
    tags->size += e->hdr.len;
  }

  /* Mark end of tags */
  struct lz_tag_hdr *end = next_tag(tags);
  end->type = LZ_TAG_END;
  end->len = sizeof(struct lz_tag_hdr);
  tags->size += end->len;

  grub_dprintf ("slaunch", "broadcasting INIT\r\n");
  *apic = 0x000c0500;               // INIT, all excluding self
  grub_dprintf ("slaunch", "grub_tpm_relinquish_lcl\r\n");
  grub_tpm_relinquish_lcl (0);

  grub_dprintf("slaunch", "Invoke SKINIT\r\n");

  return GRUB_ERR_NONE;
}
