//
//  BRFileService.h
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

#ifndef BRFileService_h
#define BRFileService_h

#include <pthread.h>

// #if defined (TASK_DESCRIPTION)
//Both Bitcoin and Ethereum Wallet Managers include the ability to save and load peers, block,
//transactions and logs (for Ethereum) to the file system.  But, they both implement the file
//operations independently.  Pull out the implementation into BRFileService.
//
// Allow each WalletManager (Bitcoin, Bitcash, Ethereum) to create their own BRFileService (storing
// stuff in a subdirectory specific to the manager+network+whatever for a given base path).  Allow
// each WalletManager to further define 'persistent types' (peers, blocks, etc) saved.  Allow for
// a versioning system (at least on read; everything gets written/saved w/ the latest version).
// Allow an upgrade path from existing IOS/Andriod Sqlite3 databases.
//
// Candidate interface:

//{code:C}
typedef struct BRFileServiceRecord *BRFileService;

typedef enum {
    FS_ERR_CLEAR,       // creation went perfectly
    FS_ERR_EXISTS,      // directory exists
    FS_ERR_DIR_CREATE   // error creating the directory
} BRFileServiceError;

// adjust buffer sizes as needed
enum {
    FSR_LENGTH_BASEDIR   = 100,
    FSR_LENGTH_NETWORK   =  25,
    FSR_LENGTH_CURRENCY  =  25,
    FSR_LENGTH_TYPE      =  25,
    FSR_ELEMENTS_MAXIMUM =  10
} BRFileServiceLimit;

enum {
    FS_MUTEX_FILESERVICELOAD,
    FS_MUTEX_FILESERVICESAVE,
    FS_MUTEX_FILESERVICECLEAR,
    FS_MUTEX_FILESERVICEDEFINETYPE,
    FS_MUTEX_FILESERVICEDEFINECURRENTVERSION,
    FS_MUTEX_ELEMENTS
} BRFileServiceTheadSafeFunctions;

// PTHREAD_MUTEX_INITIALIZER or PTHREAD_RECURSIVE_MUTEX_INITIALIZER
static pthread_mutex_t BRFileServiceMutex[FS_MUTEX_ELEMENTS] = {
    PTHREAD_MUTEX_INITIALIZER, // FS_MUTEX_FILESERVICELOAD
    PTHREAD_MUTEX_INITIALIZER, // FS_MUTEX_FILESERVICESAVE
    PTHREAD_MUTEX_INITIALIZER, // FS_MUTEX_FILESERVICECLEAR
    PTHREAD_MUTEX_INITIALIZER, // FS_MUTEX_FILESERVICEDEFINETYPE
    PTHREAD_MUTEX_INITIALIZER // FS_MUTEX_FILESERVICEDEFINECURRENTVERSION
};

typedef int (*_FileReader)(BRSet *results, char *path, uint16_t version);
typedef int (*_FileWriter)(void *entity, char *path, uint16_t version);

struct BRFileServiceRecord {
    uint32_t status;

    char baseDirectory[FSR_LENGTH_BASEDIR];
    char network[FSR_LENGTH_NETWORK];
    char currency[FSR_LENGTH_CURRENCY];

    // used when current revision is set
    uint8_t current_element;

    uint8_t count_elements;

    char type[FSR_LENGTH_TYPE][FSR_ELEMENTS_MAXIMUM];
    uint16_t version[FSR_ELEMENTS_MAXIMUM];

    _FileReader FileReader[FSR_ELEMENTS_MAXIMUM];
    _FileWriter FileWriter[FSR_ELEMENTS_MAXIMUM];
};

// return available storage
extern uint64_t
fileServiceGetFreeStorage(BRFileService fs);

/* args (w/ 'currency' as BTC, BCH, ETH and 'network' and ???) */
extern BRFileService
fileServiceCreate(const char *baseDirectory, const char *network, const char *currency);

extern void
fileServiceRelease (BRFileService fs);

extern int
fileServiceLoad (BRFileService fs,
                 BRSet *results,
                 const char *type);        /* blocks, peers, transactions, logs, ... */

extern int
fileServiceSave (BRFileService fs,
                 void *entity,     /* BRMerkleBlock*, BRTransaction, BREthereumTransaction, ... */
                 const char *type);  /* block, peers, transactions, logs, ... */

extern void
fileServiceClear (BRFileService fs,
                  const char *type);

extern void
fileServiceClearAll (BRFileService fs);

extern int
fileServiceDefineType (BRFileService fs,
                       const char *type,
                       uint16_t version,
                       int (*reader)(BRSet *results, char *path, uint16_t version),
                       int (*writer)(void *entity, char *path, uint16_t version));

extern int
fileServiceDefineCurrentVersion (BRFileService fs,
                                 const char *type,
                                 uint16_t version);
//{code}
#if defined (TASK_DESCRIPTION)
//Example use:

//{code:C}

//
// In BRWalletManager.c
// ...
manager->fileService = fileServiceCreate (/* BTC or BCH, Mainnet or Testnet, ...*/);
fileServiceDefineType (manager->fileService,
                       "transaction",
                       SomeVersion
                       BRTransactionFileReaderForSomeVersion,     /* TBD */
                       BRTransactionFileWriter);    /* TBD */
// ... transaction, other versions ...
fileServiceDefineCurrentVersion (manager->fileService, "transaction", CurrentVersion);

// ... block ...
// ... peer ...

BRSetOf(BRTransaction) transactions;
manager->transactions = BRSetNew (/* ... */);

fileServiceLoad (manager->fileService,
                 manager->transactions,
                 "transaction");


manager->wallet = BRWalletNew (transactionsAsArray, array_count(transactionsAsArray), mpk, fork);
// ...

//
// In _BRWalletManagerTxAdded.
//
static void
_BRWalletManagerTxAdded (void *info, BRTransaction *tx) {
    BRWalletManager manager = (BRWalletManager) info;
    fileServiceSave (manager->fs,
                     "transaction",
                     tx);
    // ...
}
//{code}
#endif /* defined (TASK_DESCRIPTION) */

#endif /* BRFileService_h */
