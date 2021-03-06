/**
 * @file nre.systemMS.cpp
 *
 */

/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
#include <string.h>
#include <time.h>
#include "reGlobalsExtern.hpp"
#include "icatHighLevelRoutines.hpp"
#include "rcMisc.h"
#include "execMyRule.h"
#include "region.h"
#include "rules.hpp"
#include "conversion.hpp"
#include "irods_plugin_name_generator.hpp"
#include "irods_load_plugin.hpp"
#include "reFuncDefs.hpp"
#include "sockComm.h"
#include "irods_server_properties.hpp"
#include "irods_log.hpp"

int
fillSubmitConditions( char *action, char *inDelayCondition, bytesBuf_t *packedReiAndArgBBuf,
                      ruleExecSubmitInp_t *ruleSubmitInfo,  ruleExecInfo_t *rei );


#define evaluateExpression(expr, eaVal, rei) \
    computeExpression(expr, NULL,rei, 0, eaVal)

/**
 * \cond oldruleengine
 * \fn delayExec(msParam_t *mPA, msParam_t *mPB, msParam_t *mPC, ruleExecInfo_t *rei)
 *
 * \brief  Execute a set of operations later when certain conditions are met. Can be used to perform
 * periodic operations also.
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \note  This microservice is a set of statements that will be delayed in execution until delayCondition
 *    is true. The condition also supports repeating of the body until success or until
 *    some other condition is satisfied. This microservice takes the delayCondition as
 *    the first parameter, the microservice/rule chain that needs to be executed as
 *    the second parameter, and the recovery-microservice chain as the third parameter.
 *    The delayCondition is given as a tagged condition. In this case, there are two
 *    conditions that are specified.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] mPA - mPA is a msParam of type STR_MS_T which is a delayCondition about when to execute the body.
 *  		   These are tagged with the following tags:
 * 			\li EA - execAddress - host where the delayed execution needs to be performed
 *			\li ET - execTime - absolute time when it needs to be performed.
 * 			\li PLUSET - relExeTime - relative to current time when it needs to execute
 * 			\li EF - execFreq - frequency (in time widths) it needs to be performed.
 * \code
 * 			The format for EF is quite rich:
 * 			The EF value is of the format:
 *			nnnnU <directive>  where
 *			nnnn is a number, and
 *     			U is the unit of the number (s-sec,m-min,h-hour,d-day,y-year),
 * 			The <directive> can be for the form:
 *     			<empty-directive>    - equal to REPEAT FOR EVER
 *     			REPEAT FOR EVER
 *     			REPEAT UNTIL SUCCESS
 *     			REPEAT nnnn TIMES    - where nnnn is an integer
 *     			REPEAT UNTIL <time>  - where <time> is of the time format supported by checkDateFormat function below.
 *     			REPEAT UNTIL SUCCESS OR UNTIL <time>
 *     			REPEAT UNTIL SUCCESS OR nnnn TIMES
 *     			DOUBLE FOR EVER
 *     			DOUBLE UNTIL SUCCESS   - delay is doubled every time.
 *     			DOUBLE nnnn TIMES
 *     			DOUBLE UNTIL <time>
 *     			DOUBLE UNTIL SUCCESS OR UNTIL <time>
 *     			DOUBLE UNTIL SUCCESS OR nnnn TIMES
 *     			DOUBLE UNTIL SUCCESS UPTO <time>
 *			Example: <PLUSET>1m</PLUSET><EF>10m<//EF>  means start after 1 minute and repeat every 20 minutes
 * \endcode
 * \param[in] mPB - mPB is a msParam of type STR_MS_T which is a body of microservices/rules within backticks (``)
 * \param[in] mPC - mPC is a msParam of type STR_MS_T which is a recoveryBody of microservices/rules within backticks (``)
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa  none
 * \endcond
**/
int delayExec( msParam_t *mPA, msParam_t *mPB, msParam_t *mPC, ruleExecInfo_t *rei ) {
    int i;
    char actionCall[MAX_ACTION_SIZE];
    char recoveryActionCall[MAX_ACTION_SIZE];
    char delayCondition[MAX_ACTION_SIZE];

    rstrcpy( delayCondition, ( char * ) mPA->inOutStruct, MAX_ACTION_SIZE );
    rstrcpy( actionCall, ( char * ) mPB->inOutStruct, MAX_ACTION_SIZE );
    rstrcpy( recoveryActionCall, ( char * ) mPC->inOutStruct, MAX_ACTION_SIZE );
    i = _delayExec( actionCall, recoveryActionCall, delayCondition, rei );
    return i;
}

