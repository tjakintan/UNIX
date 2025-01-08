#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"

int is_mounted = 0;                                                     
int is_written = 0;                                                     

int diskid;
int blockid;

uint32_t package (uint32_t block, uint32_t disk, uint32_t cmd) {

  uint32_t packvalue = 0x0, tempblock, tempdisk, tempcmd;               

  tempblock = block&0xff;                                               
  tempdisk = (disk&0xff) << 8;                                          
  tempcmd = (cmd&0xff) << 12;                                           
  packvalue = tempblock|tempdisk|tempcmd;                              

  return packvalue;                                                     
}


int mdadm_mount(void) {
    if (is_mounted != 0){
        return -1; // mdadm_mount been called
    }
    else {
      if (jbod_client_operation(package(0,0,JBOD_MOUNT), NULL) == JBOD_NO_ERROR){  //check if mounting will result to any error  
      is_mounted = 1;                                                   //if successfully mounted, set is_mounted = 1 
      return 1; 
    }
    }
    return (jbod_error == JBOD_ALREADY_MOUNTED) ? -1 : -1; // failed to mount
}

int mdadm_unmount(void) {
    if (is_mounted != 1){
        return -1; // mdadm_unmount been called
    }
    else {
      if (jbod_client_operation(package(0,0,JBOD_UNMOUNT), NULL) == JBOD_NO_ERROR){ //check if unmounting will result to any error
      is_mounted = 0;                                                    //if successfully unmounted, set is_mounted = 0
      return 1;
    }
    }
    return (jbod_error == JBOD_ALREADY_MOUNTED) ? -1 : -1; // failed to unmount
}

int mdadm_write_permission(void){
    
	int result = jbod_client_operation(package(0,0,JBOD_WRITE_PERMISSION), NULL);     
  return (result == 0) ? (is_written = 1, 1) : -1;
}


int mdadm_revoke_write_permission(void){

	int result = jbod_client_operation(package(0,0,JBOD_REVOKE_WRITE_PERMISSION), NULL);
  return (result == 0) ? (is_written= 0, 1) : -1;
}


