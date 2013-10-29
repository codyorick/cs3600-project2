/*
 * CS3600, Fall 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__


#endif

// Our magic number!!!
#define MAGICNUMBER 48151623

// Block structures. These are copied from the spec

// VCB struct
// probably doesn't actually matter what size this is, just as long as it's <= 512
typedef struct vcb_s {
  // to identify the disk
  int magic;                    // 4
  
  int blocksize;                // 4
  int de_start;                 // 4
  int de_length;                // 4
  int fat_start;                // 4
  int fat_length;               // 4
  int db_start;                 // 4
  
  // metadata
  uid_t user;                   // 4
  gid_t group;                  // 4
  mode_t mode;                  // 4
  struct timespec access_time;  // 8
  struct timespec modify_time;  // 8
  struct timespec create_time;  // 8
} vcb;                          // 64 bytes

// Directory Entry struct
// we want 512 to be divisible by this eventually; for now, we'll make it 512
typedef struct dirent_s {
  unsigned int valid;                 // 4
  unsigned int first_block;           // 4
  unsigned int size;                  // 4
  uid_t user;                         // 4
  gid_t group;                        // 4
  mode_t mode;                        // 4
  struct timespec access_time;
  struct timespec modify_time;
  struct timespec create_time;
  char name[440];                     
} dirent;                             // 512 bytes

// FAT Entry struct
// These numbers are "bit fields" meaning this struct will be 32 bits (4 bytes)
// So, 128 of these can fit into a block (512/4 = 128)
typedef struct fatent_s {
  unsigned int used:1;
  unsigned int eof:1;
  unsigned int next:30;
} fatent;