int _delayExec( char *inActionCall, char *recoveryActionCall,
                char *delayCondition,  ruleExecInfo_t *rei ) {

    char *args[MAX_NUM_OF_ARGS_IN_ACTION];
    int i, argc;
    ruleExecSubmitInp_t *ruleSubmitInfo;
    /* char action[MAX_ACTION_SIZE]; */
    char tmpStr[NAME_LEN];
    bytesBuf_t *packedReiAndArgBBuf = NULL;
    char *ruleExecId;
    char *actionCall;


    RE_TEST_MACRO( "    Calling _delayExec" );

    actionCall = inActionCall;
    /* Get Arguments */
    actionCall = ( char * ) malloc( strlen( inActionCall ) + strlen( recoveryActionCall ) + 3 );
    sprintf( actionCall, "%s|%s", inActionCall, recoveryActionCall );
    args[1] = NULL;
    argc = 0;
    /* Pack Rei and Args */
    i = packReiAndArg( rei, args, argc, &packedReiAndArgBBuf );
    if ( i < 0 ) {
        if ( actionCall != inActionCall ) {
            free( actionCall );
        }
        return i;
    }
    /* fill Conditions into Submit Struct */
    ruleSubmitInfo = ( ruleExecSubmitInp_t * ) mallocAndZero( sizeof( ruleExecSubmitInp_t ) );
    i  = fillSubmitConditions( actionCall, delayCondition, packedReiAndArgBBuf, ruleSubmitInfo, rei );
    if ( actionCall != inActionCall ) {
        free( actionCall );
    }
    if ( i < 0 ) {
        free( ruleSubmitInfo );
        return i;
    }

    /* Store ReiArgs Struct in a File */
    i = rsRuleExecSubmit( rei->rsComm, ruleSubmitInfo, &ruleExecId );
    if ( packedReiAndArgBBuf != NULL ) {
        clearBBuf( packedReiAndArgBBuf );
        free( packedReiAndArgBBuf );
    }

    free( ruleSubmitInfo );
    if ( i < 0 ) {
        return i;
    }
    free( ruleExecId );
    snprintf( tmpStr, NAME_LEN, "%d", i );
    i = pushStack( &delayStack, tmpStr );
    return i;
}

int recover_delayExec( msParam_t*, msParam_t*,  ruleExecInfo_t *rei ) {

    int i;
    ruleExecDelInp_t ruleExecDelInp;

    RE_TEST_MACRO( "    Calling recover_delayExec" );

    i  = popStack( &delayStack, ruleExecDelInp.ruleExecId );
    if ( i < 0 ) {
        return i;
    }

    i = rsRuleExecDel( rei->rsComm, &ruleExecDelInp );
    return i;

}


