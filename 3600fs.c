/*
 * CS3600, Fall 2013
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

// VCB, dirents and fatents stored when we are mounted
vcb myvcb;
dirent mydirents[100];
// fat entries will vary in number
fatent *myfatents;


// Finds the block number of the desired dirent of the file name, or -1 if not found
static int find_file(const char *path) {
  for(int i = 0; i < 100; i++) {
    if((strcmp(mydirents[i].name, path) == 0) && mydirents[i].valid)
      return i;
  }
  // if we get here, the file wasn't found
  return -1;
}

// Finds a free fat entry, or -1 if not found
static int find_fatent() {
  for(int i = 0; i < myvcb.fat_length * 128; i++) {
    if(myfatents[i].used == 0) {
      myfatents[i].used = 1;
      myfatents[i].eof = 1;
      return i;
    }
  }
  // if we get here, there are no more fatents
  return -1;
}

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
  fprintf(stderr, "vfs_mount called\n");

  // Do not touch or move this code; connects the disk
  dconnect();

  /* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
           AND LOAD ANY DATA STRUCTURES INTO MEMORY */
           
  // load the VCB
  char temp[BLOCKSIZE];
  memset(temp, 0, BLOCKSIZE);
  dread(0, temp); // load the vcb (at block 0) into temp
  memcpy(&myvcb, temp, sizeof(vcb)); // copy it to global variable
  if(myvcb.magic != MAGICNUMBER) {
    fprintf(stderr, "incorrect disk\n");
    exit(1);
  }
  // load all dirents into global dirent array
  for(int i = 1; i < 101; i++) {
    memset(temp, 0, BLOCKSIZE);
    dread(i, temp);
    memcpy(&mydirents[i-1], temp, sizeof(dirent));
  }
  
  // load all fatent blocks
  myfatents = malloc(myvcb.fat_length * BLOCKSIZE);
  for(int i = 0; i < myvcb.fat_length; i++) {
    memset(temp, 0, BLOCKSIZE);
    dread(myvcb.fat_start + i, temp);
    memcpy(myfatents + 128*i, temp, BLOCKSIZE); // since each fatent is 4 bytes, load 128 per block
  }

  return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
  fprintf(stderr, "vfs_unmount called\n");

  /* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
           ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
           KEEP DATA CACHED THAT'S NOT ON DISK */
  // write the dirents back to disk
  for(int i = 0; i < 100; i++) {
    char temp[BLOCKSIZE];
    memset(temp, 0, BLOCKSIZE);
    memcpy(temp, &mydirents[i], sizeof(dirent));
    dwrite(i+1, temp);
  }
  
  // write fatents back
  for(int i = 0; i < myvcb.fat_length; i++) {
    char temp[BLOCKSIZE];
    memset(temp, 0, BLOCKSIZE);
    memcpy(temp, myfatents + 128*i, BLOCKSIZE);
    dwrite(myvcb.fat_start + i, temp);
  }

  // Do not touch or move this code; unconnects the disk
  dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
  fprintf(stderr, "vfs_getattr called\n");

  // Do not mess with this code 
  stbuf->st_nlink = 1; // hard links
  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;

  /* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
  
  // if it's the root directory
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode  = 0777 | S_IFDIR;
    return 0;
  }
  
  // get the block number of the file
  int block = find_file(path);
  // if the file doesn't exist, return the required error
  if(block == -1)
    return -ENOENT;
  
  // otherwise, get the attributes
  stbuf->st_mode    = mydirents[block].mode | S_IFREG;
  stbuf->st_uid     = mydirents[block].user;
  stbuf->st_gid     = mydirents[block].group;
  stbuf->st_atime   = mydirents[block].access_time.tv_sec;
  stbuf->st_mtime   = mydirents[block].modify_time.tv_sec;
  stbuf->st_ctime   = mydirents[block].create_time.tv_sec;
  stbuf->st_size    = mydirents[block].size;
  stbuf->st_blocks  = mydirents[block].size / BLOCKSIZE;

  return 0;
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */
/*
 * NOTE: YOU CAN IGNORE THIS METHOD, UNLESS YOU ARE COMPLETING THE 
 *       EXTRA CREDIT PORTION OF THE PROJECT.  IF SO, YOU SHOULD
 *       UN-COMMENT THIS METHOD.
static int vfs_mkdir(const char *path, mode_t mode) {

  return -1;
} */

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  // make sure path is "/"
  if(strcmp(path, "/") != 0)
    return -1;
  // find the first dirent after the given offset
  int first = offset;
  for(first; first < 100; first++) {
    // call "filler" with arguments buf, filename, NULL, and next
    // if filler returns nonzero or if there are no more files, return 0
    if(mydirents[first].valid == 1) {
      if(!filler(buf, mydirents[first].name + 1, NULL, first+1))
        return 0;
    }
  }
  return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
  // make sure the file does not already exist
  for(int i = 0; i < 100; i++) {
    if(strcmp(mydirents[i].name, path) == 0)
      return -EEXIST;
  }
  
  // make sure we have room for a new file
  int empty = -1;
  for(int i = 0; i < 100; i++) {
    if(mydirents[i].valid == 0) {
      empty = i;
      break;
    }
  }
  // if there is no more room, return an error
  if(empty == -1)
    return -1;
    
  // otherwise, create the dirent
  mydirents[empty].valid = 1;
  mydirents[empty].user = geteuid();
  mydirents[empty].group = getegid();
  mydirents[empty].mode = mode;
  clock_gettime(CLOCK_REALTIME, &mydirents[empty].access_time);
  clock_gettime(CLOCK_REALTIME, &mydirents[empty].modify_time);
  clock_gettime(CLOCK_REALTIME, &mydirents[empty].create_time);
  strcpy(mydirents[empty].name, path);
  return 0;
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{
  // make sure the file exists/find dirent block
  int block = find_file(path);
  if(block == -1)
    return -1;
  
  // read "size" bytes from file "path", starting at "offset", into "buf"
  int bytes = 0;
  if(size + offset > mydirents[block].size) {
    bytes = mydirents[block].size - offset;
  }
  
  // just read one block
  if(offset % BLOCKSIZE + bytes <= BLOCKSIZE) {
    char temp[BLOCKSIZE];
    memset(temp, 0, BLOCKSIZE);
    dread(myvcb.db_start + mydirents[block].first_block, temp);
    memcpy(buf, temp + offset % BLOCKSIZE, bytes);
  }
  
  return bytes;
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{
  /* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
           MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */
  // make sure the file exists
  int block = find_file(path);
  if(block == -1)
    return -1;
    
  // write "size" bytes from "buf" into "path" starting at "offset"
  // if "size" is 0 for whatever reason, do nothing
  if(size == 0)
    return 0;
    
  // write small files
  int filesize = mydirents[block].size;
  if(filesize == 0 && size <= BLOCKSIZE) {
    char temp[BLOCKSIZE];
    memset(temp, 0, BLOCKSIZE);
    memcpy(temp + offset, buf, size);
    int startblock = find_fatent();
    if(startblock == -1)
      return -1;
    mydirents[block].first_block = startblock;
    mydirents[block].size = size + offset;
    dwrite(myvcb.db_start + startblock, temp);
    return size + offset; 
  }
  
  /*
  // how to do this: get the current size of the file, find how many blocks it
  // uses, find how many more blocks we will need to allocate based on the 
  // offset and the size, then allocate the blocks/fatents. then, actually 
  // write the data?
  int currblocks = (filesize + (BLOCKSIZE-1)) / BLOCKSIZE;
  // the new size of the file
  int newsize;
  if((offset + size) > filesize) {
    newsize = offset + size;
  } else {
    // we just overwrite existing data
    newsize = filesize;
  }
  int newblocks = (newsize + (BLOCKSIZE-1)) / BLOCKSIZE;
  int numnew = newblocks - currblocks; // number of new blocks we need
  // find the blocks we're using currently
  int fatnums[currblocks];
  if(currblocks > 0) {
    int startblock = mydirents[block].first_block;
    for(int i = 0; i < currblocks; i++) {
      
    }
  }
  */
  

  return 0;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{

  /* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
           AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */
    
    // find the file, mark the dirent as free
  for(int i = 0; i < 100; i++) {
    if(strcmp(mydirents[i].name, path) == 0 && mydirents[i].valid) {
      mydirents[i].valid = 0;
      mydirents[i].name[0] = '\0';
      int j = mydirents[i].first_block;
      myfatents[j].used = 0;
      return 0;
    }
  }
  // if we get here, the file wasn't found
  return -1;
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{
  // find the file, change the name
  for(int i = 0; i < 100; i++) {
    if(strcmp(mydirents[i].name, from) == 0 && mydirents[i].valid == 1) {
      strcpy(mydirents[i].name, to);
      return 0;
    }
  }
  
  // if we get here, the file was not found
  return -1;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{
  int block = find_file(file);
  if(block == -1)
    return -1;
  mydirents[block].mode = (mode & 0x0000ffff);
  return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
  int block = find_file(file);
  if(block == -1)
    return -1;
  mydirents[block].user = uid;
  mydirents[block].group = gid;
  return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
  int block = find_file(file);
  if(block == -1)
    return -1;
  mydirents[block].access_time = ts[0];
  mydirents[block].modify_time = ts[1];
  return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{

  /* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE. */

    return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
};

int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
      printf("Usage: ./3600fs -s -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}

