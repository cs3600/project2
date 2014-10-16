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

#define MAGICNUMBER 184901

// fix makefile so that these files can be defined and linked in 3600fs.h/.c
// TODO issue because of 2 main methods
void validate_structs_size();
void init_disk_layout(const int size);
int create_free(int bn, int last);
int create_vcb();
int create_root_dnode();
int create_root_dirent();


void myformat(int size) {
  // not enough size for minimal initialization
  // 3 BLOCKS at least: VCB, DNODE (root), DIRENT (root), 1 FREE
  // Exit with non-zero exit code
	if (size < 4) {
		fprintf(stderr, "File system must be at least 3 blocks.\n");
		exit(-1);
	}

  // Do not touch or move this function
  dcreate_connect();

  // check structures are appropriate size
  validate_structs_size();

  // initialize disk layout
	init_disk_layout(size);

  // Do not touch or move this function
  dunconnect();
}

//  Validate all structures are the appropriate size.
//  System will exit if any structure is invalid.
void validate_structs_size() {
	// blocknum should be 4 bytes
	assert(sizeof(blocknum) == 4);
	// vcb should be BLOCKSIZE bytes
	assert(sizeof(vcb) == BLOCKSIZE);
	// dnode should be BLOCKSIZE bytes
	printf("dnode size:%d\n", sizeof(dnode));
	assert(sizeof(dnode) == BLOCKSIZE);
	// indirect should be BLOCKSIZE bytes
	assert(sizeof(indirect) == BLOCKSIZE);
	// direntry should be BLOCKSIZE bytes
	assert(sizeof(direntry) == 32);
	// dirent should be BLOCKSIZE bytes
	assert(sizeof(dirent) == BLOCKSIZE);
	// inode should be BLOCKSIZE bytes
	printf("inode size:%d\n", sizeof(inode));
	assert(sizeof(inode) == BLOCKSIZE);
	// db should be BLOCKSIZE bytes
	assert(sizeof(db) == BLOCKSIZE);
	// free_b should be BLOCKSIZE bytes
	assert(sizeof(free_b) == BLOCKSIZE);
}

// Allocate the disk layout in the Inode file system format as
// follows:
//
//   BLOCK 0: VCB
//   BLOCK 1: DNODE (root)
//   BLOCK 2: DIRENT (1st root dirent)
//   BLOCK 3 -> BLOCK size: FREE
void init_disk_layout(const int size) {

	// temporary buffer
  char buf[BLOCKSIZE];
  memset(buf, 0, BLOCKSIZE);

	// Create the vcb on disk
  create_vcb();

  // Create the root dnode on disk
  create_root_dnode(); // TODO can abstract

  // Create the root dnode's first dirent on disk
  // This dirent houses 2 valid direntries initially: '.', and '..'
  create_root_dirent(); // TODO can abstract

  // Write out to all the free blocks in the disk
  for (int i = 0; i < size; i++) {
  	//FIXME can change i to 3, and size to size-1, tackle last free outside loop
  	// Checking each block to see if we can write to it on disk.
  	// Also zeroes out the disk, so there are no surprises.
  	// TODO do we need this? Doubles the amount of initial writes
    if (dwrite(i, buf) < 0) {
      perror("Error while writing to disk");
    }
    // Assign free blocks from Block 3 -> n - 1
    else if ((i > 2) && (i < (size - 1))) {
      // Create a free for a non-last block (0 == false)
      create_free(i, 0);
    }
    // Assign the last block as invalid
    else if (i == (size - 1) && (i > 2)) {
      // Create a free for the last block (1 == true)
      create_free(i, 1);
    } 
    // do nothing
    else {}
  }
}

// Create a free block at the given blocknum and have it point to the
// next free. If the blocknum is the last block, create a free block
// with an invalid next block. TODO: Error Handling here
int create_free(int bn, int last) {

  // Set it to point to next in cronological order, and valid
  blocknum b = { .block = (bn + 1), .valid = 1 };
  free_b temp_free;
  temp_free.next = b;
  
  // If it is the last block, make it invalid
  if (last) {
    b.valid = 0;
  }

  // write free block to disk
  char buf[BLOCKSIZE];
  memset(buf, 0, BLOCKSIZE);
  memcpy(buf, &temp_free, sizeof(free_b));
  dwrite(bn, buf);

  return 0;
}

// Create VCB, assign to Block 0
int create_vcb() {
  // Root blockNum
  blocknum root_dnode_block = { .block = 1, .valid = 1};

  // Create the first free block
  blocknum next_free = { .block = 3, .valid = 1};

  // Assign the proper variables to the this_vcb
  vcb this_vcb = { .magic = MAGICNUMBER, .blocksize = BLOCKSIZE, 
    .root = root_dnode_block, .free = next_free, .name = "JOBS"};

  // Allocate appropriate memory and copy over
  char tmp[BLOCKSIZE];
  memset(tmp, 0, BLOCKSIZE);
  memcpy(tmp, &this_vcb, sizeof(vcb));
  // Write to block 0
  dwrite(0, tmp);
 
  return 0;
}

// Create original Dnode, assign to Block 1
int create_root_dnode() { // TODO rename to root dnode
  // Make struct
  dnode root_dnode;
  root_dnode.size = 2; // initially 2 entries ".", and ".."
  root_dnode.user = getuid();			   // user's id
  root_dnode.group = getgid();           // user's group
  root_dnode.mode = (mode_t) 0777;       // set mode
  clock_gettime(CLOCK_REALTIME, &(root_dnode.access_time));  // access time
  root_dnode.direct[0].block = 2;        // assign first dirent to block 2
  root_dnode.direct[0].valid = 1;        // make it valid
  root_dnode.single_indirect.valid = 0;  // invalid
  root_dnode.double_indirect.valid = 0;  // invalid

  // invalidate all but the first dirent blocknums
  for (int i = 1; i < 110; i++) { // TODO WHY HARDCODED
  	root_dnode.direct[i].valid = 0;
	}
  // Allocate appropriate memory
  char buf[BLOCKSIZE];
  memset(buf, 0, BLOCKSIZE);
  memcpy(buf, &root_dnode, sizeof(dnode));
  // Write to block 1
  dwrite(1, buf);
  
  return 0;

}

// Create original Dirent, assign to Block 2
int create_root_dirent() {
  // Dirent that contains "." and ".."
  dirent dir;

  // set the "." and ".." values to true and point to block 1
  blocknum cur_block = { .block = 1, .valid = 1};

  // TODO: Where do these live?
  // type 1 refers to directory
  direntry current = { .name = ".", .type = '1', .block = cur_block};   // "."
  direntry parent = { .name = "..", .type = '1', .block = cur_block};   // ".."

  // invalidate direntries
  for (int i = 0; i < 16; i++) { // TODO WHY HARDCODED
  	dir.entries[i].block.valid = 0;
	}

  // set entries
  dir.entries[0] = current;
  dir.entries[1] = parent;
  
  // Allocate space for Dirent
  char buf[BLOCKSIZE];
  memset(buf, 0, BLOCKSIZE);
  memcpy(buf, &dir, sizeof(dirent));

  // Write to block 2
  dwrite(2, buf);

  return 0;
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
