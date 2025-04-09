/*
 * fs_param.h
 *
 * File server parameters (used by both clients and server).
 */

#pragma once

/*
 * File system parameters
 */

/*
 * Size of a disk block (in bytes)
 */
static constexpr unsigned int FS_BLOCKSIZE = 512;

/*
 * Maximum # of data blocks in a file or directory.  Computed so that
 * an inode is exactly 1 block.
 */
static constexpr unsigned int FS_MAXFILEBLOCKS = 124;

/*
 * Maximum length of a file or directory name, not including the null terminator
 */
static constexpr unsigned int FS_MAXFILENAME = 59;

/*
 * Maximum length of a full pathname, not including the null terminator
 */
static constexpr unsigned int FS_MAXPATHNAME = 128;

/*
 * Maximum length of a user name, not including the null terminator
 */
static constexpr unsigned int FS_MAXUSERNAME = 10;
