/*
 * CS3600, Fall 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This program is intended to format your disk file, and should be executed
 * BEFORE any attempt is made to mount your file system.  It will not, however
 * be called before every mount (you will call it manually when you format 
 * your disk file).
 */

#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "3600fs.h"
#include "disk.h"

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  /* 3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
           STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
           A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

  /* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
           WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */
           
  // We want (from spec):
  // 1 VCB
  // 100 dirents
  // fatents + data blocks = (size - 101)
  // # of fatents = (data blocks / 128)
  
  // Block 0: VCB
  // Blocks 1-100 = dirents
  // Blocks 101-rest = fat blocks + data blocks
  
  
  // Since fatents are 4 bytes, we can fit 128 of them in one block
  // BLOCKSIZE is defined in disk.c (which we are not to modify)
  // structs are defined in 3600fs.h
  
  // Create the VCB
  vcb myvcb;
  myvcb.magic = MAGICNUMBER;
  myvcb.blocksize = BLOCKSIZE;
  myvcb.de_start = 1; // starts after the VCB
  myvcb.de_length = 100; // we want 100 dirents
  int rest = size - 101;
  int numfat = (rest / 128) + 1; // add extra fat block since division will truncate, possibly resulting in insufficient fat blocks
  myvcb.fat_start = 101; // starts after VCB and dirents
  myvcb.fat_length = numfat;
  myvcb.db_start = myvcb.fat_start + myvcb.fat_length;
  myvcb.user = geteuid();
  myvcb.group = getegid();
  myvcb.mode = 0777;
  struct timespec currtime; // get the time for the time fields
  clock_gettime(CLOCK_REALTIME, &currtime);
  myvcb.access_time = currtime;
  myvcb.modify_time = currtime;
  myvcb.create_time = currtime;
  
  // Create an empty dirent
  dirent mydirent; 
  mydirent.valid = 0;
  mydirent.size = 0;
  // rest of the fields won't matter until the file is created
  
  // create empty fatent
  fatent myfatent;
  myfatent.used = 0;
  myfatent.eof = 0;

  // first, create a zero-ed out array of memory  
  char *tmp = (char *) malloc(BLOCKSIZE);
  memset(tmp, 0, BLOCKSIZE);
  // copy the VCB into this
  memcpy(tmp, &myvcb, sizeof(vcb));
  // write it to the disk
  dwrite(0, tmp);
  
  // write dirents to disk
  memset(tmp, 0, BLOCKSIZE);
  memcpy(tmp, &mydirent, sizeof(dirent));
  for(int i = 1; i < 101; i++) {
    dwrite(i, tmp);
  }
  
  // write fat blocks
  // first we want to create a block of just fatents
  memset(tmp, 0, BLOCKSIZE);
  for(int i = 0; i < BLOCKSIZE; i = i+4) {
    memcpy(tmp + i, &myfatent, sizeof(fatent));
  }
  // write that to the disk
  for(int i = myvcb.fat_start; i < (myvcb.fat_start + myvcb.fat_length); i++) {
    dwrite(i, tmp);
  }
  
  // write empty blocks for the data
  memset(tmp, 0, BLOCKSIZE);
  for(int i = myvcb.db_start; i < size; i++) {
    dwrite(i, tmp);
  }

  // voila! we now have a disk containing all zeros

  // Do not touch or move this function
  dunconnect();
}








// Don't touch stuff below this
/**********************************************************************/

int main(int argc, char** argv) {
  // Do not touch this function
  if (argc != 2) {
    printf("Invalid number of arguments \n");
    printf("usage: %s diskSizeInBlockSize\n", argv[0]);
    return 1;
  }

  unsigned long size = atoi(argv[1]);
  printf("Formatting the disk with size %lu \n", size);
  myformat(size);
}
