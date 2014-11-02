/*
 * CS3600, Spring 2014
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
#include <sys/types.h>
#include <limits.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

// Cache size; supports for at most 1000 entries 
#define CACHE_SIZE 1000

// Magic number to identify our disk
const int MAGICNUMBER = 184901;
// Number of direntries per dirent
const int NUM_DIRENTRIES = BLOCK_SIZE / sizeof(direntry);
// Number of block in an indirect
const int NUM_INDIRECT_BLOCKS = BLOCK_SIZE / sizeof(blocknum);

// Cache of file_locs
cache_entry *cache[CACHE_SIZE];

// Get the file_loc specified by the given path
// If the file_loc returned is invalid, then the file
// is not in the cache.
static file_loc get_cache_entry(char *path) {
	// check each occupied cache entry for the path
  for (int i = 0; i < CACHE_SIZE; i++) {
    // cache hit
    if (!cache[i]->open && strcmp(cache[i]->path, path) == 0) {
      return cache[i]->loc;
		}
	}
	// cache miss
	file_loc loc;
	loc.valid = 0;
	return loc;
}

// Gets the next eviction entry (the oldest one).
static int next_evict() {
	long long oldest = ULLONG_MAX;
  for (int i = 0; i < CACHE_SIZE; i++) {
  	// update the oldest entry
    if (oldest > cache[i]->ts) {
    	oldest = cache[i]->ts;
		}
	}
	return oldest;
}

// Add the given file to the cache with the given file_loc
// If the file is already in cache, we replace that entry.
// Otherwise if there is no open cache entry, we evict the
// entry located at the next_cache_entry.
static void add_cache_entry(char *path, file_loc loc) {
  // create a new path copy
  int path_len = strlen(path) + 1;
  char * new_path = (char *) malloc(path_len);
  strncpy(new_path, path, path_len);

	for (int i = 0; i < CACHE_SIZE; i++) {
		// there is an open slot; add a new cache entry there
    if (cache[i]->open) {
    	// free old path
    	free(cache[i]->path);
    	// add new path
      cache[i]->path = new_path;
      // add loc; entry is now used
      cache[i]->loc = loc;
      cache[i]->open = 0;
    	return;
		}
	}
	// otherwise no open entry was found, replace the oldest
	int oldest = next_evict();
	cache[oldest]->path = new_path;
	cache[oldest]->loc = loc;
}

// Update the cache entry specified by the file from to
// the file specified by to.
static void update_cache_entry(char *from, char *to) {
	for (int i = 0; i < CACHE_SIZE; i++) {
		// we found the file from; change it to
    if (!cache[i]->open && strcmp(cache[i]->path, from) == 0) {
    	// free old path
    	free(cache[i]->path);
    	// add new path
    	int path_len = strlen(to) + 1;
    	char * new_path = (char *) malloc(path_len);
    	strncpy(new_path, to, path_len);
      cache[i]->path = new_path;
    	return;
		}
	}
}

// Remove the given file from the cache if it exists
static void remove_cache_entry(char *path) {
	for (int i = 0; i < CACHE_SIZE; i++) {
		// mark an entry that is not open and contains the path as open 
    if (!cache[i]->open && strcmp(cache[i]->path, path) == 0) {
      cache[i]->open = 1;
			return;
		}
	}
}

// Initialize the cache state. All entries are open.
static void init_cache() {
  for (int i = 0; i < CACHE_SIZE; i++) {
  	// open cache entry
	  cache_entry *ce = (cache_entry *) malloc(sizeof(cache_entry));
	  ce->path = NULL;
	  ce->open = 1;
	  ce->ts = 0;
	  // set each cache entry to open
    cache[i] = ce;
	}
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

  vcb myVcb;

  char *tmp = (char*) malloc(BLOCKSIZE);

  // Write to block 0
  dread(0, tmp);

  memcpy(&myVcb, tmp, sizeof(vcb));

	// wrong file system; disconnect
  if (myVcb.magic != MAGICNUMBER) {
    fprintf(stdout, "Attempt to mount wrong filesystem!\n");
    // disconnect
    dunconnect();
	}

	// initialize the file_loc cache
	init_cache();
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
  fprintf(stderr, "\nIN vfs_getattr\n");
	// Do not mess with this code 
  stbuf->st_nlink = 1; // hard links
  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;

  // create zeroed buffer
  char buf[BLOCKSIZE]; 
  memset(buf, 0, BLOCKSIZE);

	// get attr if exists
  file_loc parent = get_root_dir();
  file_loc loc = get_dir(path, path, &parent);

	// check if the file loc is a directory
  if (loc.valid && loc.is_dir) {
    
    // read in dnode
		dnode this_dnode = get_dnode(loc.node_block.block, buf);

    stbuf->st_mode  = this_dnode.mode | S_IFDIR;		

    // update stats
	  stbuf->st_uid     = this_dnode.user; // file uid
	  stbuf->st_gid     = this_dnode.group; // file gid
  	stbuf->st_atime   = this_dnode.access_time.tv_sec; // access time 
    stbuf->st_mtime   = this_dnode.modify_time.tv_sec; // modify time
    stbuf->st_ctime   = this_dnode.create_time.tv_sec; // create time
	  stbuf->st_size    = this_dnode.size; // file size
	  stbuf->st_blocks  = this_dnode.size / BLOCKSIZE; // file size in blocks
	  return 0;
  }
	// we have a regular file
  else if (loc.valid) {
		// read in inode
  	inode this_inode; 
  	dread(loc.node_block.block, buf);
  	memcpy(&this_inode, buf, sizeof(inode));

    stbuf->st_mode  = this_inode.mode | S_IFREG;

    // update stats
  	stbuf->st_uid     = this_inode.user; // file uid
	  stbuf->st_gid     = this_inode.group; // file gid
    stbuf->st_atime   = this_inode.access_time.tv_sec; // access time 
  	stbuf->st_mtime   = this_inode.modify_time.tv_sec; // modify time
	  stbuf->st_ctime   = this_inode.create_time.tv_sec; // create time
    stbuf->st_size    = this_inode.size; // file size
  	stbuf->st_blocks  = this_inode.size / BLOCKSIZE; // file size in blocks
  	return 0;
	}
	// otherwise file not found
	else {
		return -ENOENT;
	}
}

/*
 * Given an absolute path to a directory (which may or may not end in
 * '/'), vfs_mkdir will create a new directory named dirname in that
 * directory, and will create it with the specified initial mode.
 *
 * HINT: Don't forget to create . and .. while creating a
 * directory.
 */