/**
 * \cond oldruleengine
 * \fn remoteExec(msParam_t *mPD, msParam_t *mPA, msParam_t *mPB, msParam_t *mPC, ruleExecInfo_t *rei)
 *
 * \brief  A set of statements to be remotely executed
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \note This micoservice takes a set of microservices that need to be executed at a
 *    remote iRODS server. The execution is done immediately and synchronously with
 *    the result returning back from the call.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] mPD - a msParam of type STR_MS_T which is a host name of the server where the body need to be executed.
 * \param[in] mPA - a msParam of type STR_MS_T which is a delayCondition about when to execute the body.
 * \param[in] mPB - a msParam of type STR_MS_T which is a body. A statement list - micro-services and ruleNames separated by ##
 * \param[in] mPC - a msParam of type STR_MS_T which is arecoverBody. A statement list - micro-services and ruleNames separated by ##
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa  none
 * \endcond
**/
int remoteExec( msParam_t *mPD, msParam_t *mPA, msParam_t *mPB, msParam_t *mPC, ruleExecInfo_t *rei ) {
    int i;
    execMyRuleInp_t execMyRuleInp;
    msParamArray_t *tmpParamArray, *aParamArray;
    msParamArray_t *outParamArray = NULL;
    char tmpStr[LONG_NAME_LEN];
    char tmpStr1[MAX_COND_LEN];
    /*
    char actionCall[MAX_ACTION_SIZE];
    char recoveryActionCall[MAX_ACTION_SIZE];
    char delayCondition[MAX_ACTION_SIZE];
    char hostName[MAX_ACTION_SIZE];
    rstrcpy(hostName, (char *) mPD->inOutStruct,MAX_ACTION_SIZE);
    rstrcpy(delayCondition, (char *) mPA->inOutStruct,MAX_ACTION_SIZE);
    rstrcpy(actionCall, (char *) mPB->inOutStruct,MAX_ACTION_SIZE);
    rstrcpy(recoveryActionCall, (char *) mPC->inOutStruct,MAX_ACTION_SIZE);
    i = _remoteExec(actionCall, recoveryActionCall, delayCondition, hostName, rei);
    */
    memset( &execMyRuleInp, 0, sizeof( execMyRuleInp ) );
    execMyRuleInp.condInput.len = 0;
    rstrcpy( execMyRuleInp.outParamDesc, ALL_MS_PARAM_KW, LONG_NAME_LEN );
    /*  rstrcpy (execMyRuleInp.addr.hostAddr, mPD->inOutStruct, LONG_NAME_LEN);*/
    rstrcpy( tmpStr, ( char * ) mPD->inOutStruct, LONG_NAME_LEN );
    i = evaluateExpression( tmpStr, tmpStr1, rei );
    if ( i < 0 ) {
        return i;
    }
    parseHostAddrStr( tmpStr1, &execMyRuleInp.addr );

    if ( strlen( ( char* )mPC->inOutStruct ) != 0 ) {
        snprintf( execMyRuleInp.myRule, META_STR_LEN, "remExec||%s|%s", ( char* )mPB->inOutStruct, ( char* )mPC->inOutStruct );
    }
    else {
        snprintf( execMyRuleInp.myRule, META_STR_LEN, "remExec{%s}", ( char* )mPB->inOutStruct );
    }
    addKeyVal( &execMyRuleInp.condInput, "execCondition", ( char * ) mPA->inOutStruct );

    tmpParamArray = ( msParamArray_t * ) malloc( sizeof( msParamArray_t ) );
    memset( tmpParamArray, 0, sizeof( msParamArray_t ) );
    i = replMsParamArray( rei->msParamArray, tmpParamArray );
    if ( i < 0 ) {
        free( tmpParamArray );
        return i;
    }
    aParamArray = rei->msParamArray;
    rei->msParamArray = tmpParamArray;
    execMyRuleInp.inpParamArray = rei->msParamArray;
    i = rsExecMyRule( rei->rsComm, &execMyRuleInp,  &outParamArray );
    carryOverMsParam( outParamArray, aParamArray );
    rei->msParamArray = aParamArray;
    clearMsParamArray( tmpParamArray, 0 );
    free( tmpParamArray );
    return i;
}


/*****
int _remoteExec(char *inActionCall, char *recoveryActionCall,
	       char *delayCondition,  char *hostName, ruleExecInfo_t *rei)
{

  char *args[MAX_NUM_OF_ARGS_IN_ACTION];
  int i, argc;
  ruleExecSubmitInp_t *ruleSubmitInfo;
  char action[MAX_ACTION_SIZE];
  char tmpStr[NAME_LEN];
  bytesBuf_t *packedReiAndArgBBuf = NULL;
  char *ruleExecId;
  char *actionCall;


  RE_TEST_MACRO ("    Calling _delayExec");

  actionCall = inActionCall;
  if (strstr(actionCall,"##") == NULL && !strcmp(recoveryActionCall,"nop")) {
    i = parseAction(actionCall,action,args, &argc);
    if (i != 0)
      return i;
    mapExternalFuncToInternalProc(action);
    argc = 0;
  }
  else {
    actionCall = malloc(strlen(inActionCall) + strlen(recoveryActionCall) + 3);
    sprintf(actionCall,"%s|%s",inActionCall,recoveryActionCall);
    args[0] = NULL;
    args[1] = NULL;
    argc = 0;
  }

  i = packReiAndArg (rei, args, argc, &packedReiAndArgBBuf);
  if (i < 0) {
    if (actionCall != inActionCall)
      free (actionCall);
    return i;
  }

  ruleSubmitInfo = mallocAndZero(sizeof(ruleExecSubmitInp_t));
  i  = fillSubmitConditions (actionCall, delayCondition, packedReiAndArgBBuf, ruleSubmitInfo, rei);
  strncpy(ruleSubmitInfo->exeAddress,hostName,NAME_LEN);
  if (actionCall != inActionCall)
    free (actionCall);
  if (i < 0) {
    free(ruleSubmitInfo);
    return i;
  }

  i = rsRemoteRuleExecSubmit(rei->rsComm, ruleSubmitInfo, &ruleExecId);
  if (packedReiAndArgBBuf != NULL) {
    clearBBuf (packedReiAndArgBBuf);
    free (packedReiAndArgBBuf);
  }

  free(ruleSubmitInfo);
  if (i < 0)
    return i;
  free (ruleExecId);
  snprintf(tmpStr,NAME_LEN, "%d",i);
  i = pushStack(&delayStack,tmpStr);
  return i;
}
******/
int recover_remoteExec( msParam_t*, msParam_t*, char*, ruleExecInfo_t *rei ) {

    int i;
    ruleExecDelInp_t ruleExecDelInp;

    RE_TEST_MACRO( "    Calling recover_delayExec" );

    i  = popStack( &delayStack, ruleExecDelInp.ruleExecId );
    if ( i < 0 ) {
        return i;
    }
    /***
    i = rsRemoteRuleExecDel(rei->rsComm, &ruleExecDelInp);
    ***/
    return i;

}


