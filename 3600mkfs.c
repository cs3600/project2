/*
 * CS3600, Spring 2014
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

const int MAGICNUMBER = 42;
//int clock_gettime(clockid_t, struct timespec *);

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  /* 3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
           STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
           A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

  /* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
           WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */

  // first, create a zero-ed out array of memory  
  char *tmpArr = (char *) malloc(BLOCKSIZE);
  memset(tmpArr, 0, BLOCKSIZE);

  // now, write that to every block
  for (int i = 0; i < size; i++) 
    if (dwrite(i, tmpArr) < 0) 
      perror("Error while writing to disk");
  //*********** Probably want to put this all in a helper function ************
  // TODO: Create structure for our disk size * BLOCKSIZE

  // TODO: Create VCB, assign to Block 0
  blocknum vcbRoot = { .block = 1, .valid = 1};

  // Create the first free block (WHAT DOES VALID REFER TOO)
  blocknum vcbFree = { .block = 3, .valid = 1};


  vcb vcbBlock = { .magic = MAGICNUMBER, .blocksize = BLOCKSIZE, 
    .root = vcbRoot, .free = vcbFree, .name = "Nonsense" };

  char tmp[BLOCKSIZE];
  memset(tmp, 0, BLOCKSIZE);
  memcpy(tmp, &vcbBlock, sizeof(vcb));

  dwrite(0, tmp);

  // Create the first Dnode
  dnode firDnode;
  firDnode.user = getuid();
  firDnode.group = getgid();
  firDnode.mode = (mode_t) 0777;
  if (clock_gettime(CLOCK_REALTIME, &(firDnode.access_time)) == 0) 
  {
    printf("This worked!\n"); 
  }
  firDnode.direct[0].block = 2;
  firDnode.direct[0].valid = 1; 
  firDnode.single_indirect.valid = 0;
  firDnode.double_indirect.valid = 0; 
  

  char tmpDnode[BLOCKSIZE];
  memset(tmpDnode, 0, BLOCKSIZE);
  memcpy(tmpDnode, &firDnode, sizeof(dnode));

  dwrite(1, tmpDnode);

  // Dirent that contains "." and ".."
  dirent dir;

  blocknum curBlock = { .block = 1, .valid = 1};
  blocknum parBlock = { .block = 1, .valid = 1};

  // TODO: Where do these live?
  direntry current = { .name = ".", .type = '1', .block = curBlock};   // "." type 1 is dir
  direntry parent = { .name = "..", .type = '1', .block = parBlock};   // ".."


  dir.entries[0] = current;
  dir.entries[1] = parent;
  
  char tmpDirent[BLOCKSIZE];
  memset(tmpDirent, 0, BLOCKSIZE);
  memcpy(tmpDirent, &dir, sizeof(dirent));

  dwrite(2, tmpDirent);

  /* TODO: Assign rest of blocks as FREE, have their next point to next block;
           the last block's next should be invalid */         




  // voila! we now have a disk containing all zeros

  // Do not touch or move this function
  dunconnect();
}

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
