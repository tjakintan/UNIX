#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

//Uncomment the below code before implementing cache functions.
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  if (num_entries < 2 || num_entries > 4096) {return -1;}
  if (cache != NULL) {return -1; }
  cache = (cache_entry_t *)malloc(num_entries * sizeof(cache_entry_t));
  if (cache == NULL) {return -1;}

  for (int i = 0; i < num_entries; i++) {
      cache[i].valid = false; 
      cache[i].block_num = -1;
      cache[i].disk_num = -1; 
      memset(cache[i].block, 0, JBOD_BLOCK_SIZE);
  }
  cache_size = num_entries;
  return 1;
}

int cache_destroy(void) {
    if (cache == NULL) { // 'cache_destroy' called more than once 
        return -1; 
    }
    free(cache);  
    cache = NULL;    
    cache_size = 0;  
    clock = 0;       
    return 1; 
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {

  if (cache == NULL || cache_size == 0 || buf == NULL)return -1;
  num_queries++;
  
  for (int i=0; i < cache_size; i++){
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      clock++;
      cache[i].clock_accesses = clock;
      num_hits++;
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
    for (int i = 0; i < cache_size; i++) {
        if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
            memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); 
            cache[i].clock_accesses = clock++; 
        }
    }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) { 
  if (buf == NULL) return -1;                             
  if (cache == NULL || cache_size == 0) return -1;        
  if (disk_num < 0 || disk_num >= JBOD_NUM_DISKS) return -1;  
  if (block_num < 0 || block_num >= JBOD_BLOCK_SIZE) return -1; 

  num_queries++; 
  for (int i = 0; i < cache_size; i++) {  
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
      if (memcmp(buf, cache[i].block, JBOD_BLOCK_SIZE) == 0) return -1; // existing entry check
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);   
      clock++;
      cache[i].clock_accesses = clock;              
      return 1;                                      
    }
  }

  // overwrite Most Recent Used
  int mru_index = -1;
  int max_clock = -1;

  for (int i = 0; i < cache_size; i++) {
    if (!cache[i].valid) {  
      mru_index = i;
      break;
    }
    if (cache[i].clock_accesses > max_clock) { 
      max_clock = cache[i].clock_accesses;
      mru_index = i;
    }
  }
  cache[mru_index].valid = true;
  cache[mru_index].disk_num = disk_num;
  cache[mru_index].block_num = block_num;
  memcpy(cache[mru_index].block, buf, JBOD_BLOCK_SIZE);  
  clock++;
  cache[mru_index].clock_accesses = clock;              

  return 1; 
}

bool cache_enabled(void) {
  if (cache != NULL && cache_size > 0){return true;}
  return false;
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int cache_resize(int new_num_entries) {
  if (new_num_entries < 2 || new_num_entries > 4096) {return -1;}
  if (cache == NULL || cache_size == 0) {return -1;}
  cache_entry_t *new_cache = calloc(new_num_entries, sizeof(cache_entry_t));
  if (new_cache == NULL) {return -1;}

  int copy_left = (new_num_entries < cache_size) ? new_num_entries : cache_size;
  for (int i = 0; i < copy_left; i++) {
        if (cache[i].valid) {new_cache[i] = cache[i];}
  }
  free(cache);

  cache = new_cache;
  cache_size = new_num_entries;
   return 0;
}