int
doForkExec( char *prog, char *arg1 ) {
    int pid, i;

    i = checkFilePerms( prog );
    if ( i ) {
        return i;
    }

    i = checkFilePerms( arg1 );
    if ( i ) {
        return i;
    }

#ifndef windows_platform
    pid = fork();
    if ( pid == -1 ) {
        return -1;
    }

    if ( pid ) {
        /*  This is still the parent.  */
    }
    else {
        /* child */
        for ( i = 0; i < 100; i++ ) {
            close( i );
        }
        i = execl( prog, prog, arg1, ( char * ) 0 );
        printf( "execl failed %d\n", i );
        return 0;
    }
#else
    /* windows platform */
    if ( _spawnl( _P_NOWAIT, prog, prog, arg1 ) == -1 ) {
        return -1;
    }
#endif

    return 0;
}


/**
 * \fn msiGoodFailure(ruleExecInfo_t *)
 *
 * \brief  This microservice fails on purpose.
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \note This microservice performs no operations but fails the current rule application
 *    immediately even if the body still has some more microservices. But other definitions
 *    of the rule are not retried upon this failure. It is useful when you want to fail
 *    but no recovery initiated.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval negative number
 * \pre none
 * \post none
 * \sa fail
**/
int
msiGoodFailure( ruleExecInfo_t* ) {

    /**** This is Just a Test Stub  ****/
    if ( reTestFlag > 0 ) {
        if ( reTestFlag == LOG_TEST_1 ) {
            rodsLog( LOG_NOTICE, "   Calling msiGoodFailure So that It will Retry Other Rules Without Recovery\n" );
        }
        return RETRY_WITHOUT_RECOVERY_ERR;
    }
    /**** This is Just a Test Stub  ****/
    return RETRY_WITHOUT_RECOVERY_ERR;
}



/* check that a file exists and is not writable by group or other */
int
checkFilePerms( char *fileName ) {
    struct stat buf;
    if ( stat( fileName, &buf ) == -1 ) {
        printf( "Stat failed for file %s\n",
                fileName );
        return -1;
    }

    if ( buf.st_mode & 022 ) {
        printf( "File is writable by group or other: %s.\n",
                fileName );
        return -2;
    }
    return 0;
}

/**
 * \fn msiFreeBuffer(msParam_t* memoryParam, ruleExecInfo_t *rei)
 *
 * \brief  This microservice frees a named buffer, including stdout and stderr
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] memoryParam - the buffer to free
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence  none
 * \DolVarModified  none
 * \iCatAttrDependence  none
 * \iCatAttrModified  none
 * \sideeffect  none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa  none
**/
int
msiFreeBuffer( msParam_t* memoryParam, ruleExecInfo_t *rei ) {

    msParamArray_t *inMsParamArray;
    msParam_t *mP;
    execCmdOut_t *myExecCmdOut;

    RE_TEST_MACRO( "Loopback on msiFreeBuffer" );

    if ( !strcmp( memoryParam->type, "STR_PI" ) &&
            ( !strcmp( ( char* )memoryParam->inOutStruct, "stdout" ) ||
              !strcmp( ( char* )memoryParam->inOutStruct, "stderr" )
            )
       ) {
        mP = NULL;
        inMsParamArray = rei->msParamArray;
        if ( ( ( mP = getMsParamByLabel( inMsParamArray, "ruleExecOut" ) ) != NULL ) &&
                ( mP->inOutStruct != NULL ) ) {
            myExecCmdOut = ( execCmdOut_t * ) mP->inOutStruct;
            if ( !strcmp( ( char* )memoryParam->inOutStruct, "stdout" ) ) {
                if ( myExecCmdOut->stdoutBuf.buf != NULL ) {
                    free( myExecCmdOut->stdoutBuf.buf );
                    myExecCmdOut->stdoutBuf.buf = NULL;
                    myExecCmdOut->stdoutBuf.len = 0;
                }
            }
            if ( !strcmp( ( char* )memoryParam->inOutStruct, "stderr" ) ) {
                if ( myExecCmdOut->stderrBuf.buf != NULL ) {
                    free( myExecCmdOut->stderrBuf.buf );
                    myExecCmdOut->stderrBuf.buf = NULL;
                    myExecCmdOut->stderrBuf.len = 0;
                }
            }
        }
        return 0;
    }

    if ( memoryParam->inpOutBuf != NULL ) {
        free( memoryParam->inpOutBuf );
    }
    memoryParam->inpOutBuf = NULL;
    return 0;

}

