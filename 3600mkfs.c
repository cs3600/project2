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

#include "3600fs.h"
#include "disk.h"

void myformat(int size) {
  // Do not touch or move this function
  dcreate_connect();

  /* 3600: FILL IN CODE HERE.  YOU SHOULD INITIALIZE ANY ON-DISK
           STRUCTURES TO THEIR INITIAL VALUE, AS YOU ARE FORMATTING
           A BLANK DISK.  YOUR DISK SHOULD BE size BLOCKS IN SIZE. */

  //*********** Probably want to put this all in a helper function ************
  // TODO: Create structure for our disk size * BLOCKSIZE

  // TODO: Create VCB, assign to Block 0
  blocknum vcbRoot = { .block = 1, .valid = 1 };
  blocknum vcbFree = { .block = 3, .valid = 1 };

  vcb vcbBlock = { .magic = MAGICNUMBER, .blocksize = BLOCKSIZE, 
    .root = vcbRoot, .free = vcbFree, .name = "Nonsense" };

  char tmp[BLOCKSIZE];
  memset(tmp, 0, BLOCKSIZE);
  memcpy(tmp, &vcbBlock, sizeof(vcb));

  dwrite(0, tmp);

  
  // TODO: Create DNODE, assign to Block 1
  blocknum singleIndDNODE = { .block = 1, .valid = 0 };
  blocknum doubleIndDNODE = { .block = 1, .valid = 0 };

  // TODO: Iterate thorugh direct[] and assign invalid for eveything but 0
  blocknum directDNODE[];

  // TODO: Create DIRENT, assign to Block 2

  /* TODO: Assign rest of blocks as FREE, have their next point to next block;
           the last block's next should be invalid */         


  /* 3600: AN EXAMPLE OF READING/WRITING TO THE DISK IS BELOW - YOU'LL
           WANT TO REPLACE THE CODE BELOW WITH SOMETHING MEANINGFUL. */

  // first, create a zero-ed out array of memory  
  char *tmp = (char *) malloc(BLOCKSIZE);
  memset(tmp, 0, BLOCKSIZE);

  // now, write that to every block
  for (int i=0; i<size; i++) 
    if (dwrite(i, tmp) < 0) 
      perror("Error while writing to disk");

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
