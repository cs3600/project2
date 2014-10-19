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
#include <sys/types.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

// Magic number to identify our disk
const int MAGICNUMBER = 184901;
// Number of direntries per dirent
const int NUM_DIRENTRIES = BLOCK_SIZE / sizeof(direntry);
// Number of block in an indirect
const int NUM_INDIRECT_BLOCKS = BLOCK_SIZE / sizeof(blocknum);

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

	// wrong file system
  if (myVcb.magic != MAGICNUMBER) {
    fprintf(stdout, "Attempt to mount wrong filesystem!\n"); // TODO change this
    // disconnect if its not our disk
    dunconnect(); // TODO vfs_unmount?
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
    stbuf->st_mode  = (0777 & 0x0000FFFF) | S_IFDIR;

		// read in dnode
		dnode root_dnode = get_dnode(1, buf);

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
  		memcpy(buf, &this_inode, sizeof(inode));

			// write the file??? TODO what is this BS? 
      stbuf->st_mode  = (0777 & 0x0000FFFF) | S_IFREG;

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

	// Error when not root or root file?? TODO
  return -1;
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
	// we know it's the root dir
  char tmp_buf[BLOCKSIZE];
  dnode root_dnode = get_dnode(1, tmp_buf);

  // iterate over all dirents in direct
  for (int i = 0; i < NUM_DIRECT; i++) {
  	// load dirent from disk into memory
  	blocknum dirent_b = root_dnode.direct[i];
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

  // check single indirect and double indirect TODO
  return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

  fprintf(stderr, "\nIN vfs_create\n");

  // TODO path parsing? We should do this only if subdirs are implemented

  // remove debugging statement TODO
  fprintf(stderr, "Creating file %s\n", path);

  // Allocate appropriate memory 
  char buf[BLOCKSIZE];
  
  // get the file specified by the path, if it exists
  file_loc loc;
  loc = get_file(path);

  // throw error if file already exists
  if (loc.valid) {
    fprintf(stderr, "The given file already exists.\n");
    return -EEXIST;
  }
    
  // Get next free block
  blocknum this_inode = get_free();
    
  // Check that the free block is valid
  if (this_inode.valid) {

    // Set up Inode; if not 1, the inode was not set
    if (init_inode(this_inode, buf, mode, fi) != 1)
      return -1;

    // store the inode as a direntry in its parent dnode
    if (create_new_direntry(&loc, this_inode, path, buf))
    	return 0;
  }

  // TODO: Out of memory message?
  return -1;
}

// Initialize inode metadata to the given inode blocknum.
// Returns 0 if there is block is not valid.
int init_inode(blocknum b, char *buf, mode_t mode, struct fuse_file_info *fi) {
  // make sure the block is valid 
  if (b.valid) {
    // create new inode
    inode new_inode;
    // the file has no data yet
    new_inode.size = 0;
    // TODO the file's user is the current user?
    new_inode.user = getuid();
    // TODO the file's group is the current group?
    new_inode.group = getgid();
    // set the file's mode
    new_inode.mode = mode;
    // get the clock time and set access/modified/created
    clockid_t now = CLOCK_REALTIME;
    clock_gettime(now, &new_inode.access_time);
    clock_gettime(now, &new_inode.modify_time);
    clock_gettime(now, &new_inode.create_time);
    // TODO what to do with direct?

    // invalidate the single and double indirects
    new_inode.single_indirect.valid = 0;
    new_inode.double_indirect.valid = 0;

    // zero out the buffer and write inode to blocknum in our filesystem
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
int create_dirent(blocknum *b, char *buf) {
	*b = get_free();
  // make sure the given block is valid
  if (b->valid) {
    // put an indirect structure in the given block
    memset(buf, 0, BLOCKSIZE);
    dirent new_dirent;
    // invalidate all blocks
    for (int i = 0; i < NUM_DIRENTRIES; i++) {
      new_dirent.entries[i].block.valid = 0;
    }
    memcpy(buf, &new_dirent, sizeof(dirent));
    dwrite(b->block, buf);
    return 1;
  }
  // Block was not valid, error
  return 0;
}

// Initialize the given blocknum to an indirect
// returns 0 if there is an error
int create_indirect(blocknum *b, char *buf) {
	*b = get_free();
	// make sure the given block is valid
  if (b->valid) {
    // put an indirect structure in the given block
    memset(buf, 0, BLOCKSIZE);
    indirect new_indirect;
    // invalidate all blocks
    for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
      new_indirect.blocks[i].valid = 0;
    }
    memcpy(buf, &new_indirect, sizeof(indirect));
    dwrite(b->block, buf);
    return 1;
  }
  // Block was not valid, error
  return 0;
}

// Create a new direntry for the inode in the dirent specified by
// loc.dirent_block in the dnode specified by loc.parent_dnode.
// The new dnode and dirent are written to disk.
int create_new_direntry(file_loc *loc, blocknum inode, char *path, char *buf) {
	// open direnty in the direct
	if (loc->first_open_direntry_found) {
		create_direntry(loc, inode, path, buf);
	}
	// create dirent in the direct
	else if (!loc->first_open_direntry_found && loc->direct) {
		blocknum b;
		create_dirent(&b, buf);
		loc->dirent_block = b;
		loc->direntry_idx = 0;
    create_direntry(loc, inode, path, buf);
    dnode p = loc->parent_dnode;
    p.direct[loc->list_idx] = b;
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &p, sizeof(dnode));
    dwrite(1, buf); // TODO loc needs dnode blocknum
	}
	// create dirent in the single
	else if (!loc->first_open_direntry_found && loc->single_indirect) {
		blocknum b;
		create_dirent(&b, buf);
		loc->dirent_block = b;
		loc->direntry_idx = 0;
    create_direntry(loc, inode, path, buf);
    dnode p = loc->parent_dnode;
    p.single.blocks[loc->list_idx] = b;
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &p, sizeof(dnode));
    dwrite(1, buf); // TODO loc needs dnode blocknum
	}
	// create indirect in the double
	else if (!loc->first_open_direntry_found && loc->double_indirect) {

		// create a new dirent
		blocknum b;
		create_dirent(&b, buf);
		loc->dirent_block = b;
		loc->direntry_idx = 0;

		// create the new indirect
		blocknum q;
		create_indirect(&q, buf);
		memset(buf, 0, BLOCKSIZE);
		dread(q.block, buf)
		indirect i;
		memcpy(&i, buf, sizeof(indirect));
		i.blocks[0] = b;

    dnode p = loc->parent_dnode;
		blocknum dub;
		memset(buf, 0, BLOCKSIZE);
		dread(p.double_indirect.block, buf);
		indirect dub_ind;
		memcpy(&dub_ind, buf, sizeof(indirect));
		dub_ind.blocks[loc->indirect_idx] = q;

		// create new direntry
    create_direntry(loc, inode, path, buf);
    p.single.blocks[loc->list_idx] = b;
    memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &p, sizeof(dnode));
    dwrite(1, buf); // TODO loc needs dnode blocknum
	}
}