/**
 * \fn msiSleep(msParam_t* secPtr, msParam_t* microsecPtr,  ruleExecInfo_t* )
 *
 * \brief  Sleep for some amount of time
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] secPtr - secPtr is a msParam of type STR_MS_T which is seconds
 * \param[in] microsecPtr - microsecPrt is a msParam of type STR_MS_T which is microseconds
 * \param[in,out] - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre  none
 * \post none
 * \sa  none
**/
int
msiSleep( msParam_t* secPtr, msParam_t* microsecPtr,  ruleExecInfo_t* ) {

    int sec, microsec;

    sec = atoi( ( char * ) secPtr->inOutStruct );
    microsec = atoi( ( char * ) microsecPtr->inOutStruct );

    rodsSleep( sec, microsec );
    return 0;
}

/**
 * \fn msiApplyAllRules(msParam_t *actionParam, msParam_t* reiSaveFlagParam, msParam_t* allRuleExecFlagParam, ruleExecInfo_t *rei)
 *
 * \brief  DEPRECATED - msiApplyAllRules is too ad-hoc and non-deterministic.
 *      If you need to apply multiple rules and catch the cases where one may fail,
 *      the best practice is to use the errorcode() and test for the result.
 *
 * \module core
 *
 * \since pre-2.1
 *
 * \deprecated Since 4.1.0.  Will be removed in 4.2.0
 *
 * \note Normal operations of the rule engine is to stop after a rule (one of the alternates)
 *   completes successfully. But in some cases, one may want the rule engine to try all
 *   alternatives and succeed in as many as possible. Then by firing that rule under this
 *   microservice all alternatives are tried.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] actionParam - a msParam of type STR_MS_T which is the name of an action to be executed.
 * \param[in] reiSaveFlagParam - a msParam of type STR_MS_T which is 0 or 1 value used to
 *    check if the rei structure needs to be saved at every rule invocation inside the
 *    execution. This helps to save time if the rei structure is known not to be
 *    changed when executing the underlying rules.
 * \param[in] allRuleExecFlagParam - allRuleExecFlagParam is a msParam of type STR_MS_T which
 *    is 0 or 1 whether the "apply all rule" condition applies only to the actionParam
 *    invocation or is recursively done at all levels of invocation of every rule inside the execution.
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiApplyAllRules( msParam_t *actionParam, msParam_t* reiSaveFlagParam,
                  msParam_t* allRuleExecFlagParam, ruleExecInfo_t *rei ) {
    int i;
    char *action;
    int reiSaveFlag;
    int allRuleExecFlag;

    action = ( char * ) actionParam->inOutStruct;
    reiSaveFlag = atoi( ( char * ) reiSaveFlagParam->inOutStruct );
    allRuleExecFlag = atoi( ( char * ) allRuleExecFlagParam->inOutStruct );
    i = applyAllRules( action, rei->msParamArray, rei, reiSaveFlag, allRuleExecFlag );
    return i;

}


/**
 * \fn msiGetDiffTime(msParam_t* inpParam1, msParam_t* inpParam2, msParam_t* inpParam3, msParam_t* outParam, ruleExecInfo_t *rei)
 *
 * \brief This microservice returns the difference between two system times
 *
 * \module framework
 *
 * \since 2.1
 *
 *
 * \note If we have arithmetic MSs in the future we should use DOUBLE_MS_T instead of strings.
 *       Default output format is in seconds, use 'human' as the third input param for human readable format.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] inpParam1 - a STR_MS_T containing the start date (system time in seconds)
 * \param[in] inpParam2 - a STR_MS_T containing the end date (system time in seconds)
 * \param[in] inpParam3 - Optional - a STR_MS_T containing the desired output format
 * \param[out] outParam - a STR_MS_T containing the time elapsed between the two dates
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiGetDiffTime( msParam_t* inpParam1, msParam_t* inpParam2, msParam_t* inpParam3, msParam_t* outParam, ruleExecInfo_t *rei ) {
    long hours, minutes, seconds;
    char timeStr[TIME_LEN];
    char *format;
    int status;


    /* For testing mode when used with irule --test */
    RE_TEST_MACRO( "    Calling msiGetDiffTime" )


    if ( rei == NULL || rei->rsComm == NULL ) {
        rodsLog( LOG_ERROR, "msiGetDiffTime: input rei or rsComm is NULL" );
        return SYS_INTERNAL_NULL_INPUT_ERR;
    }


    /* Check for proper input */
    if ( ( parseMspForStr( inpParam1 ) == NULL ) ) {
        rodsLog( LOG_ERROR, "msiGetDiffTime: input inpParam1 is NULL" );
        return USER__NULL_INPUT_ERR;
    }

    if ( ( parseMspForStr( inpParam2 ) == NULL ) ) {
        rodsLog( LOG_ERROR, "msiGetDiffTime: input inpParam2 is NULL" );
        return USER__NULL_INPUT_ERR;
    }


    /* get time values from strings */
    seconds = atol( ( char * ) inpParam2->inOutStruct ) - atol( ( char * )inpParam1->inOutStruct );

    /* get desired output format */
    format = ( char * ) inpParam3->inOutStruct;


    /* did they ask for human readable format? */
    if ( format && !strcmp( format, "human" ) ) {
        /* get hours-minutes-seconds */
        hours = seconds / 3600;
        seconds = seconds % 3600;
        minutes = seconds / 60;
        seconds = seconds % 60;

        snprintf( timeStr, TIME_LEN, "%ldh %ldm %lds", hours, minutes, seconds );
    }
    else {
        snprintf( timeStr, TIME_LEN, "%011ld", seconds );
    }


    /* result goes to outParam */
    status = fillStrInMsParam( outParam, timeStr );

    return status;
}


