Project 2
=========

Approach:


TODO:

Objects 
  - Get definite size (check sizeof() of all structs are BLOCKSIZE)
  - Double check size
  - How should we handle "type" in DIRENT
  - Change pointers with stars to & in method signatures
  - Limit line length; line length = 80
  - Do not malloc buf, change those that do
  - Make sure tab indentation is consistent; indent = 4 spaces
  - Ensure that if conditions that are negated (!condition) work as expected (!1 != 0)
3600mkfs


3600fs (Milestone 2)
  - vfs_mount
  - vfs_create
  - vfs_delete
  - vfs_readdir
  - vfs_getattr
  - Create and read FCBs