static int vfs_mkdir(const char *path, mode_t mode) {

  fprintf(stderr, "\nIN vfs_mkdir\n");

  // Allocate appropriate memory 
  char buf[BLOCKSIZE];
  // Get the dnode
  dnode thisDnode = get_dnode(1, buf);

  blocknum file;
  file_loc loc;
  // blocknum of Inode of file
	file_loc parent = get_root_dir();
  loc = get_dir(path, path, &parent);

  // See if the file exists
  if (loc.valid) {
    // File exists...
    return -EEXIST;
  }
    
  // Get next free block
  blocknum this_dnode = get_free();
    
  // Check that the free block is valid
  if (this_dnode.valid) {
    // Set up Inode; if not 1, the inode was not set
    if (create_dnode(this_dnode, buf, mode) != 1)
      return -1;

    // Loop through dnode direct
    if (create_node_direct_dirent(&thisDnode, this_dnode, loc.name, 1, buf) == 1) {
			return 0;
		}

    // Loop through dnode -> single_indirect
    if (create_node_single_indirect_dirent(thisDnode.single_indirect, this_dnode, loc.name, 1, buf) == 1) {
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &thisDnode, sizeof(dnode));
      dwrite(1, buf);
      return 0;
    }
    // Loop through dnode -> double_indirect
    else if (create_node_double_indirect_dirent(thisDnode.double_indirect, this_dnode, loc.name, 1, buf) == 1) {
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &thisDnode, sizeof(dnode));
      dwrite(1, buf);
      return 0;
    }
    // No direntries available 
    else {
      return -1;
    }
  }

  // TODO: Out of memory message?
  return -1;
}

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
  fprintf(stderr, "\nIN vfs_readdir\n");

  // get the file_loc
  file_loc parent = get_root_dir();
  file_loc loc = get_dir(path, path, &parent);

  // we know it's the root dir
  char tmp_buf[BLOCKSIZE];
  dnode this_dnode = get_dnode(loc.node_block.block, tmp_buf);
  // list the direct entries of the dnode
  list_entries(this_dnode.direct, NUM_DIRECT, filler, buf);

  // list the single indirect entries
  list_single(this_dnode.single_indirect, filler, buf);
  // list the double indirect entries of the dnode
  list_double(this_dnode.double_indirect, filler, buf);

	return 0;
}

// list entries in the double indirect if there are any
void list_double(blocknum d, fuse_fill_dir_t filler, void *buf) {
	// check that there are single indirect entries
  if (d.valid) {
  	// get the double indirect entries
    indirect dub = get_indirect(d);
    // list entries for each indirect entry
    for (int j = 0; j < NUM_INDIRECT_BLOCKS; j++) {
      // list the entries of the ith indirect if there is data there
      if (dub.blocks[j].valid) {
        indirect i = get_indirect(dub.blocks[j]);
  	    list_entries(i.blocks, NUM_INDIRECT_BLOCKS, filler, buf);
      }
		}
	}
}

// list entries in the single indirect if there are any
void list_single(blocknum s, fuse_fill_dir_t filler, void *buf) {
	// check that there are single indirect entries
  if (s.valid) {
  	// get the indirect
    indirect i = get_indirect(s);
  	// list the entries
  	list_entries(i.blocks, NUM_INDIRECT_BLOCKS, filler, buf);
	}
}

indirect get_indirect(blocknum b) {
  char tmp_buf[BLOCKSIZE];
  memset(tmp_buf, 0, BLOCKSIZE); 	
  // read indirect from disk
  indirect i;
  dread(b.block, tmp_buf);
  memcpy(&i, tmp_buf, sizeof(indirect));

  return i;
}

// List entries in the given array of dirent blocknums
void list_entries(blocknum d[], size_t size, fuse_fill_dir_t filler, void *buf) {
  // temporary buffer
  char tmp_buf[BLOCKSIZE];

  // iterate over all dirents in d
  for (int i = 0; i < size; i++) {
  	// load dirent from disk into memory
  	blocknum dirent_b = d[i];
  	// read only valid dirents
  	if (dirent_b.valid) {
      dirent de;
      memset(tmp_buf, 0, BLOCKSIZE);
      dread(dirent_b.block, tmp_buf);
      memcpy(&de, tmp_buf, sizeof(dirent));
      // iterate over all direnties in dirent_b
      for (int j = 0; j < NUM_DIRENTRIES; j++) {
        // get the jth entry and load into a stat struct
        direntry dentry = de.entries[j];
        // read only valid direntries
        if (dentry.block.valid) {
          struct stat st;
          memset(&st, 0, sizeof(st));
          // set inode block number
          st.st_ino = dentry.block.block;
          // set the mode
          st.st_mode = dentry.type;
          // fill in the filler
          char *name = dentry.name;
        	// the filler is non 0 when buf is filled
          if (filler(buf, name, &st, 0) != 0) {
            break;
				  }
			  }
      }
    }
  }
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

  fprintf(stderr, "\nIN vfs_create\n");

  // Allocate appropriate memory 
  char buf[BLOCKSIZE];

  blocknum file;
  file_loc loc;
  // blocknum of Inode of file
	file_loc parent = get_root_dir();
  loc = get_dir(path, path, &parent);

  // dnode where we want to write file to 
  dnode thisDnode = get_dnode(parent.node_block.block, buf);

  // See if the file exists
  if (loc.valid) {
    // File exists...
    return -EEXIST;
  }
    
  // Get next free block
  blocknum this_inode = get_free();
    
  // Check that the free block is valid
  if (this_inode.valid) {
    // Set up Inode; if not 1, the inode was not set
    if (create_inode(this_inode, buf, mode) != 1)
      return -1;

    // Loop through dnode direct
    if (create_node_direct_dirent(&thisDnode, this_inode, loc.name, 0, buf) == 1) {
			return 0;
		}

    // Loop through dnode -> single_indirect
    if (create_node_single_indirect_dirent(thisDnode.single_indirect, this_inode, loc.name, 0, buf) == 1) {
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &thisDnode, sizeof(dnode));
      dwrite(1, buf);
      return 0;
    }
    // Loop through dnode -> double_indirect
    else if (create_node_double_indirect_dirent(thisDnode.double_indirect, this_inode, loc.name, 0, buf) == 1) {
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &thisDnode, sizeof(dnode));
      dwrite(1, buf);
      return 0;
    }
    // No direntries available 
    else {
      return -1;
    }
  }

  // TODO: Out of memory message?
  return -1;
}

// Initialize dnode metadata to the given dnode blocknum.
// Returns 0 if the block b is not valid.
// buf should be BLOCKSIZE bytes.
int create_dnode(blocknum b, char *buf, mode_t mode) {
  // make sure the block is valid 
  if (b.valid) {
    dnode new_node;
    // set file metadata
    new_node.size = 0;
    new_node.user = getuid();
    new_node.group = getgid();
    new_node.mode = mode;
    clockid_t now = CLOCK_REALTIME;
    clock_gettime(now, &new_node.access_time);
    clock_gettime(now, &new_node.modify_time);
    clock_gettime(now, &new_node.create_time);
    // Invalidate all direct blocks 
    blocknum invalid;
    invalid.valid = 0;
    invalid.block = 0;
    for (int i = 0; i < NUM_DIRECT; i++) {
    	new_node.direct[i] = invalid;
		}
    // invalidate the single and double indirects
    new_node.single_indirect.valid = 0;
    new_node.double_indirect.valid = 0;
    // zero out the buffer and write inode to block b
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &new_node, sizeof(inode));
    dwrite(b.block,  buf);
    return 1;
  }
  // the block is not valid
  return 0;
}