/**
 * \fn msiGetSystemTime(msParam_t* outParam, msParam_t* inpParam, ruleExecInfo_t *rei)
 *
 * \brief This microservice returns the local system time
 *
 * \module framework
 *
 * \since pre-2.1
 *
 *
 * \note Default output format is system time in seconds, use 'human' as input param for human readable format.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[out] outParam - a STR_MS_T containing the time
 * \param[in] inpParam - Optional - a STR_MS_T containing the desired output format
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiGetSystemTime( msParam_t* outParam, msParam_t* inpParam, ruleExecInfo_t *rei ) {
    char *format;
    char tStr0[TIME_LEN], tStr[TIME_LEN];
    int status;

    /* For testing mode when used with irule --test */
    RE_TEST_MACRO( "    Calling msiGetSystemTime" )


    if ( rei == NULL || rei->rsComm == NULL ) {
        rodsLog( LOG_ERROR, "msiGetSystemTime: input rei or rsComm is NULL" );
        return SYS_INTERNAL_NULL_INPUT_ERR;
    }


    format = ( char * ) inpParam->inOutStruct;

    if ( !format || strcmp( format, "human" ) ) {
        getNowStr( tStr );
    }
    else {
        getNowStr( tStr0 );
        getLocalTimeFromRodsTime( tStr0, tStr );
    }

    status = fillStrInMsParam( outParam, tStr );

    return status;
}


/**
 * \fn msiHumanToSystemTime(msParam_t* inpParam, msParam_t* outParam, ruleExecInfo_t *rei)
 *
 * \brief Converts a human readable date to a system timestamp
 *
 * \module framework
 *
 * \since pre-2.1
 *
 *
 * \note Expects an input date in the form: YYYY-MM-DD-hh.mm.ss
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] inpParam - a STR_MS_T containing the input date
 * \param[out] outParam - a STR_MS_T containing the timestamp
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiHumanToSystemTime( msParam_t* inpParam, msParam_t* outParam, ruleExecInfo_t *rei ) {
    //CID26185: prevent buffer overrun
    char sys_time[TIME_LEN + 1], *hr_time;
    int status;

    /* For testing mode when used with irule --test */
    RE_TEST_MACRO( "    Calling msiHumanToSystemTime" )


    /* microservice check */
    if ( rei == NULL || rei->rsComm == NULL ) {
        rodsLog( LOG_ERROR, "msiHumanToSystemTime: input rei or rsComm is NULL" );
        return SYS_INTERNAL_NULL_INPUT_ERR;
    }


    /* parse inpParam (date input string) */
    if ( ( hr_time = parseMspForStr( inpParam ) ) == NULL ) {
        rodsLog( LOG_ERROR, "msiHumanToSystemTime: parseMspForStr error for input param." );
        return rei->status;
    }


    /* left padding with a zero for DB format. Might change that later */
    memset( sys_time, '\0', TIME_LEN );
    sys_time[0] = '0';


    /* conversion */
    if ( ( status = localToUnixTime( hr_time, &sys_time[1] ) )  == 0 ) {
        status = fillStrInMsParam( outParam, sys_time );
    }


    return status;
}



