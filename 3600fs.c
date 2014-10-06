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

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

const int MAGICNUMBER = 184901;

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

  /* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
           AND LOAD ANY DATA STRUCTURES INTO MEMORY */

  vcb myVcb;

  char *tmp = (char*) malloc(BLOCKSIZE);
  //memcpy(tmp, &vcbBlock, sizeof(vcb));

  // Write to block 0
  dread(0, tmp);

  memcpy(&myVcb, tmp, sizeof(vcb));

  if (!(myVcb.magic == MAGICNUMBER)) {
    fprintf(stdout, "WRONG FILE SYSTEM!!!!!!!!\n");
  }
  else {
    fprintf(stdout, "Correct Magic Number!!\n");
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
  fprintf(stderr, "vfs_getattr called\n");

  // Do not mess with this code 
  stbuf->st_nlink = 1; // hard links
  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;

  /* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
  
  /*
  if (The path represents the root directory)
    stbuf->st_mode  = 0777 | S_IFDIR;
  else 
    stbuf->st_mode  = <<file mode>> | S_IFREG;

  stbuf->st_uid     = // file uid
  stbuf->st_gid     = // file gid
  stbuf->st_atime   = // access time 
  stbuf->st_mtime   = // modify time
  stbuf->st_ctime   = // create time
  stbuf->st_size    = // file size
  stbuf->st_blocks  = // file size in blocks
    */

  return 0;
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

    return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    
    // Allocate appropriate memory 
    char buf[BLOCKSIZE];
    // Get the dnode
    dnode thisDnode = get_dnode(1, buf);

    blocknum file;
    // blocknum of Inode of file
    // TODO Must free file...
    file = get_file(path);
    // See if the file exists
    if (file.valid) {
        // File exists...
        return -1;
    }
    
    // Get next free block
    blocknum this_inode = get_free();
    
    if (this_inode.valid) {

        // Set up Inode
        init_inode(this_inode, buf, mode, fi); 

        // Look for available Direntry location to put this new file
        // Loop through Dnode -> direct
        for (int i = 0; i < 20; i++) {// TODO fix hardcoded values
            if (create_inode_dirent(thisDnode.direct[i], this_inode, path, buf)) {
                return 0;
            } 
        }
   
        // Loop through Dnode -> single_indirect
        if (create_inode_single_indirect_dirent(thisDnode.single_indirect, this_inode, path, buf)) {
            return 0;
        }
        // Loop through Dnode -> double_indirect
        else if (create_inode_double_indirect_dirent(thisDnode.double_indirect, this_inode, path, buf)) {
            return 0;
        }
        else {  // NOTHING AVAILABLE
            return -1;
        }
    }

    // TODO: Out of memory message
    return -1;
}
// Initialize inode metadata to the given inode blocknum
void init_inode(blocknum b, char *buf, mode_t mode, struct fuse_file_info *fi) {
    // TODO: FILL IN
}

// Initialize the given blocknum to an indirect
// returns 0 if there is an error in doing so
int create_indirect(blocknum b, char *buf) {
    // make sure the given block is valid
    if (b.valid) {
        // put an indirect structure in the given block
        memset(buf, 0, BLOCKSIZE);
        indirect new_indirect;
        memcpy(buf, &new_indirect, sizeof(indirect));
        dwrite(b.block, buf);
        return 1;
    }
    // Block was not valid, error
    return 0;
}

// TODO: Fill in all of these methods
// Create a file at the next open direntry in this dirent
// returns 0 if there are no open direntries
int create_inode_dirent(blocknum dirent, blocknum inode, const char *path, char *buf) {
    // TODO: this is where the magic happens...
    return 0;
}

// Create a file at the next open direntry in this single_indirect
// returns 0 if there are no open direntries
int create_inode_single_indirect_dirent(blocknum single_indirect, blocknum inode, const char *path, char *buf) {
    // All other blocks free, now this must be valid
    single_indirect.valid = 1;

    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    indirect single_indirect;
    memcpy(&single_indirect, buf, BLOCKSIZE);

    for (int i = 0; i < 128; i++) { // TODO fix hardcoded values
        // check if valid, if not get one, assign to i, call function
        if (! (single_indirect.blocks[i].valid)) {
            // create a single indirect for the ith block
            // get the next free block
            blocknum temp_free = get_free();
            // try to create an indirect
            if (! (create_indirect(temp_free, buf))) {
                // error with create_indirect
                return 0;
            } 
            // otherwise we were able to create an indirect, set that to the ith block
            single_indirect.blocks[i] = temp_free;
        }

        // try to create a block at i
        if (create_inode_dirent(double_indirect.blocks[i], inode, path, buf)) {
            return 1;
        }
    }
    // No space available
    return 0;
}

