#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  int i = 0;
  int value = 0;
  while (i < len){
    value = read(fd,&buf[i],len-i);     //checking the amount read
    if (value <= 0) {                   //if fail to read return false
      return false;
    }
    i += value;                         
  }
  return true;
}


/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
bool nwrite(int fd, int len, uint8_t *buf) {
  int i = 0;
  int value = 0;
  while (i < len){
    value = write(fd,&buf[i],len-i);    //checking the amount written
    if (value <= 0) {                   //if fail to write return false
      return false;
    }
    i += value;                        
  }
  return true;
}


/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
bool recv_packet(int sd, uint32_t *op, uint8_t *ret, uint8_t *block) {
  uint8_t header[HEADER_LEN];                           
  int offset = 0;                                       
  if (nread(sd,HEADER_LEN,header) == false) return false;

  memcpy(op, header+offset, sizeof(*op));               
  offset += sizeof(*op);                                
  *op = ntohl(*op);                                     // connect network to host op

  memcpy(ret, header+offset, sizeof(*ret));             // copying header+offset into ret with size of ret
  offset += sizeof(*ret);                               

  if (*ret & 2) {                                       
    if (nread(sd,JBOD_BLOCK_SIZE,block) == false) {     
      return false;
    } 
  }
  return true;
} 

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t buffer[HEADER_LEN + JBOD_BLOCK_SIZE];              //create buffer with 261 byte in size
  int offset = 0;                                            
  uint32_t pack_code;                                        
  uint8_t code_info;                                          

  pack_code = htonl(op);                                     // store op code from server

  memcpy(buffer+offset, &pack_code, sizeof(pack_code));      // copying op code from server into buffer with size of op code
  offset += sizeof(pack_code);                              

  if ((op >> 12&0x3f) == JBOD_WRITE_BLOCK) {                 
    code_info = 2;                                            
    memcpy(buffer+offset, &code_info, sizeof(code_info));      
    offset += sizeof(code_info);                            

    memcpy(buffer+HEADER_LEN, block, JBOD_BLOCK_SIZE);      
    offset += JBOD_BLOCK_SIZE;                              

    if (nwrite(sd,HEADER_LEN + JBOD_BLOCK_SIZE, buffer) == false) return false;
     
  } else {
    code_info = 0;                                            
    memcpy(buffer+offset, &code_info, sizeof(code_info));      // copying code information to buffer
    offset += sizeof(code_info);                              

    if (nwrite(sd,HEADER_LEN, buffer) == false) return false;
  }
  
  return true;
}

/* connect to server and set the global client variable to the socket */
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in serveraddr;                         // create new socket struct

  cli_sd = socket(AF_INET, SOCK_STREAM, 0);              
  if (cli_sd == -1) {                                   
    return false;
  }

  serveraddr.sin_family = AF_INET;                       // assign network
  serveraddr.sin_port = htons(port);                     // assign port
  if(inet_aton(ip, &serveraddr.sin_addr) == 0) {         // establish network
    return false;
  }

  if (connect(cli_sd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) != 0) {  // connect to server
    return false;
  }

  return true;
}

void jbod_disconnect(void) {
  close(cli_sd);                                      
  cli_sd = -1;                                          
}

int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint8_t code_info;                                     
  
  if (cli_sd == -1) return -1;
  if (send_packet(cli_sd,op,block) == false)return -1;
  if (recv_packet(cli_sd,&op,&code_info,block) == false) return -1;
  if (code_info % 2 != 0) return -1;

  return 0;
}