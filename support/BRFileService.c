//
//  BRFileService.c
//  Core
//
//  Created by Richard Evers on 1/4/19.
//  Copyright © 2019 breadwallet. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <pthread.h>

// neither help
#include "BRSet.h"
#include "BRArray.h"

#include "BRFileService.h"

/////////////////////////////////////////////////////////////////////////
// private function definitions
/////////////////////////////////////////////////////////////////////////

// returns element (0-n) that matches type, or -1 on error
int findType(BRFileService fs, const char *type);

int createDirectory(char *path);

// remove files within dirpath
void removeFiles(char *dirpath);

/////////////////////////////////////////////////////////////////////////
// private functions
/////////////////////////////////////////////////////////////////////////

// returns element (0-n) that matches type, or -1 on error
int findType(BRFileService fs, const char *type)
{
    // get functions based on passed type
    uint8_t i, x;
    for ( i = x = 0; x == 0 && i < fs->count_elements; i++) {
        if (strcmp(fs->type[i], type) == 0) {
            ++x;
        }
    }
    return (x) ? i : -1;
}

int createDirectory(char *path)
{
    struct stat stbuf;
    int status = FS_ERR_CLEAR;
    
    // create the base directory if it does not exist
    if (stat(path, &stbuf) == -1) {
        // creation failed
        if (mkdir(path, S_IRWXU) < 0) {
            status = FS_ERR_DIR_CREATE;
        }
    }
    
    return status;
}

// remove files within dirpath
void removeFiles(char *dirpath)
{
    char filepath[1024];
    struct dirent *nextfile;
    int status = 0;
    DIR *directory = opendir(dirpath);
    
    if (directory != NULL) {
        while ((nextfile = readdir(directory)) != NULL) {
            if (strcmp(nextfile->d_name, "..") != 0 && strcmp(nextfile->d_name, ".") != 0) {
                sprintf(filepath, "%s/%s", dirpath, nextfile->d_name);
                status = unlink(filepath);
            }
        }
        
        closedir(directory);
    }
}

/////////////////////////////////////////////////////////////////////////
// public functions
/////////////////////////////////////////////////////////////////////////

// return available storage
extern uint64_t
fileServiceGetFreeStorage(BRFileService fs)
{
    struct statvfs stat;
    uint64_t free_storage = 0;
    
    if (statvfs(fs->baseDirectory, &stat) == 0) {
        free_storage = stat.f_bsize * stat.f_bavail;
    }
    
    return free_storage;
}

// Allocate memory for BRFileService, create work space, populate BRFileService for return
// note that the work space is specific to the network and currency
extern BRFileService
fileServiceCreate(const char *baseDirectory, const char *network, const char *currency)
{
    uint16_t sz_baseDirectory = (uint16_t)strlen(baseDirectory);
    uint16_t sz_network       = (uint16_t)strlen(network);
    uint16_t sz_currency      = (uint16_t)strlen(currency);

    if (sz_baseDirectory >= FSR_LENGTH_BASEDIR || sz_network  >= FSR_LENGTH_NETWORK || sz_currency >= FSR_LENGTH_CURRENCY) {
        return NULL;
    }
    
    // allocate memory and initialize to 0
    BRFileService fs = (BRFileService) calloc (1, sizeof (struct BRFileServiceRecord));
    
    if (NULL == fs) {
        return NULL;
    }

    strcpy(fs->baseDirectory, baseDirectory);
    strcpy(fs->network, network);
    strcpy(fs->currency, currency);

    fs->status = FS_ERR_CLEAR;
    
    char path[sz_baseDirectory + sz_network + sz_currency + 10];
    
    sprintf(path, "%s/%s", baseDirectory, network);
    
    int status = createDirectory(path);

    if ( status != FS_ERR_DIR_CREATE) {
        sprintf(path, "%s/%s/%s", baseDirectory, network, currency);
        status = createDirectory(path);
    }

    fs->current_element = 0;

    return fs;
}

// deallocate BRFileService
extern void
fileServiceRelease (BRFileService fs)
{
    if (NULL != fs) {
        free(fs);
    }
}

// call filereader to load results of the current version
extern int
fileServiceLoad (BRFileService fs,
                 BRSet *results,
                 const char *type)   /* blocks, peers, transactions, logs, ... */
{
    int fid = FS_MUTEX_FILESERVICELOAD;
    int rc = pthread_mutex_lock(&BRFileServiceMutex[fid]);

    // default to fail
    int status = __LINE__;
    char dirpath[256];

    if (strlen(type) < FSR_LENGTH_TYPE) {
        // returns element (0-n) that matches type, or -1 on error
        int id = findType(fs, type);
    
        if (id >= 0) {
            sprintf(dirpath, "%s/%s/%s/%s", fs->baseDirectory, fs->network, fs->currency, fs->type[id]);

            // note that version is not (yet) used 17-Jan-2019
            // use passed versiuon or current version as needed later
            status = fs->FileReader[id](results, dirpath, fs->version[fs->current_element]);
        }
    }

    rc = pthread_mutex_unlock(&BRFileServiceMutex[fid]);

    return status;
}