int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  int length = addr + len;                                              
  int boundary = JBOD_NUM_DISKS * JBOD_DISK_SIZE;                       
  int current_addr = addr;                                              
  int read_bytes;                                                       
  int offset;                                                           
  uint8_t *tempbuf = malloc(256);                                       
  jbod_error_t err = JBOD_NO_ERROR;                                     

  int blockvalue = 0;                                                   
  int original_off = addr % 256;                                        
  int blank = 0;                                                        
  
  if (len == 0 && buf == NULL) {                                       
    return 0;
  }

  if (length > boundary) {                                            
    return -1;
  }

  if(is_mounted != 1) {                                                 
    return -1;
  }
  
  if (len > 2048) {                                                    
    return -1;
  } else if (len > 0 && buf == NULL) {                                  
    return -1;
  }

  diskid = addr/JBOD_DISK_SIZE;                                         //locate the disk
  blockid = (addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;                     //locate the block
  
  for(int i = 0 ; i < len ; i+=read_bytes){                             
    
    offset = current_addr % 256;                                        

    err = jbod_client_operation(package(0,diskid, JBOD_SEEK_TO_DISK), NULL);     
    assert (err == JBOD_NO_ERROR);                                      

    err = jbod_client_operation(package(blockid,0, JBOD_SEEK_TO_BLOCK), NULL);  
    assert (err == JBOD_NO_ERROR);                                      

    if (cache_lookup(diskid,blockid,tempbuf) == -1) {                   
      err = jbod_client_operation(package(0,0,JBOD_READ_BLOCK), tempbuf);        
      assert (err == JBOD_NO_ERROR);                                    
      cache_insert(diskid,blockid,tempbuf);                             
    }                                                                   
    
    if(offset != 0){                                                   
      blockvalue++;                                                    
      if (((blockvalue*256) - offset) > len) {                          
        blank = ((blockvalue*256) - original_off) - len;                
        memcpy(buf+i, tempbuf+offset, JBOD_BLOCK_SIZE-offset-blank);    
        read_bytes = 256-offset-blank;                                  
      }else {                                                           
        memcpy(buf+i, tempbuf+offset, JBOD_BLOCK_SIZE-offset);          
        read_bytes = 256-offset;                                       
      }
    }else {
      blockvalue++;                                                     
      if((blockvalue*256)-original_off <= len) {                        
        memcpy(buf + i, tempbuf, JBOD_BLOCK_SIZE);                      
        read_bytes = JBOD_BLOCK_SIZE;                                   
      } else if((blockvalue*256)-original_off > len){                   
        blank = ((blockvalue*256)-original_off)-len;                    
        memcpy(buf + i, tempbuf, JBOD_BLOCK_SIZE-blank);                
        read_bytes = 256-blank;                                         
      }	  
    }
    
    current_addr += read_bytes;                                       
    diskid = current_addr/JBOD_DISK_SIZE;                               
    blockid = (current_addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;          
  }
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  int final_bound = addr + len;                                         
  int write_bound = JBOD_NUM_DISKS * JBOD_DISK_SIZE;                    
  int current_addr = addr;                                              
  int read_bytes;                                                       
  int offset;                                                           
  uint8_t *tempbuf = malloc(256);                                       
  jbod_error_t err = JBOD_NO_ERROR;                                    

  int blockvalue = 0;                                                   
  int original_off = addr % 256;                                        
  int blank = 0;                                                       

  if (len == 0 && buf == NULL) {                                       
    return 0;
  }
  
  if (final_bound > write_bound) {                                      
    return -1;
  }
  
  if (is_mounted != 1) {                                                
    return -1;
  }
  
  if(len > 2048) {                                                      
    return -1;
  } else if (len > 0 && buf == NULL) {                                 
    return -1;
  }

  diskid = addr/JBOD_DISK_SIZE;                                         
  blockid = (addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;                     
  
  for(int i = 0; i < len; i += read_bytes) {                            
    
    offset = current_addr % 256;                                        

    err = jbod_client_operation(package(0,diskid, JBOD_SEEK_TO_DISK), NULL);     
    assert (err == JBOD_NO_ERROR);

    err = jbod_client_operation(package(blockid,0, JBOD_SEEK_TO_BLOCK), NULL);    
    assert (err == JBOD_NO_ERROR);

    if (cache_lookup(diskid,blockid,tempbuf) == -1) {                   
      cache_insert(diskid,blockid,buf);                                 
      err = jbod_client_operation(package(0,0,JBOD_READ_BLOCK), tempbuf);      
      assert (err == JBOD_NO_ERROR);                                    
    } else {
      cache_update(diskid,blockid,buf);                               
      err = jbod_client_operation(package(0,0,JBOD_READ_BLOCK), tempbuf);        
      assert (err == JBOD_NO_ERROR);                                    
    }
    
    if(offset != 0){                                                    
      blockvalue++;                                                    
      if (((blockvalue*256) - offset) > len) {                          
        blank = ((blockvalue*256) - original_off) - len;                
        memcpy(tempbuf+offset, buf, JBOD_BLOCK_SIZE-offset-blank);      
        read_bytes = 256-offset-blank;                                  
      }else {                                                           
        memcpy(tempbuf+offset, buf, JBOD_BLOCK_SIZE-offset);            
        read_bytes = 256-offset;                                        
      }
    } else {
      blockvalue++;                                                     
      if((blockvalue*256)-original_off <= len) {                        
        memcpy(tempbuf, buf+i, JBOD_BLOCK_SIZE);                        
        read_bytes = JBOD_BLOCK_SIZE;                                   
      }else if ((blockvalue*256) -original_off > len) {                 
        blank = ((blockvalue*256) - original_off) - len;                
        memcpy(tempbuf, buf+i, JBOD_BLOCK_SIZE-blank);                  
        read_bytes = 256-blank;                                         
      }
    }

    err = jbod_client_operation(package(0,diskid, JBOD_SEEK_TO_DISK), NULL);     
    assert (err == JBOD_NO_ERROR);

    err = jbod_client_operation(package(blockid,0, JBOD_SEEK_TO_BLOCK), NULL);   
    assert (err == JBOD_NO_ERROR);

    err = jbod_client_operation(package(0,0,JBOD_WRITE_BLOCK), tempbuf);          
    assert (err == JBOD_NO_ERROR);

    current_addr += read_bytes;                                         
    diskid = current_addr/JBOD_DISK_SIZE;                               
    blockid = (current_addr%JBOD_DISK_SIZE)/ JBOD_BLOCK_SIZE;           

  }
  return len;
}