// Create a file at the next open direntry in this double_indirect
// returns 0 if there are no open direntries
int create_inode_double_indirect_dirent(blocknum double_indirect, blocknum inode, const char *path, char *buf) {
    // All other blocks free, now this must be valid
    double_indirect.valid = 1;

    memset(buf, 0, BLOCKSIZE);
    dread(b.block, buf);
    indirect double_indirect;
    memcpy(&double_indirect, buf, BLOCKSIZE);

    for (int i = 0; i < 128; i++) { // TODO fix hardcoded values
        // check if valid, if not get one, assign to i, call function
        if (! (double_indirect.blocks[i].valid)) {
            // create a single indirect for the ith block
            // get the next free block
            blocknum temp_free = get_free();
            // try to create an indirect
            if (! (create_indirect(temp_free, buf))) {
                // error with create_indirect
                return 0;
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


// TODO put this in a logical place within the code
vcb get_vcb(char *buf) {
    // Allocate appropriate memory 
    memset(buf, 0, BLOCKSIZE);
    // Read Vcb
    dread(0, buf);
    // Put the read data into a Dnode struct
    vcb thisVcb;
    memcpy(&thisVcb, buf, sizeof(vcb));
    return thisVcb;
}


// Returns the blocknum of the given path's Inode if it exists.
// If the file does not exist, then the function returns a
// non-valid blocknum.
// When done with the result blocknum, be sure to call free().
blocknum get_file(const char *path) {
    // Start at the Dnode and search the dirent, single_indirent, and double_indirent

    // general-purpose temporary buffer
    // Free this before we return
    char *buf = (char *) malloc(BLOCKSIZE);

    // Read data into a Dnode struct
    dnode thisDnode = get_dnode(1, buf); // TODO recursive dirs not 1

    // temporary blocknum buffer
    blocknum temp = (blocknum) malloc(sizeof(blocknum));

    // Check dirent for the 'path'
    for (int i = 0; i < 20; i++) {// TODO fix hardcoded values
        // TODO: return blocknum, check for validity
        temp = get_inode_dirent(thisDnode.direct[i], buf, path); // FIXME &tmpBuff?

        // If valid return temp(blocknum), else keep looking
        if (temp.valid) {
            free(buf);
            return temp;
        }
    }

    temp = get_inode_single_indirect_dirent(thisDnode.single_indirect, buf, path);
    // If valid return temp(blocknum), else keep looking
    if (temp.valid) {
        free(buf);
        return temp;
    }

    temp = get_inode_double_indirect_dirent(thisDnode.double_indirect, buf, path);
    // If valid return temp(blocknum), else file does not exist
    if (temp.valid) {
        free(buf);
        return temp;
    }

    // Will only get here if we haven't found it
    // temp blocknum is currently invalid, return that...
    free(buf);
    return temp;     
}

// TODO
dnode get_dnode(unsigned int b, char *buf) {
    // Allocate appropriate memory 
    memset(buf, 0, BLOCKSIZE);
    // Read Dnode
    dread(b, buf);
    // Put the read data into a Dnode struct
    dnode thisDnode;
    memcpy(&thisDnode, buf, sizeof(dnode));
    return thisDnode;
}

// Returns the next free block's blocknum
// If no more exist, returns a blocknum that is invalid
blocknum get_free() {
    // temporary buffer for Vcb
    char buf[BLOCKSIZE];
    vcb thisVcb = get_vcb(buf);

    // Do we have a free block
    if (vcb.free.valid) {
        // capture free block
        blocknum next_free = vcb.free;   
        // Update the next free block in Vcb   
        vcb.free = vcb.free.next;        
        return next_free;
    }

    // return an invalid blocknum
    return vcb.free;   
}

// Returns a blocknum to the file's Inode, if it exists in the 
// dirent specified by b.
blocknum get_inode_dirent(blocknum b, char *buf, const char *path) {
    if (b.valid) {
        memset(buf, 0, BLOCKSIZE);
        dread(b.block, buf);
        dirent tmpDirent;
        memcpy(&tmpDirent, buf, BLOCKSIZE);
        for (int i = 0; i < 16; i++) { // TODO fix hardcoded values
            if (tmpDirent.entries[i].block.valid) {
                if (strcmp(path, tmpDirent.entries[i].name) == 0) {
                    printf("The given file exists already\n");
                    // return the blocknum where this file lives
                    return tmpDirent.entries[i].block;
                }
            }
        }
    }

    // return a blocknum that is not valid (did not find file)
    blocknum inval;
    inval.valid = 0;
    return inval;
}

// Check every entry of a single indirect of dirents. If the file path exists
// in any of the dirents, return the blocknum to that file's Inode.
blocknum get_inode_single_indirect_dirent(blocknum b, char *buf, const char *path) {
    blocknum temp;
    if (b.valid) {
        memset(buf, 0, BLOCKSIZE);
        dread(b.block, buf);
        indirect single_indirect;
        memcpy(&single_indirect, buf, BLOCKSIZE);
        for (int i = 0; i < 128; i++) { // TODO fix hardcoded values
            temp = get_inode_dirent(single_indirect.blocks[i], buf, path);
            // will only be valid if file found, that is returned blocknum
            if (temp.valid) {
                return temp;
            }
        }
    }
    
    // return a blocknum that is not valid (did not find file)
    temp.valid = 0;
    return temp;

}

// Checks every indirect of a double indirect of dirents. For each indirect,
// check every entry of a single indirect of dirents. If the file path exists
// in any of the dirents, return the blocknum to that file's Inode.
blocknum get_inode_double_indirect_dirent(blocknum b, char *buf, const char *path) {
    blocknum temp;
    if (b.valid) {
        memset(buf, 0, BLOCKSIZE);
        dread(b.block, buf);
        indirect double_indirect;
        memcpy(&double_indirect, buf, BLOCKSIZE);
        for (int i = 0; i < 128; i++) { // TODO fix hardcoded values
            temp = get_inode_single_indirect_dirent(double_indirect.blocks[i], buf, path);
            // will only be valid if file found, that is returned blocknum
            if (temp.valid) {
                return temp;
            }
        }
    }

    // return a blocknum that is not valid (did not find file)
    temp.valid = 0;
    return temp;
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

  /* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
           AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */
	// check if the file exists
	// free DB blocks, free INode, free Dnode/dirent --> double check this is how FUSE works
   
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
	// get file -> reassign
    return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{

    return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{

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
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
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

