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
  char name[496];
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
  // TODO: Find the actual amoun this should be
  blocknum direct[20];
  // Pointer to an INDIRECT block that has pointers to DIRENT blocks
  blocknum single_indirect;
  // Pointer to an INDIRECT block that has pointers to INDIRECT blocks
  // that have pointers to DIRENT blocks
  blocknum double_indirect;
} dnode; // TODO make size 512 bytes

// Represents an indirect block (INDIRECT). Indirect blocks are simply 
// blocks that store more blocknum pointers to either DIRENT blocks, or
// other INDIRECT blocks. 
typedef struct indirect_t {
  blocknum blocks[128];
} indirect;

// Represents directory entry blocks (DIRENT). Directory entry blocks
// contain the contents of directories. These entries are statically
// allocated, so they must have a valid bit.  This is a single
// directory entry
typedef struct direntry_t {
  char name[27];
  char type;
  // The block number (block.valid is the valid bit)
  blocknum block;
} direntry; // TODO: Make size a power of 2 to easily fit in dirent

// Represents a DIRENT block, which consists of an array of direntrys
typedef struct dirent_t {
  // The contents of this directory
  direntry entries[16]; // TODO: Determine size of this array
} dirent;  // TODO: Make size 512 bytes

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
  blocknum direct[20];
  // Pointer to an INDIRECT block that has pointers to DB blocks
  blocknum single_indirect;
  // Pointer to an INDIRECT block that has pointers to INDIRECT blocks
  // that have pointers to DB blocks
  blocknum double_indirect;
} inode;

// Represents a data block (DB). These blocks contains only user data.
typedef struct db_t {
  char data[512];
} db;

// Represents a free block (FREE). Contains a pointer to the next
// free block.
typedef struct free_t {
blocknum next;
char junk[508];
} freeB;

// Find the blocknum of a given file
// if it does not exist, return a blocknum that is invalid
// Do we want to return where it lives in the DNode???
blocknum get_file(const char *path);

// TODO comment
blocknum get_inode_dirent(blocknum b, char *buf, const char *path);

// TODO comment
blocknum get_inode_single_indirect_dirent(blocknum b, char *buf, const char *path);

// TODO comment
blocknum get_inode_double_indirect_dirent(blocknum b, char *buf, const char *path);

// Initialize inode metadata to the given inode blocknum
void init_inode(blocknum b, char *buf, mode_t mode, struct fuse_file_info *fi);

// Initialize the given blocknum to an indirect
// returns 0 if there is an error in doing so
int create_indirect(blocknum b);

// Create a file at the next open direntry in this dirent
// returns 0 if there are no open direntries
int create_inode_dirent(blocknum dirent, blocknum inode, const char *path, char *buf);

// Create a file at the next open direntry in this single_indirect
// returns 0 if there are no open direntries
int create_inode_single_indirect_dirent(blocknum single_indirect, blocknum inode, const char *path, char *buf);

// Create a file at the next open direntry in this double_indirect
// returns 0 if there are no open direntries
int create_inode_double_indirect_dirent(blocknum double_indirect, blocknum inode, const char *path, char *buf);

// Reads the dnode at the given block number into buf
dnode get_dnode(unsigned int b, char *buf);

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


#endif
