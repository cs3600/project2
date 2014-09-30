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
int createVcb();
int createDnode();
int createDirent();

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
  // TODO: Do something with allocating free blocks here?
  for (int i = 0; i < size; i++) 
    if (dwrite(i, tmpArr) < 0) 
      perror("Error while writing to disk");

  //*********** Probably want to put this all in a helper function ************
  // create the Vcb and write to Block 0
  createVcb();

  // Create the first Dnode and wirte to Block 1
  createDnode();

  // Dirent that contains "." and "..", create and assign to Block 2
  createDirent();

  // Do not touch or move this function
  dunconnect();
}


// Create VCB, assign to Block 0
int createVcb() {
  // Root blockNum
  blocknum vcbRoot = { .block = 1, .valid = 1};

  // Create the first free block (WHAT DOES VALID REFER TOO)
  blocknum vcbFree = { .block = 3, .valid = 1};

  // Assign the proper variables to the vcbBlock
  vcb vcbBlock = { .magic = MAGICNUMBER, .blocksize = BLOCKSIZE, 
    .root = vcbRoot, .free = vcbFree, .name = "Nonsense" };

  // Allocate approrpriate memory and copy over
  char tmp[BLOCKSIZE];
  memset(tmp, 0, BLOCKSIZE);
  memcpy(tmp, &vcbBlock, sizeof(vcb));

  // Write to block 0
  dwrite(0, tmp);
}

// Create original Dnode, assign to Block 1
int createDnode() {
  // Make struct
  dnode firDnode;
  firDnode.user = getuid();			   // user's id
  firDnode.group = getgid();           // user's group
  firDnode.mode = (mode_t) 0777;       // set mode
  clock_gettime(CLOCK_REALTIME, &(firDnode.access_time));  // access time
  firDnode.direct[0].block = 2;        // assign first dirent to block 2
  firDnode.direct[0].valid = 1;        // make it valid
  firDnode.single_indirect.valid = 0;  // invalid
  firDnode.double_indirect.valid = 0;  // invalid
  
  // Allocate appropriate memory
  char tmpDnode[BLOCKSIZE];
  memset(tmpDnode, 0, BLOCKSIZE);
  memcpy(tmpDnode, &firDnode, sizeof(dnode));

  // Write to block 1
  dwrite(1, tmpDnode);

}

// Create original Dirent, assign to Block 2
int createDirent() {
  // Dirent that contains "." and ".."
  dirent dir;

  // set the "." and ".." values to true and point to block 1
  blocknum curBlock = { .block = 1, .valid = 1};
  blocknum parBlock = { .block = 1, .valid = 1};

  // TODO: Where do these live?
  // type 1 refers to directory
  direntry current = { .name = ".", .type = '1', .block = curBlock};   // "."
  direntry parent = { .name = "..", .type = '1', .block = parBlock};   // ".."

  // set entries
  dir.entries[0] = current;
  dir.entries[1] = parent;
  
  // Allocate space for Dirent
  char tmpDirent[BLOCKSIZE];
  memset(tmpDirent, 0, BLOCKSIZE);
  memcpy(tmpDirent, &dir, sizeof(dirent));

  // Write to block 2
  dwrite(2, tmpDirent);
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
