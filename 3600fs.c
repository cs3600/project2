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


	// TODO handle multi-directory

	// check if root
  if (strcmp("/", path) == 0) {
    
    // read in dnode
		dnode root_dnode = get_dnode(1, buf);

    stbuf->st_mode  = root_dnode.mode | S_IFDIR;		

    // update stats
	  stbuf->st_uid     = root_dnode.user; // file uid
	  stbuf->st_gid     = root_dnode.group; // file gid
  	stbuf->st_atime   = root_dnode.access_time.tv_sec; // access time 
    stbuf->st_mtime   = root_dnode.modify_time.tv_sec; // modify time
    stbuf->st_ctime   = root_dnode.create_time.tv_sec; // create time
	  stbuf->st_size    = root_dnode.size; // file size
	  stbuf->st_blocks  = root_dnode.size / BLOCKSIZE; // file size in blocks
	  return 0;
  }
	// we have a regular file
  else {

		// get attr if exists
  	file_loc loc = get_file(path);

  	// not valid file
 	  if (loc.valid) {

			// read in inode
  		inode this_inode; 
  		dread(loc.inode_block.block, buf);
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
  fprintf(stderr, "\nIN vfs_readdir\n");

  // TODO not for multi level
  if (strcmp("/", path) != 0) {
  	return -1;
	}

	// TODO for multi-level directories
  // TODO get the file_loc
  // check if it is a directory
  // otherwise throw a directory does not exist error
  //
  // file_loc should be updated to reflect file type

  // we know it's the root dir
  char tmp_buf[BLOCKSIZE];
  dnode root_dnode = get_dnode(1, tmp_buf);
  // list the direct entries of the dnode
  list_entries(root_dnode.direct, NUM_DIRECT, filler, buf);

  // list the single indirect entries
  list_single(root_dnode.single_indirect, filler, buf);
  // list the double indirect entries of the dnode
  list_double(root_dnode.double_indirect, filler, buf);

	return 0;
}

// Returns the indirect at the specified blocknum
// Undefined behavior if b does not point to a valid indirect
indirect get_indirect(blocknum b) {
  	// zeroed out buf 
  	char tmp_buf[BLOCKSIZE];
  	memset(tmp_buf, 0, BLOCKSIZE);
  	// read indirect from disk
  	indirect i;
  	dread(b.block, tmp_buf);
  	memcpy(&i, tmp_buf, sizeof(indirect));

  	return i;
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

  // TODO multi-level path parsing

  // remove debugging statement TODO
  fprintf(stderr, "Creating file %s\n", path);

  // Allocate appropriate memory 
  char buf[BLOCKSIZE];
  // Get the dnode
  dnode thisDnode = get_dnode(1, buf);

  blocknum file;
  file_loc loc;
  // blocknum of Inode of file
  loc = get_file(path);

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
    if (init_inode(this_inode, buf, mode, fi) != 1)
      return -1;

    // Loop through dnode direct
    if (create_inode_direct_dirent(&thisDnode, this_inode, path, buf) == 1) {
			return 0;
		}

    // Loop through dnode -> single_indirect
    if (create_inode_single_indirect_dirent(thisDnode.single_indirect, this_inode, path, buf) == 1) {
      memset(buf, 0, BLOCKSIZE);
      memcpy(buf, &thisDnode, sizeof(dnode));
      dwrite(1, buf);
      return 0;
    }
    // Loop through dnode -> double_indirect
    else if (create_inode_double_indirect_dirent(thisDnode.double_indirect, this_inode, path, buf) == 1) {
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

// Initialize inode metadata to the given inode blocknum.
// Returns 0 if the block b is not valid.
int init_inode(blocknum b, char *buf, mode_t mode, struct fuse_file_info *fi) {
  // make sure the block is valid 
  if (b.valid) {
    // create new inode
    inode new_inode;
    // set file metadata
    new_inode.size = 0;
    new_inode.user = getuid();
    new_inode.group = getgid();
    new_inode.mode = mode;
    clockid_t now = CLOCK_REALTIME;
    clock_gettime(now, &new_inode.access_time);
    clock_gettime(now, &new_inode.modify_time);
    clock_gettime(now, &new_inode.create_time);
    // Invalidate all direct blocks 
    blocknum invalid;
    invalid.valid = 0;
    invalid.block = 0;
    for (int i = 0; i < NUM_DIRECT; i++) {
    	new_inode.direct[i] = invalid;
		}
    // invalidate the single and double indirects
    new_inode.single_indirect.valid = 0;
    new_inode.double_indirect.valid = 0;
    // zero out the buffer and write inode to block b
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &new_inode, sizeof(inode));
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

// Link the inode to the next open direntry in the given dirent d
// returns 0 if there are no open direntries
int create_inode_dirent(blocknum d, blocknum inode, const char *path, char *buf) {
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
      dir.entries[i].block = inode;
      // TODO don't hardcode path+1; won't work for multi dir
      strncpy(dir.entries[i].name, path+1, MAX_FILENAME_LEN - 1);
      dir.entries[i].name[MAX_FILENAME_LEN - 1] = '/0';
      // set the type to a file
      dir.entries[i].type = 0;

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
int create_inode_direct_dirent(dnode *thisDnode, blocknum inode, const char *path, char *buf) {

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
    if (create_inode_dirent(thisDnode->direct[i], inode, path, buf)) {
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
int create_inode_single_indirect_dirent(blocknum s, blocknum inode, const char *path, char *buf) {
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
    if (create_inode_dirent(single_indirect.blocks[i], inode, path, buf)) {
      return 1;
    }
  }
  // No space available
  return 0;
}

// Create a file at the next open direntry in this double_indirect
// returns 0 if there are no open direntries
int create_inode_double_indirect_dirent(blocknum d, blocknum inode, const char *path, char *buf) {
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
    if (create_inode_single_indirect_dirent(double_indirect.blocks[i], inode, path, buf)) {
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


// Returns the file_loc of the given path's Inode.
// If the file specified by the path does not have an associated Inode,
// then the function returns an invalid file_loc.
file_loc get_file(const char *path) {
  // Start at the Dnode and search the direct, single_indirect,
  // and double_indirect

  // general-purpose temporary buffer
  char *buf = (char *) malloc(BLOCKSIZE);

  // temporary file_loc buffer
  file_loc loc; 
  // check the cache for our file
  loc = get_cache_entry(path);
  if (loc.valid) {
    fprintf(stderr, "Cache Hit: %s\n", path);
    return loc;
	}

  // Read data into a Dnode struct
  // TODO when implementing multi-directory, this will change
  dnode thisDnode = get_dnode(1, buf);

	// look at the right snippet of the path
	char *file_path = path;
	while (*file_path != '\0') {
		// increment pointer until non '/' found; start of file name
		if (*file_path == '/')
			file_path++;
		else 
			break;
	}

  // TODO with multi-directory, path must be parsed
	// TODO make check_subdirs function that just checks that the 
	// the dnodes contain the appropriate sub dnodes, then just check
	// the file at the end as we do below

	// Check each dirent in direct to see if the file specified by
	// path exists in the filesystem.
	loc = get_inode_direct_dirent(&thisDnode, buf, file_path);

	// If valid return the value of the file_loc; update the cache
	if (loc.valid) {
		free(buf);
		add_cache_entry(path, loc);
		return loc;
	}

  // Check each dirent in the single_indirect to see if the file specified by
  // path exists in the filesystem.
  loc = get_inode_single_indirect_dirent(thisDnode.single_indirect, buf, file_path);

	// If valid return the value of the file_loc; update the cache
  if (loc.valid) {
    free(buf);
    // set the direct, single_indirect, and double_indirect flags
    loc.direct = 0;
    loc.single_indirect = 1;
    loc.double_indirect = 0;
		add_cache_entry(path, loc);
    return loc;
  }

	// Check each dirent in each indirect in the double_indirect to see if the
  // file specified by path exists in the filesystem.
  loc = get_inode_double_indirect_dirent(thisDnode.double_indirect, buf, file_path);
	// If valid return the value of the file_loc; update the cache
  if (loc.valid) {
    free(buf);
    // set the direct, single_indirect, and double_indirect flags
    loc.direct = 0;
    loc.single_indirect = 0;
    loc.double_indirect = 1;
		add_cache_entry(path, loc);
    return loc;
  }

  // The file specified by path does not exist in our file system.
  // Return invalid file_loc.
  free(buf);
  return loc;
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

// Reads the inode at the given block number into buf
inode get_inode(unsigned int b, char *buf) {
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
          loc.inode_block = tmp_dirent.entries[i].block;
          loc.direntry_idx = i;
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

  file_loc loc = get_file(path);
  // file does not exist
  if (!loc.valid) {
    return -1;
  }

  // TODO use a helper function to get inode block
  char tmp_buf[BLOCKSIZE];
  
  // Get the inode
  inode this_inode = get_inode(loc.inode_block.block, tmp_buf);
  
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

  // if the offset is larger than the size of the inode, no read possible
  if (offset >= this_inode.size) {
    return 0;
  }


  // The list of blocks for this inode, in order
  blocknum all_blocks[current_blocks];
  
  // Get a list of the blocks we already have...
  // ******* WE NEED TO TALK ABOUT THIS LOGIC **********
  // THESE MUST BE IN ORDER, NO GAPS IN DIRECT
  // index into all_blocks
  for (int i = 0; i < current_blocks; i++) {
    if (this_inode.direct[i].valid) {
      // add it to our list
      all_blocks[i] = this_inode.direct[i];
    }
  }

  // ****** LOGIC FOR READING********
  // Iterate to the starting block
  // read up to size
  // stop at size
  // number of characters read so far
  int read = 0;

  // Data Block we are reading from
  db current_db = get_db(all_blocks[starting_block].block, tmp_buf);
 
  for (read; read < first_block_read_size; read++) {
    // we have read all the blocks we need to
    if (read == amount_to_read) {
      break;
    }
    // TODO: Error check here???
    buf[read] = current_db.data[local_offset + read]; 
  }
  
  // have we read everything
  if (read == amount_to_read) {   
    buf[read] = '\0'; 
    return read;
  }

  // THIS SHOULD BE INSIDE BLOCKS
  // Read until we are done...
  // iterate through all_blocks, should break when we have 
  for (int n = starting_block + 1; n < current_blocks ; n++) {
    // if we have read all we need to, break
    if (read == amount_to_read) {
      break;
    }  

    // current db blocknum
    unsigned int db_blocknum = all_blocks[n].block;
    // Get the next db 
    current_db = get_db(db_blocknum, tmp_buf);
    
    for (int m = 0; m < BLOCKSIZE; m++) {
      // we have written all the blocks we need to
      if (read == amount_to_read) {
        break;
      }
      // TODO: ERROR HANDLING?
      // capture the char
      buf[read] = current_db.data[m]; 
      // Increment read
      read++;    
    }  
  }

  buf[read] = '\0';

	// udpate the access time TODO
  return amount_to_read;
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

  file_loc loc = get_file(path);
  // file does not exist
  if (!loc.valid) {
    return -1;
  }

  // TODO use a helper function to get inode block
  char tmp_buf[BLOCKSIZE];
  
  // Get the inode
  inode this_inode = get_inode(loc.inode_block.block, tmp_buf);
  
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
        release_free(blocks, i);
        return -1;
      }
      blocks[i] = free_block;
    }
  }

  // The list of blocks for this inode, in order
  blocknum all_blocks[needed_blocks];
  
  // Get a list of the blocks we already have...
  // ******* WE NEED TO TALK ABOUT THIS LOGIC **********
  // THESE MUST BE IN ORDER, NO GAPS IN DIRECT
  // index into all_blocks
  int i;
  for (i = 0; i < current_blocks; i++) {
    if (this_inode.direct[i].valid) {
      // add it to our list
      all_blocks[i] = this_inode.direct[i];
    }
  }

  // If we need to create more blocks to write, add them to our list
  if (additional_blocks > 0) {
    // index into blocks list (ones we newly created)
    int j = 0;
    // Iterate through blocks, add them, capture locally list of all blocks
    for (i; i < needed_blocks; i++) {
      //add it to inode and all_blocks
      all_blocks[i] = blocks[j];
      // blocks[j] should be valid
      this_inode.direct[i] = blocks[j];  // TODO we don't need all blocks
      db new_db;                         // we can do this with just direct
      write_db(blocks[j].block, tmp_buf, new_db);
      j++;
    }
  }


  // Block in sequence we will write to first (index into all_blocks)
  int starting_block = offset / BLOCKSIZE;
  // offset to start at within that block
  int local_offset = offset % BLOCKSIZE;
  // amount to write into the first block
  int first_block_write_size = BLOCKSIZE - local_offset;

// ****** LOGIC FOR WRITING********
  // Iterate to the starting block
  // write up to size
  // stop at size
  // number of bytes written so far
  int written = 0;


  // we have written everything we can from the buf
  int buf_done = 0;

  // Data Block we are now writing to
  db current_db = get_db(all_blocks[starting_block].block, tmp_buf);
 
  for (written; written < first_block_write_size; written++) {
    // we have written all the blocks we need to
    if (written == size) {
      break;
    }
    
    // we have written everything we can from buf, but haven't
    // reached size, put in 0's
    if (buf_done) {
      current_db.data[local_offset + written] = '/0';
    }
    // We have reached the end of the buf
    else if (buf[written] == '/0') {
      // mark that there is no more data for us to read from buf
      buf_done = 1;
      current_db.data[local_offset + written] = '/0';
    }
    // We have info to ge5t from buf, write it to the current_db
    else {
      current_db.data[local_offset + written] = buf[written]; 
    } 

    if (write_db(all_blocks[starting_block].block, tmp_buf, current_db)  == 0) {
      // throw error
    }
  }
  
  // have we written everything
  if (written == size) {
    // update the inodes size
    this_inode.size += written;
    // write the inode
    if (write_inode(loc.inode_block.block, tmp_buf, this_inode)  == 0) {
      // error writing
    }
    
    return written;
  }

  // THIS SHOULD BE INSIDE BLOCKS
  // Write until we are done...
  // iterate through all_blocks, i is the size of all_blocks
  for (int n = starting_block + 1; n < i; n++) {
    // current db blocknum
    unsigned int db_blocknum = all_blocks[n].block;
    // Get the next db 
    current_db = get_db(db_blocknum, tmp_buf);
    
    for (int m = 0; m < BLOCKSIZE; m++) {
      // we have written all the blocks we need to
      if (written == size) {
        break;
      }
    
      // we have written everything we can from buf, but haven't
      // reached size, put in 0's
      if (buf_done) {
        current_db.data[m] = '0';
      }
      // We have reached the end of the buf
      else if (buf[written] == '/0') {
        // mark that there is no more data for us to read from buf
        buf_done = 1;
        current_db.data[m] = '0';
      }
      // We have info to ge5t from buf, write it to the current_db
      else {
        current_db.data[m] = buf[written]; 
      }

      // increment written to reflect writing
      written++;    
    }  
    
    // WRITE THAT SHIT... 
    if (write_db(db_blocknum, tmp_buf, current_db)  == 0) {
      // error writing
    }
  }

  this_inode.size += size;
  // write the inode
  if (write_inode(loc.inode_block.block, tmp_buf, this_inode)  == 0) {
    // error writing
  }

//********** UPDATE THE SIZE OF THE INODE TO REFLECT WRITE *************
// RETURN AMOUNT WE WROTE...

  return size;
}


// Add the given list of blocks to our free block list
void release_free(blocknum blocks[], int size) {
  // temp buffer
  char buf[BLOCKSIZE];
  // get the vcb to update the free blocks
  vcb this_vcb = get_vcb(buf);
  // get the head of the free block list
  blocknum tmp = this_vcb.free;

  // free all given blocks
  for (int i = 0; i < size; i++) {
    free_b new_free;
    new_free.next = tmp;
    //  zero out buffer
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &new_free, sizeof(free_b));
    // write new free block to disk
    dwrite(blocks[i].block, buf);
    tmp = blocks[i];
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
	// check if the file exists
	// free DB blocks, free INode, free Dnode/dirent --> double check this is how FUSE works
  char buf[BLOCKSIZE];

  // Get the file location of the given path 
  file_loc loc = get_file(path);

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

    // This new free block's next will point to the 
    // VCB's previous free block. This free block
    // overwrites the inode block.
    vcb this_vcb = get_vcb(buf);
    free_b new_free;
    new_free.next = this_vcb.free;
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &new_free, sizeof(free_b));
    dwrite(loc.inode_block.block, buf);

    // Update VCB to point to the a new free block
    // that is located where the file was previously located.
    this_vcb.free = loc.inode_block;
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &this_vcb, sizeof(vcb));
    dwrite(0, buf);
  }

  return 0;
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
  file_loc loc = get_file(from);

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
  file_loc loc = get_file(file);

  // If the file exists, change mode
  if (loc.valid) {
    // Get the inode for the file
    // TODO: Should we abstarct this as well???
    inode this_inode;
    char buf[BLOCKSIZE];
    dread(loc.inode_block.block, buf);
    memcpy(&this_inode, buf, sizeof(inode));

    // Change shit up
    this_inode.mode = (mode_t) mode;

    // Write modified one to disk
    memcpy(buf, &this_inode, sizeof(inode));
    dwrite(loc.inode_block.block, buf);

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
  file_loc loc = get_file(file);

   // If the file exists, change mode
  if (loc.valid) {
    // Get the inode for the file
    // TODO: Should we abstarct this as well???
    inode this_inode;
    char buf[BLOCKSIZE];
    
    dread(loc.inode_block.block, buf);
    memcpy(&this_inode, buf, sizeof(inode));

    this_inode.user = uid;
    this_inode.group = gid;
    
    // Write modified one to disk
    memcpy(buf, &this_inode, sizeof(inode));
    dwrite(loc.inode_block.block, buf);

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
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
  fprintf(stderr, "\nIN vfs_utimens\n");

  // Get the file/directory
  file_loc loc = get_file(file);
 
  // If the file exists, change mode
  if (loc.valid) {
    // Get the inode for the file
    // TODO: Should we abstarct this as well???
    inode this_inode;
    char buf[BLOCKSIZE];
    dread(loc.inode_block.block, buf);
    memcpy(&this_inode, buf, sizeof(inode));

    // Change shit up
    this_inode.access_time = ts[0];
    this_inode.modify_time = ts[1];

    // Write modified one to disk
    memcpy(buf, &this_inode, sizeof(inode));
    dwrite(loc.inode_block.block, buf);
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
  file_loc loc = get_file(file);
  // file does not exist
  if (!loc.valid) {
    return -1;
  }

  // TODO use a helper function to get inode block
  char tmp_buf[BLOCKSIZE];
  
  // Get the inode
  inode this_inode = get_inode(loc.inode_block.block, tmp_buf);

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
    release_free(&all_blocks[starting_block], blocks_to_delete);
  }

  // update the size of the file
  this_inode.size = offset;
  write_inode(loc.inode_block.block, tmp_buf, this_inode);

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