// TODO
int create_direntry(file_loc *loc, blocknum b, char *path, char *buf) {
	// create new direntry with metadata
	direntry dent;
	strncpy(dent.name, path+1, MAX_FILENAME_LEN); //fix path+1 TODO
	dent.type = 0;
	dent.block = inode;

	// update the dirent with the new direntry
	dirent d;
	memset(buf, 0, BLOCKSIZE);
	dread(loc->dirent_block, buf);
	memcpy(&d, buf, sizeof(dirent));
	d.entries[loc->direntry_idx] = dent;

	// write out the new dirent
	memcpy(buf, &d, sizeof(dirent));
	dwrite(loc->dirent_block, buf);
  // TODO we could store the dirent in a loc and save a read
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
// then the function returns an invalid file_loc that points to first
// open direntry, if one exists.
//
file_loc get_file(const char *path) {
  // Start at the Dnode and search the direct, single_indirect,
  // and double_indirect

  // TODO create a cache of file_loc and do a cache check here
  
  // general-purpose temporary buffer
  char buf[BLOCKSIZE];

  // Read data into a Dnode struct
  // TODO when implementing multi-directory, this will change
  dnode parent_dnode = get_dnode(1, buf);

  // temporary file_loc buffer
  file_loc loc;
  // first open direntry slot not found
  loc.first_open_direntry_found = 0;

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
	get_inode_direct_dirent(&parent_dnode, buf, file_path, &loc);
	// If valid return the value of the file_loc
	if (loc.valid) {
		return loc;
	}

  // Check each dirent in the single_indirect to see if the file specified by
  // path exists in the filesystem.
  get_inode_single_indirect_dirent(parent_dnode.single_indirect, buf, file_path, &loc);
	// If valid return the value of the file_loc
  if (loc.valid) {
    return loc;
  }

	// Check each dirent in each indirect in the double_indirect to see if the
  // file specified by path exists in the filesystem.
  get_inode_double_indirect_dirent(parent_dnode.double_indirect, buf, file_path, &loc);
	// If valid return the value of the file_loc
  if (loc.valid) {
    return loc;
  }

  // The file specified by path does not exist in our file system.
  // Return invalid file_loc.
  return loc;
}

// Reads the dnode at the given block number into buf
dnode get_dnode(unsigned int b, char *buf) {
  // Allocate appropriate memory 
  memset(buf, 0, BLOCKSIZE);
  // Read Dnode
  dread(b, buf);
  // Put the read data into a Dnode struct
  dnode this_dnode;
  memcpy(&this_dnode, buf, sizeof(dnode));
  return this_dnode;
}

// Returns the next free block's blocknum
// If no more exist, returns a blocknum that is invalid
blocknum get_free() {
  // temporary buffer for Vcb
  char buf[BLOCKSIZE];
  vcb this_vcb = get_vcb(buf);

  // Do we have a free block
  if (this_vcb.free.valid) {
    // Get the free block structure
    memset(buf, 0, BLOCKSIZE);
    dread(this_vcb.free.block, buf);
    free_b tmpFree;
    memcpy(&tmpFree, buf, BLOCKSIZE);

    // capture free block
    blocknum next_free = this_vcb.free;   
    // Update the next free block in Vcb   
    this_vcb.free = tmpFree.next; 
    
    // Write the adjusted vcb to disk
		memset(buf, 0, BLOCKSIZE);
    memcpy(buf, &this_vcb, BLOCKSIZE);
    dwrite(0, buf);

    return next_free;
  }

  // return an invalid blocknum
  return this_vcb.free;   
}

// Returns a file_loc to the file specified by path if it exists in the
// dirent specified by blocknum b. If b is not valid, an invalid file_loc
// is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to a dirent. The behavior if b is a valid blocknum to another
// structure type is undefined.
// If find_first_open_direntry is 0, then we need not look for the next open direntry.
// Else, we look for the next open direntry and store it in the file_loc.
void get_inode_dirent(blocknum b, char *buf, const char *path, file_loc *loc) {
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
          loc->valid = 1;
          loc->dirent_block = b;
          loc->inode_block = tmp_dirent.entries[i].block;
          loc->direntry_idx = i;
          return loc;
        }
      }
      // find the first open direntry
      if (!loc->first_open_direntry_found) {
      	loc->dirent_block = b;
      	loc->direntry_idx = i;
      	// first open direntry found
      	loc->first_open_direntry_found = 1;
			}
    }
  }
	// file not found, invalid file_loc
	loc->valid = 0;
}

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within the thisDnode.direct array. If not found, an invalid
// file_loc is returned with a pointer to first open direntry if it exists.
void get_inode_direct_dirent(dnode *thisDnode, char *buf, const char *path, file_loc *loc) {
	loc->direct = 0;
	// Check direct dirents to see if the file specified by path
  // exists in the filesystem.
  for (int i = 0; i < NUM_DIRECT; i++) {
    get_inode_dirent(thisDnode->direct[i], buf, path, loc);
    // If valid return the value of the file_loc
    if (loc->valid) { // TODO make an or
      // set the direct, single_indirect, and double_indirect flags
      loc->direct = 1;
      loc->single_indirect = 0;
      loc->double_indirect = 0;
      // set the index of where the file was located in the direct array
      loc->list_idx = i;
      return;
    }
    // we found the first open direntry in the direct // FIXME repeat code?
		else if (loc->first_open_direntry_found && !loc->direct) {
			loc->direct = 1;
			loc->single_indirect = 0;
			loc->double_indirect = 0;
			loc->list_idx = i;
		}
		// we found the first invalid dirent; note it
		else if (!thisDnode->direct[i].valid && !loc->direct) {
			loc->direct = 1;
			loc->single_indirect = 0;
			loc->double_indirect = 0;
			loc->list_idx = i;
		}
  }
}

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within the indirect specified by blocknum b. If b is not valid, 
// an invalid file_loc is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to an indirect of dirents. The behavior if b is a valid blocknum 
// to another structure type is undefined.
void get_inode_single_indirect_dirent(blocknum b, char *buf, const char *path, file_loc *loc) {
	loc->single_indirect = 0;
	// check that the indirect blocknum is valid
  if (b.valid) {
    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    indirect single_indirect;
    memcpy(&single_indirect, buf, BLOCKSIZE);
    // check each dirent for the file specified by path
    for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
      get_inode_dirent(single_indirect.blocks[i], buf, path, loc);
      // will only be valid if file location exists in ith dirent
      if (loc->valid) {
        // set the direct, single_indirect, and double_indirect flags
        loc->direct = 0;
        loc->single_indirect = 1;
        loc->double_indirect = 0;
        // set the index of the dirent of where the file was located in the
        // single_indirect.blocks array
        loc->list_idx = i;
        return;
      }
      // if the first open slot has been found, and the direct flag
      // is not toggled, then we found in the single indirect
			else if (loc->first_open_direntry_found && !loc->direct && !loc->single_indirect) {
      	loc->single_indirect = 1;
      	loc->double_indirect = 0;
      	loc->list_idx = i;
		  }
		  // we found the first invalid dirent; note it
		  else if (!single_indirect.blocks[i].valid && !loc->direct && !loc->single_indirect) {
	  		loc->single_indirect = 1;
  			loc->double_indirect = 0;
	  		loc->list_idx = i;
		  }
    }
  }
}