// Initialize inode metadata to the given inode blocknum.
// Returns 0 if the block b is not valid.
// buf should be BLOCKSIZE bytes.
int create_inode(blocknum b, char *buf, mode_t mode) {
  // make sure the block is valid 
  if (b.valid) {
    inode new_node;
    // set file metadata
    new_node.size = 0;
    new_node.user = getuid();
    new_node.group = getgid();
    new_node.mode = mode;
    clockid_t now = CLOCK_REALTIME;
    clock_gettime(now, &new_node.access_time);
    clock_gettime(now, &new_node.modify_time);
    clock_gettime(now, &new_node.create_time);
    // Invalidate all direct blocks 
    blocknum invalid;
    invalid.valid = 0;
    invalid.block = 0;
    for (int i = 0; i < NUM_DIRECT; i++) {
    	new_node.direct[i] = invalid;
		}
    // invalidate the single and double indirects
    new_node.single_indirect.valid = 0;
    new_node.double_indirect.valid = 0;
    // zero out the buffer and write inode to block b
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &new_node, sizeof(inode));
    dwrite(b.block,  buf);
    return 1;
  }
  // the block is not valid
  return 0;
}

// Initialize the given blocknum to a dirent
// returns 0 if there is an error in doing so
int create_dirent(blocknum b, char *buf) {
  // make sure the given block is valid
  if (b.valid) {
    // put an indirect structure in the given block
    memset(buf, 0, BLOCKSIZE);
    dirent new_dirent;
    // invalidate all blocks
    for (int i = 0; i < NUM_DIRENTRIES; i++) {
      new_dirent.entries[i].block.valid = 0;
    }
    memcpy(buf, &new_dirent, sizeof(dirent));
    dwrite(b.block, buf);
    return 1;
  }
  // Block was not valid, error
  return 0;
}

// Initialize the given blocknum to an indirect
// returns 0 if there is an error
int create_indirect(blocknum b, char *buf) {
	// make sure the given block is valid
  if (b.valid) {
    // put an indirect structure in the given block
    memset(buf, 0, BLOCKSIZE);
    indirect new_indirect;
    // invalidate all blocks
    for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
      new_indirect.blocks[i].valid = 0;
    }
    memcpy(buf, &new_indirect, sizeof(indirect));
    dwrite(b.block, buf);
    return 1;
  }
  // Block was not valid, error
  return 0;
}

// Create a file at the next open direntry in this dirent
// returns 0 if there are no open direntries
// Returns 1 on success.
// Returns -1 on error.
// the passed in buf is meant as a reusable buf so we can save memory.
int create_node_dirent(blocknum d, blocknum node, const char *name, unsigned int type, char *buf) {
  // this must now be valid
  d.valid = 1;

  // get the dirent object
  memset(buf, 0, BLOCKSIZE);
  dread(d.block, buf);
  dirent dir;
  memcpy(&dir, buf, BLOCKSIZE);
    
  // look through each of the direntries
  for (int i = 0; i < NUM_DIRENTRIES; i++) {
    // add at the ith direntry
    if (!dir.entries[i].block.valid) {
      // create a new direntry for the inode
      dir.entries[i].block = node;
      // add the name
			strncpy(dir.entries[i].name, name, MAX_FILENAME_LEN - 1);
      dir.entries[i].name[MAX_FILENAME_LEN - 1] = '/0';
      // set the type to a file
      dir.entries[i].type = type;

      // write to dirent to disk
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &dir, BLOCKSIZE);
      dwrite(d.block, buf);

      return 1;
    }
  }

  return 0;
}

// Create a file at the next open direntry in the given direct array.
// Returns 0 if there is no space available for the new file in direct.
// Returns 1 on success.
// Returns -1 on error.
// the passed in buf is meant as a reusable buf so we can save memory.
int create_node_direct_dirent(dnode *thisDnode, blocknum node, const char *name, unsigned int type, char *buf) {

  // Look for available direntry location to put this new file
  // Loop through dnode -> direct
  for (int i = 0; i < NUM_DIRECT; i++) {
    // check if valid, if not get one, assign to i, call function
    if (!(thisDnode->direct[i].valid)) {
      // create a dirent for the ith block
      // get the next free block
      blocknum temp_free = get_free();
      // try to create a dirent
      if (!(create_dirent(temp_free, buf))) {
        // error with create_indirect
        return -1;
      } 
      // otherwise we were able to create a dirent, set that to the ith block
      thisDnode->direct[i] = temp_free;
    }

    // dirent is valid, look through direntries for next open slot
    if (create_node_dirent(thisDnode->direct[i], node, name, type, buf)) {
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, thisDnode, sizeof(dnode));
      dwrite(1, buf);			
      return 1;
    } 
  }

	// No space in direct for new file
  return 0;
}

// Create a file at the next open direntry in this single_indirect
// returns 0 if there are no open direntries
// Returns 1 on success.
// Returns -1 on error.
// the passed in buf is meant as a reusable buf so we can save memory.
int create_node_single_indirect_dirent(blocknum s, blocknum node, const char *name, unsigned int type, char *buf) {
  // All other blocks free, now this must be valid
  s.valid = 1;

  memset(buf, 0, BLOCKSIZE);
  dread(s.block, buf);
  indirect single_indirect;
  memcpy(&single_indirect, buf, BLOCKSIZE);

  for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
    // check if valid, if not get one, assign to i, call function
    if (!(single_indirect.blocks[i].valid)) {
      // create a single indirect for the ith block
      // get the next free block
      blocknum temp_free = get_free();
      // try to create an indirect
      if (!(create_indirect(temp_free, buf))) {
        // error with create_indirect
        return -1;
      } 
      // otherwise we were able to create an indirect, set that to the ith block
      single_indirect.blocks[i] = temp_free;
    }

    // try to create a block at i
    if (create_node_dirent(single_indirect.blocks[i], node, name, type, buf)) {
      return 1;
    }
  }
  // No space available
  return 0;
}

// Create a file at the next open direntry in this double_indirect
// returns 0 if there are no open direntries
// Returns 1 on success.
// Returns -1 on error.
// the passed in buf is meant as a reusable buf so we can save memory.
int create_node_double_indirect_dirent(blocknum d, blocknum node, const char *name, unsigned int type, char *buf) {
  // All other blocks free, now this must be valid
  d.valid = 1;

  memset(buf, 0, BLOCKSIZE);
  dread(d.block, buf);
  indirect double_indirect;
  memcpy(&double_indirect, buf, BLOCKSIZE);

  for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
    // check if valid, if not get one, assign to i, call function
    if (!(double_indirect.blocks[i].valid)) {
      // create a single indirect for the ith block
      // get the next free block
      blocknum temp_free = get_free();
      // try to create an indirect
      if (!(create_indirect(temp_free, buf))) {
        // error with create_indirect
        return -1;
      } 
      // otherwise we were able to create an indirect, set that to the ith block
      double_indirect.blocks[i] = temp_free;
    }

    // try to create a block at i
    if (create_node_single_indirect_dirent(double_indirect.blocks[i], node, name, type, buf)) {
      return 1;
    }
  }
  // No space available
  return 0;
}


