/***************************************************************
* StreamDevice record interface for aao records                *
*                                                              *
* (C) 1999 Dirk Zimoch (zimoch@delta.uni-dortmund.de)          *
* (C) 2006 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is an EPICS record Interface for StreamDevice.          *
* Please refer to the HTML files in ../docs/ for a detailed    *
* documentation.                                               *
*                                                              *
* If you do any changes in this file, you are not allowed to   *
* redistribute it any more. If there is a bug or a missing     *
* feature, send me an email and/or your patch. If I accept     *
* your changes, they will go to the next release.              *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

#include <stdio.h>
#include "epicsString.h"
#include "aaoRecord.h"
#include "devStream.h"

static long readData(dbCommon *record, format_t *format)
{
    aaoRecord *aao = (aaoRecord *)record;
    double dval;
    long lval;
    unsigned short monitor_mask;

    for (aao->nord = 0; aao->nord < aao->nelm; aao->nord++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                if (streamScanf(record, format, &dval) == ERROR)
                    goto end;
                switch (aao->ftvl)
                {
                    case DBF_DOUBLE:
                        ((epicsFloat64 *)aao->bptr)[aao->nord] = (epicsFloat64)dval;
                        break;
                    case DBF_FLOAT:
                        ((epicsFloat32 *)aao->bptr)[aao->nord] = (epicsFloat32)dval;
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from double to %s\n",
                            record->name, pamapdbfType[aao->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_ULONG:
            case DBF_LONG:
            case DBF_ENUM:
            {
                if (streamScanf(record, format, &lval) == ERROR)
                    goto end;
                switch (aao->ftvl)
                {
                    case DBF_DOUBLE:
                        ((epicsFloat64 *)aao->bptr)[aao->nord] = (epicsFloat64)lval;
                        break;
                    case DBF_FLOAT:
                        ((epicsFloat32 *)aao->bptr)[aao->nord] = (epicsFloat32)lval;
                        break;
#ifdef DBF_INT64
                    case DBF_INT64:
                    case DBF_UINT64:
                        ((epicsInt64 *)aao->bptr)[aao->nord] = (epicsInt64)lval;
                        break;
#endif
                    case DBF_LONG:
                    case DBF_ULONG:
                        ((epicsInt32 *)aao->bptr)[aao->nord] = (epicsInt32)lval;
                        break;
                    case DBF_SHORT:
                    case DBF_USHORT:
                    case DBF_ENUM:
                        ((epicsInt16 *)aao->bptr)[aao->nord] = (epicsInt16)lval;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        ((epicsInt8 *)aao->bptr)[aao->nord] = (epicsInt8)lval;
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from long to %s\n",
                            record->name, pamapdbfType[aao->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            case DBF_STRING:
            {
                switch (aao->ftvl)
                {
                    case DBF_STRING:
                        if (streamScanfN(record, format,
                            (char *)aao->bptr + aao->nord * MAX_STRING_SIZE,
                            MAX_STRING_SIZE) == ERROR)
                            goto end;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                    {
						ssize_t length;
                        aao->nord = 0;
                        if ((length = streamScanfN(record, format,
                            (char *)aao->bptr, aao->nelm)) == ERROR)
                        {
                            memset(aao->bptr, 0, aao->nelm);
                            return ERROR;
                        }
                        if (length < (ssize_t)aao->nelm)
                        {
                            memset(((char*)aao->bptr)+lval , 0, aao->nelm-lval);
                            lval++;
                        }
                        else
                        {
                            ((char*)aao->bptr)[aao->nelm-1] = 0;
                        }
                        aao->nord = (long)length;
                        goto end_no_check;
                    }
                    default:
                        errlogSevPrintf(errlogFatal,
                            "readData %s: can't convert from string to %s\n",
                            record->name, pamapdbfType[aao->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf(errlogMajor,
                    "readData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[format->type].strvalue,
                    pamapdbfType[aao->ftvl].strvalue);
                return ERROR;
            }
        }
    }
end:
    if (aao->nord == 0) return ERROR;
end_no_check:
    if (record->pact) return OK;
    /* In @init handler, no processing, enforce monitor updates. */
    monitor_mask = recGblResetAlarms(aao);
#if defined(VERSION_INT) || EPICS_MODIFICATION >= 12
    if (aao->mpst == aaoPOST_Always)
        monitor_mask |= DBE_VALUE;
    if (aao->apst == aaoPOST_Always)
        monitor_mask |= DBE_LOG;
    if ((aao->mpst == aaoPOST_OnChange) ||
        (aao->apst == aaoPOST_OnChange))
    {
        unsigned int hash = epicsMemHash(aao->bptr,
            aao->nord * dbValueSize(aao->ftvl), 0);
        if (hash != aao->hash)
        {
            if (aao->mpst == aaoPOST_OnChange)
                monitor_mask |= DBE_VALUE;
            if (aao->apst == aaoPOST_OnChange)
                monitor_mask |= DBE_LOG;
            aao->hash = hash;
            db_post_events(aao, &aao->hash, DBE_VALUE);
        }
    }
