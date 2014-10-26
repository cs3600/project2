/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 */

#ifndef __3600FS_H__
#define __3600FS_H__
#include "disk.h"

// Constant number of pointers in direct[] inode and dnode
#define NUM_DIRECT 110
// Constant copy of BLOCKSIZE
#define BLOCK_SIZE 512 
// Maximum length of a file name TODO make larger
#define MAX_FILENAME_LEN 59 
/*
  Contains the underlying file system structure defintions. The
  structure definitions assume a blocksize of BLOCKSIZE bytes.
*/

// Magic number for the disk formatting
extern const int MAGICNUMBER;

// Represents block pointers. 
// Many of these pointers will be statically allocated, and therefore
// many of them will be invalid when they are first created.
typedef struct blocknum_t {
  int block:31;
  unsigned int valid:1;
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
  char name[BLOCK_SIZE - (2 * sizeof(int)) - (2 * sizeof(blocknum))];
} vcb;

// Represents a directory node block (DNODE). This block contains the
// metadata for the directory (timestamps, owner, mode) as well as 
// pointers to the DIRENT blocks that actually contain the entries
// in the directory.
typedef struct dnode_t {
  // The number of entries in the directory
  // Use this to find where the the next open space is...
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
  blocknum direct[NUM_DIRECT];
  // Pointer to an INDIRECT block that has pointers to DIRENT blocks
  blocknum single_indirect;
  // Pointer to an INDIRECT block that has pointers to INDIRECT blocks
  // that have pointers to DIRENT blocks
  blocknum double_indirect;
} dnode;

// Represents an indirect block (INDIRECT). Indirect blocks are simply 
// blocks that store more blocknum pointers to either DIRENT blocks, or
// other INDIRECT blocks. 
typedef struct indirect_t {
  blocknum blocks[BLOCK_SIZE / sizeof(blocknum)];
} indirect;

// Represents directory entry blocks (DIRENT). Directory entry blocks
// contain the contents of directories. These entries are statically
// allocated, so they must have a valid bit.  This is a single
// directory entry
typedef struct direntry_t {
  char name[MAX_FILENAME_LEN];
  unsigned char type;
  // The block number (block.valid is the valid bit)
  blocknum block;
} direntry;

// Represents a DIRENT block, which consists of an array of direntrys
typedef struct dirent_t {
  // The contents of this directory
  direntry entries[BLOCK_SIZE / sizeof(direntry)];
} dirent;

// Represents a file inode block (INODE). This block contains file 
// metadata (timestamps, owner, mode) as well as pointers to the
// DB blocks that actually contain the data in the file.
typedef struct inode_t {
  // The number of entries in the file 
  unsigned int size;
  // The user id of who owns the file
  uid_t user;
  // The group id of who owns the file
  gid_t group;
  // The permissions associated with the file
  mode_t mode;
  // The time the file was last accessed
  struct timespec access_time;
  // The time the file was last modified
  struct timespec modify_time;
  // The time the file was created
  struct timespec create_time;
  // The locations of the data blocks
  blocknum direct[NUM_DIRECT];
  // Pointer to an INDIRECT block that has pointers to DB blocks
  blocknum single_indirect;
  // Pointer to an INDIRECT block that has pointers to INDIRECT blocks
  // that have pointers to DB blocks
  blocknum double_indirect;
} inode;

// Represents a data block (DB). These blocks contains only user data.
typedef struct db_t {
  char data[BLOCK_SIZE];
} db;

// Represents a free block (FREE). Contains a pointer to the next
// free block.
typedef struct free_t {
blocknum next;
char junk[BLOCK_SIZE - sizeof(blocknum)];
} free_b;