// Reads the vcb at the given block number into buf
vcb get_vcb(char *buf) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Read Vcb
  dread(0, buf);
  // Put the read data into a Dnode struct
  vcb this_vcb;
  memcpy(&this_vcb, buf, sizeof(vcb));
  return this_vcb;
}


// Returns the file_loc of the given path's Inode form the parent.
// If the file specified by the path does not have an associated Inode,
// then the function returns an invalid file_loc.
file_loc get_file(char *abs_path, char *path, file_loc parent) {
  // Start at the Dnode and search the direct, single_indirect,
  // and double_indirect

  // general-purpose temporary buffer
  char *buf = (char *) malloc(BLOCKSIZE);

  // temporary file_loc buffer
  file_loc loc; 
  // check the cache for our file
  loc = get_cache_entry(abs_path);
  if (loc.valid) {
    fprintf(stderr, "Cache Hit: %s\n", abs_path);
    return loc;
	}

  // Read data into a Dnode struct
  // TODO when implementing multi-directory, this will change
  dnode thisDnode = get_dnode(parent.node_block.block, buf);

	// look at the right snippet of the path
	char *file_path = path;
	unsigned int name_len = 1;
	while (*file_path != '\0' && *file_path != '/') {
		name_len++;
		file_path++;
	}
	char name[name_len];
	strncpy(name, path, name_len);
	name[name_len - 1] = '\0';

	// Check each dirent in direct to see if the file specified by
	// path exists in the filesystem.
	loc = get_inode_direct_dirent(&thisDnode, buf, name);

	// If valid return the value of the file_loc; update the cache
	if (loc.valid) {
		free(buf);
		add_cache_entry(abs_path, loc);
		return loc;
	}

  // Check each dirent in the single_indirect to see if the file specified by
  // path exists in the filesystem.
  loc = get_inode_single_indirect_dirent(thisDnode.single_indirect, buf, name);

	// If valid return the value of the file_loc; update the cache
  if (loc.valid) {
    free(buf);
    // set the direct, single_indirect, and double_indirect flags
    loc.direct = 0;
    loc.single_indirect = 1;
    loc.double_indirect = 0;
		add_cache_entry(abs_path, loc);
    return loc;
  }

	// Check each dirent in each indirect in the double_indirect to see if the
  // file specified by path exists in the filesystem.
  loc = get_inode_double_indirect_dirent(thisDnode.double_indirect, buf, name);
	// If valid return the value of the file_loc; update the cache
  if (loc.valid) {
    free(buf);
    // set the direct, single_indirect, and double_indirect flags
    loc.direct = 0;
    loc.single_indirect = 0;
    loc.double_indirect = 1;
		add_cache_entry(abs_path, loc);
    return loc;
  }

  // The file specified by path does not exist in our file system.
  // Return invalid file_loc with the short name.
  free(buf);
  strncpy(loc.name, name, name_len);
  return loc;
}

// Get the root file_loc.
file_loc get_root_dir() {
	// the root block
	blocknum b;
	b.valid = 1;
	b.block = 1;
	// root file_loc
	file_loc root;
	root.node_block = b;
	root.is_dir = 1;
	root.valid = 1;
	return root;
}

// get the directory specified by path in the parent directory.
// The abs path is what the cache uses to perform lookups.
file_loc get_dir(char *abs_path, char *path, file_loc *parent) {
	// for the annoying case of when / is its own parent
	if (strcmp(path, "/") == 0) {
		return *parent;
	}

	// a pointer to walk the string
	char *p = path;

	// remove leading /
	if (*p == '/') {
		p++;
	}

	// the start address of the first token
	char *start = p;
  // the length of a sub string delimited by /; '/0'
	int child_len = 1;
	// get the next token
  while (*p != '/' && *p != '\0') {
	  p++;
	  child_len++;
  }

  // set the child path
  char child_path[child_len];
  strncpy(child_path, start, child_len);
  child_path[child_len - 1] = '\0';

  // check if the child exists
  file_loc child_loc = get_file(abs_path, child_path, *parent);

	// file not found return the invalid file loc
  // if the child is a file return it
  if (!child_loc.valid || !child_loc.is_dir) {
  	return child_loc;
	}
	// if the child is a dir, recur if there more
  else if (*p != '\0') {
  	parent->node_block = child_loc.node_block;
  	return get_dir(abs_path, p, parent);
	}
	// otherwise return the directory file_loc
	else {
		return child_loc;
	}
}

// Reads the dnode at the given block number into buf
dnode get_dnode(unsigned int b, char *buf) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Read Dnode
  dread(b, buf);
  // Put the read data into a dnode struct
  dnode this_dnode;
  memcpy(&this_dnode, buf, sizeof(dnode));
  return this_dnode;
}

// Write the given dnode (d) to disk at the given block (b)
int write_dnode(unsigned int b, char *buf, dnode d) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Put the dnode into the buf
  memcpy(buf, &d, sizeof(dnode));
  // Write dnode data, return 1 if successful
  if (dwrite(b, buf) < 0) {
    return 1;
  }
  // Error in write
  return 0;
}

// Reads the inode at the given block number
inode get_inode(unsigned int b) {
  char buf[BLOCKSIZE];
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Read Dnode
  dread(b, buf);
  // Put the read data into a inode struct
  inode this_inode;
  memcpy(&this_inode, buf, sizeof(inode));
  return this_inode;
}

// Write the given inode (i) to disk at the given block (b)
int write_inode(unsigned int b, char *buf, inode i) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Put the inode into the buf
  memcpy(buf, &i, sizeof(inode));
  // Write inode data, return 1 if successful
  if (dwrite(b, buf) < 0) {
    return 1;
  }
  // Error in write
  return 0;
}

// Reads the db at the given block number into buf
db get_db(unsigned int b, char *buf) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Read Dnode
  dread(b, buf);
  // Put the read data into a db struct
  db this_db;
  memcpy(&this_db, buf, sizeof(db));
  return this_db;
}

// Write the given db (d) to disk at the given block (b)
int write_db(unsigned int b, char *buf, db d) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Put the db into the buf
  memcpy(buf, &d, sizeof(db));
  // Write db data, return 1 if successful
  if (dwrite(b, buf) < 0) {
    return 1;
  }
  // Error in write
  return 0;
}

