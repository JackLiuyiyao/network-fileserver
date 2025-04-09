/*
 * fs_client.h
 *
 * Header file for clients of the file server.
 */

#pragma once

#if !defined(__cplusplus) || __cplusplus < 201700L
#error Please configure your compiler to use C++17 or C++20
#endif

#include <sys/types.h>
#include <netinet/in.h>

#include "fs_param.h"

/*
 * Initialize the client library.
 * The location of the file server is specified by (hostname, port).
 *
 * fs_clientinit returns 0 on success, -1 on failure.
 */
int fs_clientinit(const char *hostname, uint16_t port);

/*
 * Read a block of data from the file specified by pathname.  offset specifies
 * the block to be read.  buf specifies where to store the data read from the
 * file.
 *
 * fs_readblock returns 0 on success, -1 on failure.  Possible failures include:
 *     pathname is invalid
 *     pathname does not exist, is not a file, or is not owned by username
 *     offset is out of range
 *     username is invalid
 *
 * fs_readblock is thread safe.
 */
int fs_readblock(const char *username, const char *pathname,
                        unsigned int offset, void *buf);

/*
 * Write a block of data to the file specified by pathname.  offset specifies
 * the block to be written.  offset may refer to an existing block in the file,
 * or it may refer to the block immediately after the current end of the file
 * (this is how files grow in size).  buf specifies where to get the data
 * that will be written to the file.
 *
 * fs_writeblock returns 0 on success, -1 on failure.  Possible failures include:
 *     pathname is invalid
 *     pathname does not exist, is not a file, or is not owned by username
 *     offset is out of range
 *     the disk or file is out of space
 *     username is invalid
 *
 * fs_writeblock is thread safe.
 */
int fs_writeblock(const char *username, const char *pathname,
                         unsigned int offset, const void *buf);

/*
 * Create a new file or directory "pathname".  Type can be 'f' (file) or 'd'
 * (directory).
 *
 * fs_create returns 0 on success, -1 on failure.  Possible failures include:
 *     pathname is invalid
 *     pathname is in a directory that does not exist
 *     pathname is in a directory not owned by username
 *     pathname already exists
 *     the disk or directory containing pathname is out of space
 *     invalid type
 *     username is invalid
 *
 * fs_create is thread safe.
 */
int fs_create(const char *username, const char *pathname, char type);

/*
 * Delete the existing file or directory "pathname".
 *
 * fs_delete returns 0 on success, -1 on failure.  Possible failures include:
 *     pathname is invalid
 *     pathname is not owned by username
 *     pathname is in a directory not owned by username
 *     pathname does not exist
 *     username is invalid
 *
 * fs_delete is thread safe.
 */
int fs_delete(const char *username, const char *pathname);