/**
 * \fn msiStrToBytesBuf(msParam_t* str_msp, msParam_t* buf_msp, ruleExecInfo_t* )
 *
 * \brief Converts a string to a bytesBuf_t
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \note For easily passing parameters to microservices that require a BUF_LEN_MS_T
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[in] str_msp - a STR_MS_T
 * \param[out] buf_msp - a BUF_LEN_MS_T
 * \param[in,out] - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiStrToBytesBuf( msParam_t* str_msp, msParam_t* buf_msp, ruleExecInfo_t* ) {
    char *inStr;
    bytesBuf_t *outBuf;

    /* parse str_msp */
    if ( ( inStr = parseMspForStr( str_msp ) ) == NULL ) {
        rodsLog( LOG_ERROR, "msiStrToBytesBuf: input str_msp is NULL." );
        return USER__NULL_INPUT_ERR;
    }

    /* buffer init */
    outBuf = ( bytesBuf_t * )malloc( sizeof( bytesBuf_t ) );
    memset( outBuf, 0, sizeof( bytesBuf_t ) );

    /* fill string in buffer */
    outBuf->len = strlen( inStr );
    outBuf->buf = strdup( inStr );

    /* fill buffer in buf_msp */
    fillBufLenInMsParam( buf_msp, outBuf->len, outBuf );

    return 0;
}

/**
 *\fn msiBytesBufToStr(msParam_t* buf_msp,  msParam_t* str_msp, ruleExecInfo_t* )
 *
 * \brief Writes  a bytesBuf_t into a character string
 *
 * \module core
 *
 * \since 3.3
 *
 *
 * \remark This may blow up if the buffer is not a character string. so be careful
 *
 * \note The string can have length up to BUF_LEN_MS_T
 *
 * \usage
 *
 * testrule{
 *    msiBytesBufToStr(*Buff, *St)
 *    writeLine(stdout,*St)
 * }
 * ruleExecOut
 *
 * \param[in] buf_msp - a BUF_LEN_MS_T
 * \param[out] str_msp - a STR_MS_T
 * \param[in,out]  - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiBytesBufToStr( msParam_t* buf_msp, msParam_t* str_msp, ruleExecInfo_t* ) {
    char *outStr;
    bytesBuf_t *inBuf;
    int single_buff_sz = 0;
    irods::error ret = irods::get_advanced_setting<int>(
                           irods::CFG_MAX_SIZE_FOR_SINGLE_BUFFER,
                           single_buff_sz );
    if ( !ret.ok() ) {
        irods::log( PASS( ret ) );
        return ret.code();
    }
    single_buff_sz *= 1024 * 1024;

    /*check buf_msp */
    if ( buf_msp == NULL || buf_msp->inOutStruct == NULL ) {
        rodsLog( LOG_ERROR, "msiBytesBufToStr: input buf_msp is NULL." );
        return USER__NULL_INPUT_ERR;
    }
    inBuf = buf_msp->inpOutBuf;
    if ( inBuf->len < 0 || inBuf->len > ( single_buff_sz - 10 ) )  {
        rodsLog( LOG_ERROR, "msiBytesBufToStr: input buf_msp is NULL." );
        return USER_INPUT_FORMAT_ERR;
    }
    outStr = ( char * ) malloc( ( inBuf->len + 1 ) );
    outStr[inBuf->len] = '\0';
    strncpy( outStr, ( char * ) inBuf->buf, inBuf->len );
    fillStrInMsParam( str_msp, outStr );
    free( outStr );

    return 0;
}