// Reads the indirect at the given block number into buf
indirect get_indirect2(unsigned int b, char *buf) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Read indirect
  dread(b, buf);
  // Put the read data into a indirect struct
  indirect this_indirect;
  memcpy(&this_indirect, buf, sizeof(indirect));
  return this_indirect;
}

// Write the given indirect (i) to disk at the given block (b)
int write_indirect(unsigned int b, char *buf, indirect i) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Put the indirect into the buf
  memcpy(buf, &i, sizeof(indirect));
  // Write indirect data, return 1 if successful
  if (dwrite(b, buf) < 0) {
    return 1;
  }
  // Error in write
  return 0;
}



// Returns the next free block's blocknum
// If no more exist, returns a blocknum that is invalid
blocknum get_free() {
  // temporary buffer for Vcb
  char buf[BLOCKSIZE];
  vcb thisVcb = get_vcb(buf);

  // Do we have a free block
  if (thisVcb.free.valid) {
    // Get the free block structure
    memset(buf, 0, BLOCKSIZE);
    dread(thisVcb.free.block, buf);
    free_b tmpFree;
    memcpy(&tmpFree, buf, BLOCKSIZE);

    // capture free block
    blocknum next_free = thisVcb.free;   
    // Update the next free block in Vcb   
    thisVcb.free = tmpFree.next; 
    
    // Write the adjusted vcb to disk
		memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &thisVcb, BLOCKSIZE);
    dwrite(0, buf);

    return next_free;
  }

  // return an invalid blocknum
  return thisVcb.free;   
}

// Returns a file_loc to the file specified by path if it exists in the
// dirent specified by blocknum b. If b is not valid, an invalid file_loc
// is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to a dirent. The behavior if b is a valid blocknum to another
// structure type is undefined.
file_loc get_inode_dirent(blocknum b, char *buf, const char *path) {

  // the file_loc to return
  file_loc loc;

	// check that the dirent blocknum is valid
	if (b.valid) {
    // read dirent into memory from disk
    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    dirent tmp_dirent;
    memcpy(&tmp_dirent, buf, BLOCKSIZE);
    // check each direntry for the file specified by path
    for (int i = 0; i < NUM_DIRENTRIES; i++) {
      // valid direntry
      if (tmp_dirent.entries[i].block.valid) {
        // the file exists, update file_loc result and return it
        if (strcmp(path, tmp_dirent.entries[i].name) == 0) {
          loc.valid = 1;
          loc.dirent_block = b;
          loc.node_block = tmp_dirent.entries[i].block;
          loc.direntry_idx = i;
          loc.is_dir = tmp_dirent.entries[i].type;
          strncpy(loc.name, path, MAX_FILENAME_LEN);
          return loc;
        }
      }
    }
  }
  // file not found, return invalid file_loc
  loc.valid = 0;
  return loc;
}

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within the thisDnode.direct array. If not found, an invalid
// file_loc is returned.
file_loc get_inode_direct_dirent(dnode *thisDnode, char *buf, const char *path) {

  // the file_loc to return
  file_loc loc;

  // Check direct dirents to see if the file specified by path
  // exists in the filesystem.
  for (int i = 0; i < NUM_DIRECT; i++) {
    loc = get_inode_dirent(thisDnode->direct[i], buf, path);

    // If valid return the value of the file_loc
    if (loc.valid) {
      // set the direct, single_indirect, and double_indirect flags
      loc.direct = 1;
      loc.single_indirect = 0;
      loc.double_indirect = 0;
      // set the index of where the file was located in the direct array
      loc.list_idx = i;
      return loc;
    }
  }

  // file not found, return invalid file_loc
  loc.valid = 0;
  return loc;
}

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within the indirect specified by blocknum b. If b is not valid, 
// an invalid file_loc is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to an indirect of dirents. The behavior if b is a valid blocknum 
// to another structure type is undefined.
file_loc get_inode_single_indirect_dirent(blocknum b, char *buf, const char *path) {

  // the file_loc to return
  file_loc loc;

  // check that the indirect blocknum is valid
  if (b.valid) {
    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    indirect single_indirect;
    memcpy(&single_indirect, buf, BLOCKSIZE);
    // check each dirent for the file specified by path
    for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
      loc = get_inode_dirent(single_indirect.blocks[i], buf, path);
      // will only be valid if file location exists in ith dirent
      if (loc.valid) {
        // set the index of the dirent of where the file was located in the
        // single_indirect.blocks array
        loc.list_idx = i;
        return loc;
      }
    }
  }
  // not a valid block num, return invalid file_loc
  loc.valid = 0;
  return loc;
}

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within any indirect within the indirect specified by blocknum b.
// If b is not valid, an invalid file_loc is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to an indirect of indirects of dirents. The behavior if b is a 
// valid blocknum to another structure type is undefined.
file_loc get_inode_double_indirect_dirent(blocknum b, char *buf, const char *path) {

  // the file_loc to return
  file_loc loc;

  // check that the indirect blocknum is valid
  if (b.valid) {
    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    indirect double_indirect;
    memcpy(&double_indirect, buf, BLOCKSIZE);
    // check each indirect for the file specified by path
    for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
      loc = get_inode_single_indirect_dirent(double_indirect.blocks[i], buf, path);
      // will only be valid if file location exists in ith indirect
      if (loc.valid) {
        // set the index of the indirect where the file was located in the 
        // double_indirect.blocks array
        loc.indirect_idx = i;
        return loc;
      }
    }
  }
  // file not found, return invalid file_loc
  loc.valid = 0;
  return loc;
}