// Represents a file location. Will only live in memory, not disk.
// This structure contains the dirent blocknum where a file is
// located as well as the index of the specific direnty that the
// file is located. Also, the file's location with respect to
// direct, single_indirect, and double_indirect are contained here.
// For double_indirect file locations, the index of the indirect
// in which the dirent was located is stored in addition to the
// index of the dirent.
typedef struct file_loc_t { // add file short name to file_loc??
  // Is this a valid file location? TODO redundant? just check dirent_block.valid
  unsigned int valid:1;
  // Is this file located in the direct dirents?
  unsigned int direct:1;
  // Is this file located in the single indirect dirents?
  unsigned int single_indirect:1;
  // Is this file located in the double indirect dirents?
  unsigned int double_indirect:1;
  // The dirent blocknum where the file is located
  blocknum dirent_block;
  // The inode blocknum where the file is stored
  blocknum inode_block;
  // The index where the direntry for inode_block is located within 
  // dirent.entries.
  unsigned int direntry_idx;
  // The index where the dirent is located within in an array
  // of dirents. This applies to direct (an array of dirents),
  // single_indirect.blocks (an array of dirents), and the following
  // indirect: 
  // 
  //    dnode d;
  //    indirect i = d.double_indirect.blocks[double_indirect_idx];
  //    dirent dirent = i.blocks[list_idx];
  //    direntry direntry = dirent.entries[dirent_idx];
  unsigned int list_idx;
  // The index of the indirect where the dirent is located within the
  // double_indirect.
  unsigned int indirect_idx;
} file_loc;

// Returns the file_loc of the file specified by path.
// If the file is not in the file system, then the function returns an 
// invalid file_loc.
file_loc get_file(const char *path);

// Returns a file_loc to the file specified by path if it exists in the
// dirent specified by blocknum b. If b is not valid, an invalid file_loc
// is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to a dirent. The behavior if b is a valid blocknum to another
// structure type is undefined.
file_loc get_inode_dirent(blocknum b, char *buf, const char *path);

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within the thisDnode.direct array. If not found, an invalid
// file_loc is returned.
file_loc get_inode_direct_dirent(dnode *thisDnode, char *buf, const char *path);

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within the indirect specified by blocknum b. If b is not valid, 
// an invalid file_loc is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to an indirect of dirents. The behavior if b is a valid blocknum 
// to another structure type is undefined.
file_loc get_inode_single_indirect_dirent(blocknum b, char *buf, const char *path);

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within any indirect within the indirect specified by blocknum b.
// If b is not valid, an invalid file_loc is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to an indirect of indirects of dirents. The behavior if b is a 
// valid blocknum to another structure type is undefined.
file_loc get_inode_double_indirect_dirent(blocknum b, char *buf, const char *path);

// Initialize inode metadata to the given inode blocknum
int init_inode(blocknum b, char *buf, mode_t mode, struct fuse_file_info *fi);

// Initialize the given blocknum to an indirect
// returns 0 if there is an error in doing so
int create_indirect(blocknum b, char *buf);

// Initialize the given blocknum to a dirent
// returns 0 if there is an error in doing so
int create_dirent(blocknum b, char *buf);

// Create a file at the next open direntry in this dirent
// returns 0 if there are no open direntries
int create_inode_dirent(blocknum d, blocknum inode, const char *path, char *buf);

// Create a file at the next open direntry in the given direct array.
// Returns 0 if there is no space available for the new file in direct.
// Returns 1 on success.
// Returns -1 on error.
int create_inode_direct_dirent(dnode *dnode, blocknum inode, const char *path, char *buf); 

// Create a file at the next open direntry in this single_indirect
// returns 0 if there are no open direntries
// Returns 1 on success.
// Returns -1 on error.
int create_inode_single_indirect_dirent(blocknum s, blocknum inode, const char *path, char *buf);

// Create a file at the next open direntry in this double_indirect
// returns 0 if there are no open direntries
// Returns 1 on success.
// Returns -1 on error.
int create_inode_double_indirect_dirent(blocknum d, blocknum inode, const char *path, char *buf);

// Reads the dnode at the given block number into buf
dnode get_dnode(unsigned int b, char *buf);

// Write the given dnode (d) to disk at the given block (b)
int write_dnode(unsigned int b, char *buf, dnode d);

// Reads the inode at the given block number into buf
inode get_inode(unsigned int b, char *buf);

// Write the given inode (i) to disk at the given block (b)
int write_inode(unsigned int b, char *buf, inode i);

// Reads the db at the given block number into buf
db get_db(unsigned int b, char *buf);

// Write the given db (d) to disk at the given block (b)
int write_db(unsigned int b, char *buf, db d);


// Returns the next free block's blocknum
// If no more exist, returns a blocknum that is invalid
blocknum get_free();

// Reads the vcb at the given block number into buf
vcb get_vcb(char *buf);


// TODO: Multiple things have these structs.. can we abstract by passing a param???
// Access Single_indirect
blocknum get_single_block(int loc);
// Access Double_indirect
blocknum get_double_block(int loc);

// rename
void release_free(blocknum blocks[], int size);

#endif