/**
 * \fn msiListEnabledMS(msParam_t *outKVPairs, ruleExecInfo_t *rei)
 *
 * \brief Returns the list of compiled microservices on the local iRODS server
 *
 * \module framework
 *
 * \since 2.1
 *
 *
 * \note This microservice looks at reAction.hpp and returns the list of compiled
 *  microservices on the local iRODS server.
 *      The results are written to a KeyValPair_MS_T. For each pair the keyword is the MS name
 *  while the value is the module where the microservice belongs.
 *  Standard non-module microservices are listed as "core".
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[out] outKVPairs - A KeyValPair_MS_T containing the results.
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiListEnabledMS(
    msParam_t*      outKVPairs,
    ruleExecInfo_t* rei ) {

    extern irods::ms_table MicrosTable;
    keyValPair_t *results;		/* the output data structure */


    /* For testing mode when used with irule --test */
    RE_TEST_MACRO( "    Calling msiEnabledMS" )


    /* Sanity test */
    if ( rei == NULL || rei->rsComm == NULL ) {
        rodsLog( LOG_ERROR, "msiListEnabledMS: input rei or rsComm is NULL." );
        return SYS_INTERNAL_NULL_INPUT_ERR;
    }

    /* Allocate memory for our result struct */
    results = ( keyValPair_t* )malloc( sizeof( keyValPair_t ) );
    memset( results, 0, sizeof( keyValPair_t ) );

    // =-=-=-=-=-=-=-
    // scan table for msvc names and add to kvp struct
    irods::ms_table::iterator itr = MicrosTable.begin();
    for ( ; itr != MicrosTable.end(); ++itr ) {
        addKeyVal( results, itr->first.c_str(), "core" );

    } // for i

    // =-=-=-=-=-=-=-
    // scan plugin directory for additional plugins
    std::string plugin_home;
    irods::error ret = irods::resolve_plugin_path( irods::PLUGIN_TYPE_MICROSERVICE, plugin_home );
    if ( !ret.ok() ) {
        free( results );
        irods::log( PASS( ret ) );
        return ret.code();
    }

    irods::plugin_name_generator name_gen;
    irods::plugin_name_generator::plugin_list_t plugin_list;
    ret = name_gen.list_plugins( plugin_home, plugin_list );
    if ( ret.ok() ) {
        irods::plugin_name_generator::plugin_list_t::iterator it = plugin_list.begin();
        for ( ; it != plugin_list.end(); ++it ) {
            addKeyVal( results, it->c_str(), "plugin" );
        }
    }

    /* Send results out to outKVPairs */
    fillMsParam( outKVPairs, NULL, KeyValPair_MS_T, results, NULL );

    return 0;
}


/**
 * \fn msiGetFormattedSystemTime(msParam_t* outParam, msParam_t* inpParam,
 *          msParam_t* inpFormatParam, ruleExecInfo_t *rei)
 *
 * \brief Returns the local system time, formatted
 *
 * \module core
 *
 * \since pre-2.1
 *
 *
 * \note Default output format is system time in seconds. Use 'human' as input
 *       parameter for human readable format.
 *
 * \usage See clients/icommands/test/rules/
 *
 * \param[out] outParam - a STR_MS_T containing the time
 * \param[in] inpParam - Optional - a STR_MS_T containing the desired output format (human)
 * \param[in] inpFormatParam - Optional - a STR_MS_T containing printf formatting (if inpParam was human)
 * \param[in,out] rei - The RuleExecInfo structure that is automatically
 *    handled by the rule engine. The user does not include rei as a
 *    parameter in the rule invocation.
 *
 * \DolVarDependence none
 * \DolVarModified none
 * \iCatAttrDependence none
 * \iCatAttrModified none
 * \sideeffect none
 *
 * \return integer
 * \retval 0 on success
 * \pre none
 * \post none
 * \sa none
**/
int
msiGetFormattedSystemTime( msParam_t* outParam, msParam_t* inpParam,
                           msParam_t* inpFormatParam, ruleExecInfo_t *rei ) {
    char *format;
    char *dateFormat;
    char tStr[TIME_LEN];
    time_t myTime;
    struct tm *mytm;
    int status;

    /* For testing mode when used with irule --test */
    RE_TEST_MACRO( "    Calling msiGetFormattedSystemTime" );


    if ( rei == NULL || rei->rsComm == NULL ) {
        rodsLog( LOG_ERROR,
                 "msiGetFormattedSystemTime: input rei or rsComm is NULL" );
        return SYS_INTERNAL_NULL_INPUT_ERR;
    }
    format = ( char* )inpParam->inOutStruct;
    dateFormat = ( char* )inpFormatParam->inOutStruct;

    if ( !format || strcmp( format, "human" ) ) {
        getNowStr( tStr );
    }
    else {
        myTime = time( NULL );
        mytm = localtime( &myTime );

        snprintf( tStr, TIME_LEN, dateFormat,
                  mytm->tm_year + 1900, mytm->tm_mon + 1, mytm->tm_mday,
                  mytm->tm_hour, mytm->tm_min, mytm->tm_sec );
    }
    status = fillStrInMsParam( outParam, tStr );
    return status;
}