// TODO: Multiple things have these structs.. can we abstract by passing a param???
// Access Single_indirect
blocknum get_single_block(int loc) {
  // mod by 128 for single index
  // divide by 128 for index in direct
}
// Access Double_indirect
blocknum get_double_block(int loc) {
  // mod by 128 for index in double single index
  // call get_single with loc/128
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
  fprintf(stderr, "\nIN vfs_read\n");

	file_loc parent = get_root_dir();
  file_loc loc = get_dir(path, path, &parent);
  // file does not exist
  if (!loc.valid) {
    return -1;
  }

  // TODO use a helper function to get inode block
  char tmp_buf[BLOCKSIZE];
  
  // Get the inode
  inode this_inode = get_inode(loc.node_block.block);

  // if the offset is larger than the size of the inode, no read possible
  if (offset >= this_inode.size) {
    return -1;
  }
  //********************** ABSTRACT FROM HERE **********************
  // Number of db blocks in this inode
  int current_blocks = (int) ceil((double) this_inode.size / BLOCKSIZE);

  // Block in sequence we will read from first (index into all_blocks)
  int starting_block = offset / BLOCKSIZE;
  // offset to start at within that block
  int local_offset = offset % BLOCKSIZE;
  // amount to read from the first block
  int first_block_read_size = BLOCKSIZE - local_offset;

  // only read to the end of file
  int amount_to_read = size;

  // if we might read past the end of file, update amount_to_read
  // to stop at the end of the file
  if ((offset + size) > this_inode.size) {
    amount_to_read = this_inode.size - offset;
  } 

  // The list of db blocks for this inode, in order
  blocknum all_blocks[current_blocks];
  
  indirect sing = get_indirect2(this_inode.single_indirect.block, tmp_buf);
  indirect doub = get_indirect2(this_inode.double_indirect.block, tmp_buf);
  int indirect_blocks = BLOCKSIZE/sizeof(blocknum);
  // Get a list of the db blocks containing data for this inode
  for (int i = 0; i < current_blocks; i++) {
    if (i < NUM_DIRECT) {
      // add it to our list
      all_blocks[i] = this_inode.direct[i];
    } 
    // Case for single
    else if ( i < (NUM_DIRECT + indirect_blocks)) {
      all_blocks[i] = sing.blocks[i - NUM_DIRECT];
    }
   // Case for double
   else {
     int block_loc = i - NUM_DIRECT - indirect_blocks;
     int s_loc = block_loc / indirect_blocks;
     int loc_in_s = block_loc % indirect_blocks;
     blocknum sing_blocknum = doub.blocks[s_loc];
  
     // REMOVE THE NESCESSITY TO READ EVERYTIME   
     indirect new_sing = get_indirect2(sing_blocknum.block, tmp_buf);
     all_blocks[i] = new_sing.blocks[loc_in_s];
   }
  }
  
  //********************** TO HERE *********************
  // ****** LOGIC FOR READING********
  // characters read so far
  int read = 0;

  // Data Block we are reading from
  db current_db = get_db(all_blocks[starting_block].block, tmp_buf);
 
  // If we will only read from the first block, do that and return
  if (amount_to_read <= first_block_read_size) {
    strncpy(buf, &current_db.data[local_offset], amount_to_read);
    return amount_to_read;
  }
  // We will read from more than the first block, read it and decrement amount to read
  else {
    strncpy(buf, &current_db.data[local_offset], first_block_read_size);
    amount_to_read -= first_block_read_size;  
    read += first_block_read_size; 
  }

  // Read the rest
  while (amount_to_read > 0) {
    //increment starting_block, so we can get the next block
    starting_block++;
    // Get the correct db block
    current_db = get_db(all_blocks[starting_block].block, tmp_buf);

    // Last read, last block, only read amount_to_read
    if (amount_to_read <= BLOCKSIZE) {
      strncpy(&buf[read], current_db.data, amount_to_read); 
      read += amount_to_read;
      amount_to_read = 0;   
      return read;
    }
    // We will be reading this whole block
    else {
      strncpy(&buf[read], current_db.data, BLOCKSIZE); 
      amount_to_read -= BLOCKSIZE;  
      read += BLOCKSIZE; 
    }
  }

  // return how much we read
  return read;
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

  fprintf(stderr, "\nIN vfs_write\n");
  /* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
           MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */

	file_loc parent = get_root_dir();
  file_loc loc = get_dir(path, path, &parent);
  // file does not exist
  if (!loc.valid) {
    return -1;
  }

  // TODO use a helper function to get inode block
  char tmp_buf[BLOCKSIZE];
  
  // Get the inode
  inode this_inode = get_inode(loc.node_block.block);
  
  // calculate the extra bytes we need to write to the file
  int needed_bytes = offset + size;
  int current_blocks = (int) ceil((double) this_inode.size / BLOCKSIZE);
  int needed_blocks = (int) ceil((double) needed_bytes / BLOCKSIZE);
  // the additional number of blocks needed to write the data
  
  int additional_blocks = needed_blocks - current_blocks;

  // the blocks we need to write to
  blocknum blocks[abs(additional_blocks)];
  if (additional_blocks > 0) {

    // get the free blocks needed to write additional data
    for (int i = 0; i < additional_blocks; i++) {
      blocknum free_block = get_free();
      // if we ran out of free blocks
      if (!free_block.valid) {
        release_blocks(blocks, i);
        return -1;
      }
      blocks[i] = free_block;
    }
  }

  // The list of blocks for this inode, in order
  blocknum all_blocks[needed_blocks];
   
  indirect sing = get_indirect2(this_inode.single_indirect.block, tmp_buf);
  indirect doub = get_indirect2(this_inode.double_indirect.block, tmp_buf);
  int indirect_blocks = BLOCKSIZE/sizeof(blocknum);

 
  // Get a list of the blocks we already have...
  // ******* WE NEED TO TALK ABOUT THIS LOGIC **********
  // THESE MUST BE IN ORDER, NO GAPS IN DIRECT
  // index into all_blocks
  int i;
  for (i = 0; i < current_blocks; i++) {
    if (i < NUM_DIRECT) {
      // add it to our list
      all_blocks[i] = this_inode.direct[i];
    }
    // get from single
    else if ( i < (NUM_DIRECT + indirect_blocks)) {
      all_blocks[i] = sing.blocks[i - NUM_DIRECT];
    }
    else {
      int block_loc = i - NUM_DIRECT - indirect_blocks;
      int s_loc = block_loc / indirect_blocks;
      int loc_in_s = block_loc % indirect_blocks;
      blocknum sing_blocknum = doub.blocks[s_loc];
  
      // REMOVE THE NESCESSITY TO READ EVERYTIME   
      indirect new_sing = get_indirect2(sing_blocknum.block, tmp_buf);
      all_blocks[i] = new_sing.blocks[loc_in_s];
    }
  }

  // Make a single, write to disk
  if ((!this_inode.single_indirect.valid) && (needed_blocks > NUM_DIRECT)) {
    blocknum free_block = get_free();
    if (!free_block.valid) {
      release_blocks(blocks, additional_blocks);
      return -1;
    }
    indirect new_sing;
    write_indirect(this_inode.single_indirect.block, tmp_buf, new_sing);
    this_inode.single_indirect = free_block;
    sing = new_sing;
  }

  // Make a double, write to disk
  if ((!this_inode.double_indirect.valid) && 
    (needed_blocks > NUM_DIRECT + indirect_blocks)) {
    blocknum free_block = get_free();
    if (!free_block.valid) {
      release_blocks(blocks, additional_blocks);
      return -1;
    }
    indirect new_doub;
    write_indirect(this_inode.double_indirect.block, tmp_buf, new_doub);
    this_inode.double_indirect = free_block;
    doub = new_doub;
  }

   // DETERMINE IF WE NEED TO MAKE SINGLE BLOCKS FOR DOUBLE
  // ADD THEM
  // WRITE IT
  if ((needed_blocks >= (NUM_DIRECT + indirect_blocks)) && 
    (additional_blocks > 0)) {
    int block_loc = needed_blocks - NUM_DIRECT - indirect_blocks;
    int s_loc = (int) ceil((double)block_loc / indirect_blocks);
  
    // Make sure each of the singles in the double is valid  
    for (int y = 0; y < s_loc; y++) {
      // if not valid, make one
      if (!doub.blocks[y].valid) {
        blocknum free_block = get_free();
        // if we run out of free, return
        if (!free_block.valid) {
          release_blocks(blocks, additional_blocks);
          return -1;
        }  
        // Create a new single, write to disk and assign to doub
        indirect new_sing;
        write_indirect(free_block.block, tmp_buf, new_sing);
        doub.blocks[y] = free_block;
      }
    }
    // Write the changed doub
    write_indirect(this_inode.double_indirect.block, tmp_buf, doub);
  // If we need to create more blocks to write, add them to our list
  if (additional_blocks > 0) {
    // index into blocks list (ones we newly created)
    int j = 0;
    // Iterate through blocks, add them, capture locally list of all blocks
    for (i; i < needed_blocks; i++) {
      db new_db;

      // add to direct
      if ( i < NUM_DIRECT) {
        //add it to inode and all_blocks
        all_blocks[i] = blocks[j];
        // blocks[j] should be valid
        this_inode.direct[i] = blocks[j];  // TODO we don't need all blocks
      }
      // Add to single
      else if (i < (NUM_DIRECT + indirect_blocks)) {
        sing.blocks[i - NUM_DIRECT] = blocks[j];
      }
      // Add to double
      else {
        int block_loc = i - NUM_DIRECT - indirect_blocks;
        int s_loc = block_loc / indirect_blocks;
        int loc_in_s = block_loc % indirect_blocks;
        blocknum sing_blocknum = doub.blocks[s_loc];
  
        // REMOVE THE NESCESSITY TO READ EVERYTIME   
        indirect new_sing = get_indirect2(sing_blocknum.block, tmp_buf);
        new_sing.blocks[loc_in_s] = blocks[j];
        write_indirect(sing_blocknum.block, tmp_buf, new_sing);      
      }

      // we can do this with just direct
      write_db(blocks[j].block, tmp_buf, new_db);
      j++;
    }
  }
  // write the modified single
  write_indirect(this_inode.single_indirect.block, tmp_buf, sing);

  // Block in sequence we will write to first (index into all_blocks)
  int starting_block = offset / BLOCKSIZE;
  // offset to start at within that block
  int local_offset = offset % BLOCKSIZE;
  // amount to write into the first block
  int first_block_write_size = BLOCKSIZE - local_offset;

// ****** LOGIC FOR WRITING********
  // number of bytes written so far
  int written = 0;
  int to_write = size;

  // Data Block we are now writing to
  db current_db = get_db(all_blocks[starting_block].block, tmp_buf);
 
  // if the write will fit completely in the first block
  if (to_write <= first_block_write_size) {
    strncpy(&current_db.data[local_offset], buf, to_write);
    written += to_write;
  }
  // Write what we can to the first block, update counters
  else {
    strncpy(&current_db.data[local_offset], buf, first_block_write_size);
    written += first_block_write_size;
    to_write -= first_block_write_size;
  }
  // write the db to disk  
  if (write_db(all_blocks[starting_block].block, tmp_buf, current_db) == 0) {
    // throw error
  }
  
  // have we written everything
  if (written == size) {
    // update the inodes size
    this_inode.size += written;
    // write the inode
    if (write_inode(loc.node_block.block, tmp_buf, this_inode)  == 0) {
      // error writing
    }    
    return written;
  }

  // iterate through all_blocks, until we have written everything
  
  while (to_write > 0) {
    // Get the next block
    starting_block++;
    unsigned int db_blocknum = all_blocks[starting_block].block;
    current_db = get_db(db_blocknum, tmp_buf);

    // if less than block size, only write to_write
    if (to_write < BLOCKSIZE) {
      strncpy(&current_db.data, &buf[written], to_write);
      written += to_write;
      to_write = 0;
   }
    // otherwise write BLOCKSIZE
    else { 
      strncpy(&current_db.data, &buf[written], BLOCKSIZE);
      written += BLOCKSIZE;
      to_write -= BLOCKSIZE;
    }
     
    // write the block to disk
    if (write_db(db_blocknum, tmp_buf, current_db)  == 0) {
      // error writing
    }
  }

  this_inode.size += size;
  // write the inode
  if (write_inode(loc.node_block.block, tmp_buf, this_inode)  == 0) {
    // error writing
  }

  return size;
}


// Add the given list of blocks to our free block list
void release_blocks(blocknum blocks[], int size) {
  // temp buffer
  char buf[BLOCKSIZE];
  // get the vcb to update the free blocks
  vcb this_vcb = get_vcb(buf);
  // get the head of the free block list
  blocknum tmp = this_vcb.free;

  // free all given blocks
  for (int i = 0; i < size; i++) {
  	// ensure we only release valid blocks
  	if (blocks[i].valid) {
      free_b new_free;
      new_free.next = tmp;
      //  zero out buffer
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &new_free, sizeof(free_b));
      // write new free block to disk
      dwrite(blocks[i].block, buf);
      tmp = blocks[i];
		}
  }
  // update the vcb free list head
  this_vcb.free = tmp;
  memset(buf, 0, BLOCKSIZE);
  memcpy(buf, &this_vcb, sizeof(vcb));
  dwrite(0, buf);
}


