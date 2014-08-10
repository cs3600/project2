/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__

/*
  Contains the underlying file system structure defintions. The
  structure definitions assume a blocksize of 512 bytes.
*/

// Represents block pointers. 
// Many of these pointers will be statically allocated, and therefore
// many of them will be invalid when they are first created.
typedef struct blocknum_t {
  int block:31;
  int valid:1;
} blocknum;

// Represents a volume control block (VCB). This is the first block in
// the file system, and contains metadata on the entire disk and the 
// information to find the 'root' directory DNODE.
typedef struct vcb_t {
  // A 'magic' number to identify your disk when mounting.
  int magic;
  // The size of a block
  int blocksize;
  // The location of the root DNODE block
  blocknum root;
  // The location of the first FREE block
  blocknum free;
  // disk name
  char name[496];
} vcb;

// Represents a directory node block (DNODE). This block contains the
// metadata for the directory (timestamps, owner, mode) as well as 
// pointers to the DIRENT blocks that actually contain the entries
// in the directory.
typedef struct dnode_t {
  // The number of entries in the directory
  unsigned int size;
  // The user id of who owns the directory
  uid_t user;
  // The group id of who owns the directory
  gid_t group;
  // The permissions associated with the directory
  mode_t mode;
  // The time the directory was last accessed
  struct timespec access_time;
  // The time the directory was last modified
  struct timespec modify_time;
  // The time the directory was created
  struct timespec create_time;
  // The locations of the directory entry blocks
  blocknum direct[...];
  // Pointer to an INDIRECT block that has pointers to DIRENT blocks (.?)
  blocknum single_indirect;
  // Pointer to an INDIRECT block that has pointers to INDIRECT blocks
  // that have pointers to DIRENT blocks (..?)
  blocknum double_indirect;
} dnode; // TODO check is struct size 512 bytes?

// Represents an indirect block (INDIRECT). Indirect blocks are simply 
// blocks that store more blocknum pointers to either DIRENT blocks, or
// other INDIRECT blocks. 
typedef struct indirect_t {
blocknum blocks[128];
} indirect;



#endif