// call filewriter to save entity
extern int
fileServiceSave (BRFileService fs,
                 void *entity,      /* BRMerkleBlock*, BRTransaction, BREthereumTransaction, ... */
                 const char *type)  /* block, peers, transactions, logs, ... */
{
    int fid = FS_MUTEX_FILESERVICESAVE;
    int rc = pthread_mutex_lock(&BRFileServiceMutex[fid]);

    // default to fail
    int status = __LINE__;
    char dirpath[256];

    if (strlen(type) < FSR_LENGTH_TYPE) {
        // returns element (0-n) that matches type, or -1 on error
        int id = findType(fs, type);

        if (id >= 0) {
            sprintf(dirpath, "%s/%s/%s/%s", fs->baseDirectory, fs->network, fs->currency, fs->type[id]);
            
            // always save with the current version within the file header
            status = fs->FileWriter[id](entity, dirpath, fs->version[fs->current_element]);
        }
    }

    rc = pthread_mutex_unlock(&BRFileServiceMutex[fid]);

    return status;
}

// erases "type" of files from the file system, and associated type folder
extern void
fileServiceClear (BRFileService fs,
                  const char *type)
{
    int fid = FS_MUTEX_FILESERVICECLEAR;
    int rc = pthread_mutex_lock(&BRFileServiceMutex[fid]);

    char dirpath[256];

    // remove files in matching type folder, then remove the type folder
    if (strlen(type) < FSR_LENGTH_TYPE) {
        for (uint8_t id = 0; id < fs->count_elements; id++) {
            if (strcmp(fs->type[id], type) == 0) {

                sprintf(dirpath, "%s/%s/%s/%s", fs->baseDirectory, fs->network, fs->currency, fs->type[id]);

                // remove files within dirpath
                removeFiles(dirpath);

                // remove type folder
                rmdir(dirpath);
            }
        }
    }
    
    rc = pthread_mutex_unlock(&BRFileServiceMutex[fid]);
}

// erases all files from the file system
extern void
fileServiceClearAll (BRFileService fs)
{
    char dirpath[256];
    struct stat stbuf;

    // first pass will remove all type files in the tree
    for (uint8_t id = 0; id < fs->count_elements; id++) {
        sprintf(dirpath, "%s/%s/%s/%s", fs->baseDirectory, fs->network, fs->currency, fs->type[id]);

        // remove files within dirpath
        removeFiles(dirpath);

        // remove type folder
        rmdir(dirpath);
    }

    // second pass will remove all currency directories in tree
    for (uint8_t id = 0; id < fs->count_elements; id++) {
        sprintf(dirpath, "%s/%s/%s", fs->baseDirectory, fs->network, fs->currency);
        
        // remove directory if it exists
        if (stat(dirpath, &stbuf) == 0) {
            rmdir(dirpath);
        }
    }

    // final pass will remove all network directories in tree
    for (uint8_t id = 0; id < fs->count_elements; id++) {
        sprintf(dirpath, "%s/%s", fs->baseDirectory, fs->network);
        
        // remove directory if it exists
        if (stat(dirpath, &stbuf) == 0) {
            rmdir(dirpath);
        }
    }
}

// adds passed parms into BRFileService fs
extern int
fileServiceDefineType (BRFileService fs,
                       const char *type,
                       uint16_t version,
                       int (*reader)(BRSet *results, char *path, uint16_t version),
                       int (*writer)(void *entity, char *path, uint16_t version))
{
    int fid = FS_MUTEX_FILESERVICEDEFINETYPE;
    int rc = pthread_mutex_lock(&BRFileServiceMutex[fid]);

    // default to failure
    int status = __LINE__;

    if (strlen(type) < FSR_LENGTH_TYPE && fs->count_elements < FSR_ELEMENTS_MAXIMUM) {
        if (fs->count_elements < FSR_ELEMENTS_MAXIMUM ) {
            strcpy(fs->type[fs->count_elements], type);
            fs->version[fs->count_elements] = version;
            fs->FileReader[fs->count_elements] = reader;
            fs->FileWriter[fs->count_elements] = writer;
            fs->count_elements++;

            if (fs->count_elements < FSR_ELEMENTS_MAXIMUM ) {
                status = 0;
            }
        }
    }

    rc = pthread_mutex_unlock(&BRFileServiceMutex[fid]);

    return status;
}

// locates version within BRFileService fs, sets current_element
extern int
fileServiceDefineCurrentVersion (BRFileService fs,
                                 const char *type,
                                 uint16_t version) 
{
    int fid = FS_MUTEX_FILESERVICEDEFINECURRENTVERSION;
    int rc = pthread_mutex_lock(&BRFileServiceMutex[fid]);

    int status = __LINE__;
    
    if (strlen(type) < FSR_LENGTH_TYPE) {
        uint8_t i, x;
        for ( i = x = 0; x == 0 && i < fs->count_elements; i++) {
            if (fs->version[i] == version) {
                ++x;
                fs->current_element = i;
                status = 0;
            }
        }
    }

    rc = pthread_mutex_unlock(&BRFileServiceMutex[fid]);

    return status;
}