/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{
  fprintf(stderr, "\nIN vfs_delete\n");
  char buf[BLOCKSIZE];
  // Get the file location of the given path 
	file_loc parent = get_root_dir();
  file_loc loc = get_dir(path, path, &parent);

  // If it is valid, get the dirent, and go to the direntry
  // for the given file
  if (loc.valid) {
    // Update the cache
    remove_cache_entry(path);

    // Get the dirent
    dirent file_dirent;
    memset(buf, 0, BLOCKSIZE);
    dread(loc.dirent_block.block, buf);
    memcpy(&file_dirent, buf, sizeof(dirent));

    // Set inode block to invalid in the dirent
    file_dirent.entries[loc.direntry_idx].block.valid = 0;
    // write the dirent to disk
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &file_dirent, sizeof(dirent));
    dwrite(loc.dirent_block.block, buf);

    // Get all the db block of the file inode and free them.
    // Free the inode block as well.
    int size;
    blocknum *all_blocks;
    get_file_blocks(loc.node_block, &all_blocks, &size);
    release_blocks(all_blocks, size);
  }

  return 0;
}

// Get all the blocks from an inode (all db blocks) and itself.
// This implementation is not ideal. Could be made faster.
// Redult is stored in blocks array with size size.
void get_file_blocks(blocknum in, blocknum *blocks[], int *size) {
  // Free the inode; free the inode's db blocks; free the inodes
  inode this_inode = get_inode(in.block);

	// all the blocks; direct + single + double (single^2) + this file block
	int all_blocks_size = NUM_DIRECT + NUM_INDIRECT_BLOCKS + 
		(NUM_INDIRECT_BLOCKS * NUM_INDIRECT_BLOCKS) + 1;
	blocknum all_blocks[all_blocks_size];

	// add the inode block
	*size = 0;
  all_blocks[*size] = in;
  size++;

	// add all valid blocknums from direct
	for (int i = 0; i < NUM_DIRECT; i++) {
		if (this_inode.direct[i].valid) {
  	  all_blocks[*size] = this_inode.direct[i];
  	  size++;
		}
	}

	// add all valid blocknums from single_indirect.blocks if valid
	if (this_inode.single_indirect.valid) {
		// get the single indirect
		indirect si = get_indirect(this_inode.single_indirect);
  	for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
  		if (si.blocks[i].valid) {
			  all_blocks[*size] = si.blocks[i];
  		  size++;
			}
	  }
	}

	// add all valid blocknums from double_indirect.blocks if valid
	if (this_inode.double_indirect.valid) {
		// get the double indirect
		indirect di = get_indirect(this_inode.double_indirect);
		// loop through each single indirect
  	for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
  		// get the ith single indirect
  		if (di.blocks[i].valid) {
  			indirect si = get_indirect(di.blocks[i]);
    		// loop through the blocks of each single indirect
    		for (int j = 0; j < NUM_INDIRECT_BLOCKS; j++) {
    			if (si.blocks[i].valid) {
  	  	    all_blocks[*size] = si.blocks[i];
  		      size++;
					}
		  	}
			}
	  }
	}
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
  fprintf(stderr, "\nIN vfs_rename\n");
  // get_file -> then rename...

  // if they are the same, no need to actually rename
  if (strncmp(from, to, MAX_FILENAME_LEN) == 0) {
    return 0;
  }

  // now get the file we are going to change
	file_loc parent = get_root_dir();
  file_loc loc = get_dir(from, from, &parent);

  // If the file exists, change mode
  if (loc.valid) {

    // Delete it if it exists, otherwise does nothing...
    vfs_delete(to);

    // Get the dirent and adjust the name...
    // TODO: Should we abstarct this as well???
    dirent this_dirent;
    char buf[BLOCKSIZE];
    dread(loc.dirent_block.block, buf);
    memcpy(&this_dirent, buf, sizeof(dirent));

    // Change the name at the proper direntry
    // TODO: Change the +1
    strncpy(this_dirent.entries[loc.direntry_idx].name, to+1, MAX_FILENAME_LEN);

    // Write modified one to disk
    memcpy(buf, &this_dirent, sizeof(dirent));
    dwrite(loc.dirent_block.block, buf);

    // udpate the cache
    update_cache_entry(from, to);
    return 0;
  }
  
  // No such file exists
  // TODO: get this to work for directories
  else {
    return -1;
  }
  return 0;
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
  fprintf(stderr, "\nIN vfs_chmod\n");
  // get_file -> reassign
	file_loc parent = get_root_dir();
  file_loc loc = get_dir(file, file, &parent);

  // If the file exists, change mode
  if (loc.valid) {
    // Get the inode for the file
    // TODO: Should we abstarct this as well???
    inode this_inode;
    char buf[BLOCKSIZE];
    dread(loc.node_block.block, buf);
    memcpy(&this_inode, buf, sizeof(inode));

    // Change shit up
    this_inode.mode = (mode_t) mode;

    // Write modified one to disk
    memcpy(buf, &this_inode, sizeof(inode));
    dwrite(loc.node_block.block, buf);

    return 0;
  }
  
  // No such file exists
  // TODO: get this to work for directories
  else {
    return -1;
  }

  return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{
  fprintf(stderr, "\nIN vfs_chown\n");
  // get file -> reassign
	file_loc parent = get_root_dir();
  file_loc loc = get_dir(file, file, &parent);

   // If the file exists, change mode
  if (loc.valid) {
    // Get the inode for the file
    inode this_inode;
    char buf[BLOCKSIZE];
    
    dread(loc.node_block.block, buf);
    memcpy(&this_inode, buf, sizeof(inode));

    this_inode.user = uid;
    this_inode.group = gid;
    
    // Write modified one to disk
    memcpy(buf, &this_inode, sizeof(inode));
    dwrite(loc.node_block.block, buf);

    return 0;
  }
  
  // No such file exists
  else {
    return -1;
  }

  return 0;

}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
  fprintf(stderr, "\nIN vfs_utimens\n");

  // Get the file/directory
	file_loc parent = get_root_dir();
  file_loc loc = get_dir(file, file, &parent);
 
  // If the file exists, change mode
  if (loc.valid) {
    // Get the inode for the file
    // TODO: Should we abstarct this as well???
    inode this_inode;
    char buf[BLOCKSIZE];
    dread(loc.node_block.block, buf);
    memcpy(&this_inode, buf, sizeof(inode));

    // Change shit up
    this_inode.access_time = ts[0];
    this_inode.modify_time = ts[1];

    // Write modified one to disk
    memcpy(buf, &this_inode, sizeof(inode));
    dwrite(loc.node_block.block, buf);
    return 0;
  }
  
  // No such file exists
  // TODO: get this to work for directories
  else {
    return -1;
  }

  return 0;

}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{
  fprintf(stderr, "\nIN vfs_truncate\n");
  /* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE. */
	file_loc parent = get_root_dir();
  file_loc loc = get_dir(file, file, &parent);
  // file does not exist
  if (!loc.valid) {
    return -1;
  }

  char tmp_buf[BLOCKSIZE];
  // Get the inode
  inode this_inode = get_inode(loc.node_block.block);

  // check if the offset exceeds the file size
  if (this_inode.size - 1 < offset) {
    return -1;
  }

  // the number of data blocks that this inode has 
  int current_blocks = (int) ceil((double) this_inode.size / BLOCKSIZE);
  // The list of blocks for this inode, in order
  blocknum all_blocks[current_blocks];
  
  // Get a list of the blocks we already have...
  int i;
  for (i = 0; i < current_blocks; i++) {
    if (this_inode.direct[i].valid) {
      // add it to our list
      all_blocks[i] = this_inode.direct[i];
    }
  }

  // First block to free
  int starting_block = (int) ceil((double) offset / BLOCKSIZE);
  int blocks_to_delete = current_blocks - starting_block;
  // blocks need to be deleted
  if (blocks_to_delete > 0) {
    release_blocks(&all_blocks[starting_block], blocks_to_delete);
  }

  // update the size of the file
  this_inode.size = offset;
  write_inode(loc.node_block.block, tmp_buf, this_inode);

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
  .init     = vfs_mount,
  .destroy  = vfs_unmount,
  .getattr  = vfs_getattr,
  .readdir  = vfs_readdir,
  .create   = vfs_create,
  .read     = vfs_read,
  .write    = vfs_write,
  .unlink	  = vfs_delete,
  .rename   = vfs_rename,
  .chmod    = vfs_chmod,
  .chown    = vfs_chown,
  .utimens  = vfs_utimens,
  .truncate	= vfs_truncate,
  .mkdir = vfs_mkdir,
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

