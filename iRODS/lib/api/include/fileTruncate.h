/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* fileTruncate.h - This file may be generated by a program or script
 */

#ifndef FILE_TRUNCATE_H__
#define FILE_TRUNCATE_H__

/* This is a Internal I/O API call */

#include "rcConnect.h"
#include "fileOpen.h"


#if defined(RODS_SERVER)
#define RS_FILE_TRUNCATE rsFileTruncate
#include "rodsConnect.h"
/* prototype for the server handler */
int
rsFileTruncate( rsComm_t *rsComm, fileOpenInp_t *fileTruncateInp );
int
_rsFileTruncate( rsComm_t *rsComm, fileOpenInp_t *fileTruncateInp );
int
remoteFileTruncate( rsComm_t *rsComm, fileOpenInp_t *fileTruncateInp,
                    rodsServerHost_t *rodsServerHost );
#else
#define RS_FILE_TRUNCATE NULL
#endif

/* prototype for the client call */
#ifdef __cplusplus
extern "C" {
#endif
int
rcFileTruncate( rcComm_t *conn, fileOpenInp_t *fileTruncateInp );
#ifdef __cplusplus
}
#endif

#endif	// FILE_TRUNCATE_H__
