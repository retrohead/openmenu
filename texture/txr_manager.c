/*
 * File: txr_manager.c
 * Project: texture
 * File Created: Friday, 21st May 2021 2:30:13 pm
 * Author: Hayden Kowalchuk
 * -----
 * Copyright (c) 2021 Hayden Kowalchuk, Hayden Kowalchuk
 * License: BSD 3-clause "New" or "Revised" License, http://www.opensource.org/licenses/BSD-3-Clause
 */

#include "txr_manager.h"

#include <dc/pvr.h>
#include <stdio.h>
#include <string.h>

#include "../dat_format.h"
#include "../ui/draw_kos.h"
#include "../ui/draw_prototypes.h"
#include "block_pool.h"
#include "lru.h"

/* CFG for small pvr pool (128x128 16bit, 16 spaces) */
#define SM_SLOT_NUM (16)
#define SM_SLOT_SIZE (128 * 128 * 2)
#define SM_POOL_SIZE (SM_SLOT_NUM * SM_SLOT_SIZE * sizeof(char))

/* CFG for large pvr pool (256x256 16bit, 4 spaces) */
#define LG_SLOT_NUM (4)
#define LG_SLOT_SIZE (256 * 256 * 2)
#define LG_POOL_SIZE (LG_SLOT_NUM * LG_SLOT_SIZE * sizeof(char))

static cache_instance cache_small;
static cache_instance cache_large;
static block_pool pvr_small;
static block_pool pvr_large;
static dat_file dat_icon;
static dat_file dat_box;

extern const char *serial_santize(const char *id);
extern int serial_sanitizer_init(void);

unsigned int block_pool_add_cb(const char *key, void *user) {
  block_pool *pool = (block_pool *)user;
  unsigned int ret;
  char *ptr;
  pool_get_next_free(pool, &ret, (void **)&ptr);
  return ret;
}

unsigned int block_pool_del_cb(const char *key, void *value, void *user) {
  block_pool *pool = (block_pool *)user;
  unsigned int slot_num = *(unsigned int *)value;
  pool_dealloc_slot(pool, slot_num);
  return 0;
}

int txr_load_DATs(void) {
  serial_sanitizer_init();

  DAT_init(&dat_icon);
  DAT_init(&dat_box);
  DAT_load_parse(&dat_icon, "ICON.DAT");
  DAT_load_parse(&dat_box, "BOX.DAT");

  return 0;
}

int txr_create_small_pool(void) {
  void *buffer = pvr_mem_malloc(SM_POOL_SIZE);
  pool_create(&pvr_small, buffer, SM_POOL_SIZE, SM_SLOT_NUM);
  cache_small.cache = NULL;
  cache_set_size(&cache_small, SM_SLOT_NUM);
  cache_callback_userdata(&cache_small, &pvr_small);
  cache_callback_add(&cache_small, block_pool_add_cb);
  cache_callback_del(&cache_small, block_pool_del_cb);

  return 0;
}

/* pool_destroy_user(&pvr_small, pvr_mem_free); */

int txr_create_large_pool(void) {
  void *buffer = pvr_mem_malloc(LG_POOL_SIZE);
  pool_create(&pvr_large, buffer, LG_POOL_SIZE, LG_SLOT_NUM);
  cache_large.cache = NULL;
  cache_set_size(&cache_large, LG_SLOT_NUM);
  cache_callback_userdata(&cache_large, &pvr_large);
  cache_callback_add(&cache_large, block_pool_add_cb);
  cache_callback_del(&cache_large, block_pool_del_cb);
  return 0;
}

/*
called with "T1121.pvr" and a pointer to pointer to vram
returns pointer to use for texture upload/reference
 */
int txr_get_small(const char *id, struct image *img) {
  void *txr_ptr;
  int slot_num;
  const char *id_santized = serial_santize(id);

#ifdef LOOSE_FILES
  /* construct full filename */
  char buffer[64] = {0};
  strcat(buffer, "/cd/icon/");
  strcat(buffer, id_santized);
  strcat(buffer, ".PVR");

  /* check if exists and if not, return missing image */
  if (!file_exists(buffer)) {
    draw_load_missing_icon(img);
  } else
#else
  /* check if exists in DAT and if not, return missing image */
  if (!DAT_get_offset_by_ID(&dat_icon, id_santized)) {
    draw_load_missing_icon(img);
  } else
#endif
  {
    slot_num = find_in_cache(&cache_small, id_santized);
    if (slot_num == -1) {
      add_to_cache(&cache_small, id_santized, 0);
      slot_num = find_in_cache(&cache_small, id_santized);
      txr_ptr = pool_get_slot_addr(&pvr_small, slot_num);

      /* now load the texture into vram */
#ifdef LOOSE_FILES
      draw_load_texture_buffer(buffer, img, txr_ptr);
#else
      draw_load_texture_from_DAT_to_buffer(&dat_icon, id_santized, img, txr_ptr);
#endif
    } else {
      img->width = 128;
      img->height = 128;
      img->format = (0 << 26) | (1 << 27); /* RGB565, Twiddled */
      img->texture = pool_get_slot_addr(&pvr_small, slot_num);
    }
  }
  return 0;
}

int txr_get_large(const char *id, struct image *img) {
#if 1
  void *txr_ptr;
  int slot_num;
  const char *id_santized = serial_santize(id);

#ifdef LOOSE_FILES
  /* construct full filename */
  char buffer[64] = {0};
  strcat(buffer, "/cd/box/");
  strcat(buffer, id_santized);
  strcat(buffer, ".pvr");

  /* check if exists and if not, return missing image */
  if (!file_exists(buffer)) {
    draw_load_missing_icon(img);
  } else
#else
  /* check if exists in DAT and if not, return missing image */
  if (!DAT_get_offset_by_ID(&dat_box, id_santized)) {
    draw_load_missing_icon(img);
  } else
#endif
  {
    slot_num = find_in_cache(&cache_large, id_santized);
    if (slot_num == -1) {
      add_to_cache(&cache_large, id_santized, 0);
      slot_num = find_in_cache(&cache_large, id_santized);
      txr_ptr = pool_get_slot_addr(&pvr_large, slot_num);

      /* now load the texture into vram */
#ifdef LOOSE_FILES
      draw_load_texture_buffer(buffer, img, txr_ptr);
#else
      draw_load_texture_from_DAT_to_buffer(&dat_box, id_santized, img, txr_ptr);
#endif
    } else {
      img->width = 256;
      img->height = 256;
      img->format = (0 << 26) | (1 << 27); /* RGB565, Twiddled */
      img->texture = pool_get_slot_addr(&pvr_large, slot_num);
    }
  }
#endif
  return 0;
}