// Returns a file_loc to the file specified by path if it exists in the
// any dirent within any indirect within the indirect specified by blocknum b.
// If b is not valid, an invalid file_loc is returned.
// If b is valid, it is the caller's responsibility to ensure that b is a 
// blocknum to an indirect of indirects of dirents. The behavior if b is a 
// valid blocknum to another structure type is undefined.
void get_inode_double_indirect_dirent(blocknum b, char *buf, const char *path, file_loc *loc) {
	loc->double_indirect = 0;
	// check that the indirect blocknum is valid
  if (b.valid) {
    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    indirect double_indirect;
    memcpy(&double_indirect, buf, BLOCKSIZE);
    // check each indirect for the file specified by path
    for (int i = 0; i < NUM_INDIRECT_BLOCKS; i++) {
      get_inode_single_indirect_dirent(double_indirect.blocks[i], buf, path, loc);
      // will only be valid if file location exists in ith indirect
      if (loc->valid) {
        // set the direct, single_indirect, and double_indirect flags
        loc->direct = 0;
        loc->single_indirect = 0;
        loc->double_indirect = 1;
        // set the index of the indirect where the file was located in the 
        // double_indirect.blocks array
        loc->indirect_idx = i;
        return;
      }
      // if the first open slot has been found, and the direct and single_indirect flags
      // are not toggled, then we found in the double indirect
			else if (loc->first_open_direntry_found && !loc->direct && !loc->single_indirect && !loc->double_indirect) {
				loc->double_indirect = 1;
				loc->indirect_idx = i;
			}
		  // we found the first invalid indirect; note it
			else if (!double_indirect.blocks[i].valid && !loc->direct && !loc->single_indirect && !loc->double_indirect) {
				loc->double_indirect = 1;
				loc->indirect_idx = i;
			}
    }
  }
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
  return 0;
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

  return 0;
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
  return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{
  fprintf(stderr, "\nIN vfs_utimens\n");
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