#else
    monitor_mask |= DBE_VALUE | DBE_LOG;
#endif
    if (monitor_mask)
        db_post_events(aao, aao->bptr, monitor_mask);
    
    return OK;
}

static long writeData(dbCommon *record, format_t *format)
{
    aaoRecord *aao = (aaoRecord *)record;
    double dval;
    long lval;
    unsigned long nowd;

    for (nowd = 0; nowd < aao->nord; nowd++)
    {
        switch (format->type)
        {
            case DBF_DOUBLE:
            {
                switch (aao->ftvl)
                {
                    case DBF_DOUBLE:
                        dval = ((epicsFloat64 *)aao->bptr)[nowd];
                        break;
                    case DBF_FLOAT:
                        dval = ((epicsFloat32 *)aao->bptr)[nowd];
                        break;
#ifdef DBF_INT64
                    case DBF_INT64:
                        dval = ((epicsInt64 *)aao->bptr)[nowd];
                        break;
                    case DBF_UINT64:
                        dval = ((epicsUInt64 *)aao->bptr)[nowd];
                        break;
#endif
                    case DBF_LONG:
                        dval = ((epicsInt32 *)aao->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        dval = ((epicsUInt32 *)aao->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                        dval = ((epicsInt16 *)aao->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                    case DBF_ENUM:
                        dval = ((epicsUInt16 *)aao->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        dval = ((epicsInt8 *)aao->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        dval = ((epicsUInt8 *)aao->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to double\n",
                            record->name, pamapdbfType[aao->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf(record, format, dval))
                    return ERROR;
                break;
            }
            case DBF_ULONG:
            case DBF_LONG:
            case DBF_ENUM:
            {
                switch (aao->ftvl)
                {
#ifdef DBF_INT64
                    case DBF_INT64:
                        lval = ((epicsInt64 *)aao->bptr)[nowd];
                        break;
                    case DBF_UINT64:
                        lval = ((epicsUInt64 *)aao->bptr)[nowd];
                        break;
#endif
                    case DBF_LONG:
                        lval = ((epicsInt32 *)aao->bptr)[nowd];
                        break;
                    case DBF_ULONG:
                        lval = ((epicsUInt32 *)aao->bptr)[nowd];
                        break;
                    case DBF_SHORT:
                        lval = ((epicsInt16 *)aao->bptr)[nowd];
                        break;
                    case DBF_USHORT:
                    case DBF_ENUM:
                        lval = ((epicsUInt16 *)aao->bptr)[nowd];
                        break;
                    case DBF_CHAR:
                        lval = ((epicsInt8 *)aao->bptr)[nowd];
                        break;
                    case DBF_UCHAR:
                        lval = ((epicsUInt8 *)aao->bptr)[nowd];
                        break;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to long\n",
                            record->name, pamapdbfType[aao->ftvl].strvalue);
                        return ERROR;
                }
                if (streamPrintf(record, format, lval))
                    return ERROR;
                break;
            }
            case DBF_STRING:
            {
                switch (aao->ftvl)
                {
                    case DBF_STRING:
                        if (streamPrintf(record, format,
                            ((char *)aao->bptr) + nowd * MAX_STRING_SIZE))
                            return ERROR;
                        break;
                    case DBF_CHAR:
                    case DBF_UCHAR:
                        /* print aao as a null-terminated string */
                        if (aao->nord < aao->nelm)
                        {
                            ((char *)aao->bptr)[aao->nord] = 0;
                        }
                        else
                        {
                            ((char *)aao->bptr)[aao->nelm-1] = 0;
                        }
                        if (streamPrintf(record, format,
                            ((char *)aao->bptr)))
                            return ERROR;
                        return OK;
                    default:
                        errlogSevPrintf(errlogFatal,
                            "writeData %s: can't convert from %s to string\n",
                            record->name, pamapdbfType[aao->ftvl].strvalue);
                        return ERROR;
                }
                break;
            }
            default:
            {
                errlogSevPrintf(errlogFatal,
                    "writeData %s: can't convert from %s to %s\n",
                    record->name, pamapdbfType[aao->ftvl].strvalue,
                    pamapdbfType[format->type].strvalue);
                return ERROR;
            }
        }
    }
    return OK;
}

static long initRecord(dbCommon *record)
{
    aaoRecord *aao = (aaoRecord *)record;

    aao->bptr = calloc(aao->nelm, dbValueSize(aao->ftvl));
    if (aao->bptr == NULL)
    {
        errlogSevPrintf(errlogFatal,
            "initRecord %s: can't allocate memory for data array\n",
            record->name);
        return ERROR;
    }
    return streamInitRecord(record, &aao->out, readData, writeData) == ERROR ?
        ERROR : OK;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
} devaaoStream = {
    5,
    streamReport,
    streamInit,
    initRecord,
    streamGetIointInfo,
    streamWrite
};

epicsExportAddress(dset,devaaoStream);
