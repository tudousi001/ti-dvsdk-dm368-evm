/* 
 * Copyright (c) 2010, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/*
 *  ======== vidtranscode_stubs.c ========
 *  This file contains an implemenation of the IVIDTRANSCODE interface for the
 *  video transcoder class of algorithms.
 *
 *  These functions are the "client-side" of a "remote" implementation.
 *
 */
#include <xdc/std.h>
#include <ti/sdo/ce/visa.h>
#include <ti/xdais/dm/ividtranscode.h>
#include <ti/sdo/ce/osal/Memory.h>
#include <ti/sdo/utils/trace/gt.h>

#include <string.h>  /* for memcpy and memset.
                      * (TODO:L Should we introduce these in Memory_*? */

#include <ti/sdo/ce/utils/xdm/XdmUtils.h>

#include "vidtranscode.h"
#include "_vidtranscode.h"

static XDAS_Int32 process(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs);
static XDAS_Int32 processAsync(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs);
static XDAS_Int32 processWait(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, UInt timeout);
static XDAS_Int32 control(IVIDTRANSCODE_Handle h, IVIDTRANSCODE_Cmd id,
    IVIDTRANSCODE_DynamicParams *params, IVIDTRANSCODE_Status *status);
static XDAS_Int32 marshallMsg(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, _VIDTRANSCODE_Msg **pmsg);
static XDAS_Int32 unmarshallMsg(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, _VIDTRANSCODE_Msg *msg, XDAS_Int32 retVal);


IVIDTRANSCODE_Fxns VIDTRANSCODE_STUBS = {
    {&VIDTRANSCODE_STUBS, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    process, control,
};

extern GT_Mask ti_sdo_ce_vidtranscode_VIDTRANSCODE_curTrace;
#define CURTRACE ti_sdo_ce_vidtranscode_VIDTRANSCODE_curTrace

/*
 *  ======== marshallMsg ========
 */
static XDAS_Int32 marshallMsg(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, _VIDTRANSCODE_Msg **pmsg)
{
    XDAS_Int32 retVal = IVIDTRANSCODE_EOK;
    VISA_Handle visa = (VISA_Handle)h;
    _VIDTRANSCODE_Msg *msg;
    Int i;
    IVIDTRANSCODE_OutArgs *pMsgOutArgs;
    Int numBufs;
    Int sizeOfStuffToMarshall;

    /*
     * Validate arguments.  Do we want to do this _every_ time, or just in
     * checked builds?
     */
    if ((inArgs == NULL) || (inArgs->size < sizeof(IVIDTRANSCODE_InArgs)) ||
        (outArgs == NULL) || (outArgs->size < sizeof(IVIDTRANSCODE_OutArgs))) {

        /* invalid args, could even assert here, it's a spec violation. */
        return (IVIDTRANSCODE_EFAIL);
    }

    if (pmsg == NULL) {
        return (IVIDTRANSCODE_EFAIL);
    }

    /* make sure it'll all fit! */
    sizeOfStuffToMarshall = sizeof(VISA_MsgHeader) + sizeof(*inBufs) +
            (sizeof(msg->cmd.process.outBufs) * XDM_MAX_IO_BUFFERS) +
            sizeof(msg->cmd.process.numOutBufs) +
            (sizeof(msg->cmd.process.outBufSizes[0]) * XDM_MAX_IO_BUFFERS) +
            inArgs->size + outArgs->size;

    if (sizeOfStuffToMarshall > VISA_getMaxMsgSize(visa)) {
        /* Can't handle these large extended args. */
        GT_2trace(CURTRACE, GT_6CLASS,
                "process> invalid arguments - too big (0x%x > 0x%x).  "
                "Validate .size fields\n", sizeOfStuffToMarshall,
                VISA_getMaxMsgSize(visa));

        return (IVIDTRANSCODE_EUNSUPPORTED);
    }

    /* get a message appropriate for this algorithm */
    if ((msg = (_VIDTRANSCODE_Msg *)VISA_allocMsg(visa)) == NULL) {
        return (IVIDTRANSCODE_EFAIL);
    }

    /* zero out msg->cmd (not msg->visa!) */
    memset(&(msg->cmd), 0, sizeof(msg->cmd));

    /*
     * Marshall the command: copy the client-passed arguments into flattened
     * message data structures, converting every pointer address to alg.
     * data buffer into physical address.
     */

    /* First specify the processing command that the skeleton should do */
    msg->visa.cmd = _VIDTRANSCODE_CPROCESS;

    /* commentary follows for marshalling the inBufs argument: */

    /* 1) inBufs->numBufs is a plain integer, we just copy it */
    msg->cmd.process.inBufs.numBufs = inBufs->numBufs;

    /*
     * inBufs->descs[] is a sparse array of buffer descriptors.  Convert them
     * if non-NULL.
     */
    for (i = 0, numBufs = 0;
         ((numBufs < inBufs->numBufs) && (i < XDM_MAX_IO_BUFFERS)); i++) {
        if (inBufs->descs[i].buf != NULL) {
            /* valid member of sparse array, convert it */
            msg->cmd.process.inBufs.descs[i].bufSize = inBufs->descs[i].bufSize;

            msg->cmd.process.inBufs.descs[i].buf = (XDAS_Int8 *)
                Memory_getBufferPhysicalAddress(inBufs->descs[i].buf,
                    inBufs->descs[i].bufSize, NULL);

            if (msg->cmd.process.inBufs.descs[i].buf == NULL) {
                retVal = IVIDTRANSCODE_EFAIL;
                goto exit;
            }

            /* Clear .accessMask; the local processor won't access this buf */
            inBufs->descs[i].accessMask = 0;

            /* found, and handled, another buffer. */
            numBufs++;
        }
        else {
            /* empty member of sparse array, no conversion needed. */
            msg->cmd.process.inBufs.descs[i].bufSize = 0;
            msg->cmd.process.inBufs.descs[i].buf = NULL;
        }
    }

    if (VISA_isChecked()) {
        /* check that we found inBufs->numBufs pointers in inBufs->bufs[] */
        GT_assert(CURTRACE, inBufs->numBufs == numBufs);
    }

    /* we're done (with inBufs). Because msg->cmd.process is non-cacheable
     * and contiguous (it has been allocated by MSGQ), we don't have to do
     * anything else.
     */

    /* Repeat the procedure for outBufs. */
    msg->cmd.process.numOutBufs = outBufs->numBufs;

    for (i = 0, numBufs = 0;
         ((numBufs < outBufs->numBufs) && (i < XDM_MAX_IO_BUFFERS)); i++) {

        if (outBufs->bufs[i] != NULL) {
            /* valid member of sparse array, convert it */
            msg->cmd.process.outBufSizes[i] = outBufs->bufSizes[i];

            msg->cmd.process.outBufs[i] = (XDAS_Int8 *)
                Memory_getBufferPhysicalAddress(outBufs->bufs[i],
                    outBufs->bufSizes[i], NULL);

            if (msg->cmd.process.outBufs[i] == NULL) {
                /* TODO:M - should add at least a trace statement when trace
                 * is supported.  Another good idea is to return something
                 * more clear than EFAIL.
                 */
                retVal = IVIDTRANSCODE_EFAIL;
                goto exit;
            }

            /* found, and handled, another buffer. */
            numBufs++;
        }
        else {
            /* empty member of sparse array, no conversion needed */
            msg->cmd.process.outBufSizes[i] = 0;
            msg->cmd.process.outBufs[i] = NULL;
        }
    }

    if (VISA_isChecked()) {
        /* check that we found outBufs->numBufs pointers in outBufs->bufs[] */
        GT_assert(CURTRACE, outBufs->numBufs == numBufs);
    }

    /* inArgs has no pointers so simply memcpy "size" bytes into the msg */
    memcpy(&(msg->cmd.process.inArgs), inArgs, inArgs->size);

    /* point at outArgs and set the "size" */
    pMsgOutArgs = (IVIDTRANSCODE_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
        inArgs->size);

    /* set the size field - the rest is filled in by the codec */
    pMsgOutArgs->size = outArgs->size;

    /*
     * Note that although outArgs contains pointers, they're not provided
     * by the application via the outArgs struct.  Rather the actual buffers
     * are provided by the application to the algorithm via outBufs.
     * So, the addresses in outArgs are output only, and do not require
     * address translation _before_ calling process().  They _do_ require
     * adress translation _after_ process(), as the algorithm may have written
     * physical addresses into the pointers.
     */

    *pmsg = msg;

    return retVal;

exit:
    VISA_freeMsg(visa, (VISA_Msg)msg);

    return retVal;
}

/*
 *  ======== unmarshallMsg ========
 */
static XDAS_Int32 unmarshallMsg(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, _VIDTRANSCODE_Msg *msg, XDAS_Int32 retVal)
{
    VISA_Handle visa = (VISA_Handle)h;
    Int i;
    IVIDTRANSCODE_OutArgs *pMsgOutArgs;

    /*
     * Do a wholesale replace of skeleton returned structure.
     * Pointer conversion of fields in outArgs is done below (only
     * in the case of a successful return value).
     */
    pMsgOutArgs = (IVIDTRANSCODE_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
        inArgs->size);

    if (VISA_isChecked()) {
        /* ensure the codec didn't change outArgs->size */
        GT_assert(CURTRACE, pMsgOutArgs->size == outArgs->size);
    }

    memcpy(outArgs, pMsgOutArgs, outArgs->size);

    /* if VISA_call was successful, also unmarshall outBufs and outArgs */
    if (retVal == IVIDTRANSCODE_EOK) {
        /* unmarshall the output data: outBufs and outArgs. */

        /* The app consumes encoded buffers via outArgs.encodedBuf[]
         * Need to translate those addresses here.
         */
        for (i = 0; i < IVIDTRANSCODE_MAXOUTSTREAMS; i++) {
            if (outArgs->encodedBuf[i].buf != NULL) {
                outArgs->encodedBuf[i].buf = Memory_getBufferVirtualAddress(
                    (UInt32)outArgs->encodedBuf[i].buf,
                    outArgs->encodedBuf[i].bufSize);

                /* Clear .accessMask; the local proc won't access this buf */
                outArgs->encodedBuf[i].accessMask = 0;
            }
        }
    }

    /* Note that we did *nothing* with inBufs nor inArgs.  This should be ok. */

    VISA_freeMsg(visa, (VISA_Msg)msg);

    return (retVal);
}

/*
 *  ======== process ========
 *  This is the sync stub-implementation for the process method
 */
static XDAS_Int32 process(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs)
{
    XDAS_Int32 retVal;
    _VIDTRANSCODE_Msg *msg;
    VISA_Handle visa = (VISA_Handle)h;

    retVal = marshallMsg(h, inBufs, outBufs, inArgs, outArgs, &msg);
    if (retVal != IVIDTRANSCODE_EOK) {
        return retVal;
    }

    /* send the message to the skeleton and wait for completion */
    retVal = VISA_call(visa, (VISA_Msg *)&msg);

    /*
     * Regardless of return value, unmarshall outArgs.
     */
    retVal = unmarshallMsg(h, inBufs, outBufs, inArgs, outArgs, msg, retVal);

    return (retVal);
}

/*
 *  ======== processAsync ========
 *  This is the async stub-implementation for the process method
 */
static XDAS_Int32 processAsync(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs)
{
    XDAS_Int32 retVal;
    _VIDTRANSCODE_Msg *msg;
    VISA_Handle visa = (VISA_Handle)h;

    retVal = marshallMsg(h, inBufs, outBufs, inArgs, outArgs, &msg);
    if (retVal != IVIDTRANSCODE_EOK) {
        return (retVal);
    }

    /* send the message to the skeleton without waiting for completion */
    retVal = VISA_callAsync(visa, (VISA_Msg *)&msg);

    return (retVal);
}

/*
 *  ======== processWait ========
 */
static XDAS_Int32 processWait(IVIDTRANSCODE_Handle h, XDM1_BufDesc *inBufs,
    XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, UInt timeout)
{
    XDAS_Int32 retVal;
    _VIDTRANSCODE_Msg *msg;
    VISA_Handle visa = (VISA_Handle)h;

    GT_assert(CURTRACE, !VISA_isLocal(visa));

    /* wait for completion of "last" message */
    retVal = VISA_wait(visa, (VISA_Msg *)&msg, timeout);

    if (msg != NULL) {
        GT_assert(CURTRACE, msg->visa.cmd == _VIDTRANSCODE_CPROCESS);

        retVal = unmarshallMsg(h, inBufs, outBufs, inArgs, outArgs, msg,
                               retVal);
    }

    return (retVal);
}

/*
 *  ======== control ========
 *  This is the stub-implementation for the control method
 */
static XDAS_Int32 control(IVIDTRANSCODE_Handle h, IVIDTRANSCODE_Cmd id,
    IVIDTRANSCODE_DynamicParams *dynParams, IVIDTRANSCODE_Status *status)
{
    XDAS_Int32 retVal;
    VISA_Handle visa = (VISA_Handle)h;
    _VIDTRANSCODE_Msg *msg;
    IVIDTRANSCODE_Status *pMsgStatus;
    XDAS_Int8 *virtAddr = NULL;

    /*
     * Validate arguments.  Do we want to do this _every_ time, or just in
     * debug builds via GT_assert()?
     */
    if ((dynParams == NULL) ||
        (dynParams->size < sizeof(IVIDTRANSCODE_DynamicParams)) ||
        (status == NULL) ||
        (status->size < sizeof(IVIDTRANSCODE_Status))) {

        /* invalid args, could even assert here, it's a spec violation. */
        return (IVIDTRANSCODE_EFAIL);
    }

    if (/* size of "stuff to marshall" > message size */
        (sizeof(VISA_MsgHeader) + sizeof(id) + dynParams->size +
        status->size) > VISA_getMaxMsgSize(visa)) {

        /* Can't handle these large extended args. */
        GT_0trace(CURTRACE, GT_6CLASS,
            "control> invalid arguments - validate .size fields\n");

        return (IVIDTRANSCODE_EUNSUPPORTED);
    }

    /* get a message appropriate for this algorithm */
    if ((msg = (_VIDTRANSCODE_Msg *)VISA_allocMsg(visa)) == NULL) {
        return (IVIDTRANSCODE_EFAIL);
    }

    /* marshall the command */
    msg->visa.cmd = _VIDTRANSCODE_CCONTROL;

    msg->cmd.control.id = id;

    /* dynParams has no pointers so simply memcpy "size" bytes into the msg */
    memcpy(&(msg->cmd.control.dynParams), dynParams, dynParams->size);

    /* point at status based on the "size" of dynParams */
    pMsgStatus =
        (IVIDTRANSCODE_Status *)((UInt)(&(msg->cmd.control.dynParams)) +
            dynParams->size);

    /*
     * Initialize the .size and .data fields - the rest are filled in by
     * the codec.
     */
    pMsgStatus->size = status->size;

    if (status->data.buf != NULL) {
        pMsgStatus->data.bufSize = status->data.bufSize;

        /* save it for later */
        virtAddr = status->data.buf;

        pMsgStatus->data.buf = (XDAS_Int8 *)
            Memory_getBufferPhysicalAddress(status->data.buf,
                status->data.bufSize, NULL);

        if (pMsgStatus->data.buf == NULL) {
            retVal = IVIDTRANSCODE_EFAIL;
            goto exit;
        }
    }
    else {
        /* Place null into the msg so the skel knows it's invalid */
        pMsgStatus->data.buf = NULL;
    }

    /* send the message to the skeleton and wait for completion */
    retVal = VISA_call(visa, (VISA_Msg *)&msg);

    /* ensure we get CCONTROL msg (ensure async CPROCESS pipeline drained) */
    GT_assert(CURTRACE, msg->visa.cmd == _VIDTRANSCODE_CCONTROL);

    /* unmarshall status */
    pMsgStatus =
        (IVIDTRANSCODE_Status *)((UInt)(&(msg->cmd.control.dynParams)) +
            dynParams->size);

    if (VISA_isChecked()) {
        /* ensure codec didn't modify status->size */
        GT_assert(CURTRACE, pMsgStatus->size == status->size);

        /*
         * TODO:L  Should we also check that pMsgStatus->data.buf is the same
         * after the call as before?
         */
    }
    memcpy(status, pMsgStatus, status->size);

    /*
     * And finally, restore status->data.buf to its original value.  Note that
     * this works even when status->data.buf was NULL because virtAddr is
     * initialized to NULL.
     *
     * While potentially more confusing, this is just as correct as
     * (and faster than!) calling Memory_getVirtualBuffer().
     */
    status->data.buf = virtAddr;

    /* Clear .accessMask; the local processor didn't access the buffer */
    status->data.accessMask = 0;

exit:
    VISA_freeMsg(visa, (VISA_Msg)msg);

    return (retVal);
}

/*
 *  ======== VIDTRANSCODE_processAsync ========
 */
XDAS_Int32 VIDTRANSCODE_processAsync(VIDTRANSCODE_Handle handle,
    XDM1_BufDesc *inBufs, XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs)
{
    XDAS_Int32 retVal = VIDTRANSCODE_EFAIL;

    /*
     * Note, we do this because someday we may allow dynamically changing
     * the global 'VISA_isChecked()' value on the fly.  If we allow that,
     * we need to ensure the value stays consistent in the context of this call.
     */
    Bool checked = VISA_isChecked();

    GT_5trace(CURTRACE, GT_ENTER, "VIDTRANSCODE_processAsync> "
        "Enter (handle=0x%x, inBufs=0x%x, outBufs=0x%x, inArgs=0x%x, "
        "outArgs=0x%x)\n", handle, inBufs, outBufs, inArgs, outArgs);

    if (handle) {
        IVIDTRANSCODE_Handle alg = VISA_getAlgHandle((VISA_Handle)handle);

        if (alg != NULL) {
            if (checked) {

                /* validate inArgs and outArgs */
                XdmUtils_validateExtendedStruct(inArgs, sizeof(inArgs),
                    "inArgs");
                XdmUtils_validateExtendedStruct(outArgs, sizeof(outArgs),
                    "outArgs");

                /*
                 * Validate inBufs and outBufs.
                 */
                XdmUtils_validateSparseBufDesc1(inBufs, "inBufs");
                XdmUtils_validateSparseBufDesc(outBufs, "outBufs");

                /*
                 * Zero out the outArgs struct (except for .size field);
                 * it's write-only to the codec, so the app shouldn't pass
                 * values through it, nor should the codec expect to
                 * receive values through it.
                 */
                memset((void *)((XDAS_Int32)(outArgs) + sizeof(outArgs->size)),
                    0, (sizeof(*outArgs) - sizeof(outArgs->size)));
            }

            retVal = processAsync(alg, inBufs, outBufs, inArgs, outArgs);
        }
    }

    GT_2trace(CURTRACE, GT_ENTER, "VIDTRANSCODE_processAsync> "
        "Exit (handle=0x%x, retVal=0x%x)\n", handle, retVal);

    return (retVal);
}

/*
 *  ======== VIDTRANSCODE_processWait ========
 */
XDAS_Int32 VIDTRANSCODE_processWait(VIDTRANSCODE_Handle handle,
    XDM1_BufDesc *inBufs, XDM_BufDesc *outBufs, IVIDTRANSCODE_InArgs *inArgs,
    IVIDTRANSCODE_OutArgs *outArgs, UInt timeout)
{
    XDAS_Int32 retVal = VIDTRANSCODE_EFAIL;
    VIDTRANSCODE_InArgs refInArgs;

    /*
     * Note, we do this because someday we may allow dynamically changing
     * the global 'VISA_isChecked()' value on the fly.  If we allow that,
     * we need to ensure the value stays consistent in the context of this call.
     */
    Bool checked = VISA_isChecked();

    GT_5trace(CURTRACE, GT_ENTER, "VIDTRANSCODE_processWait> "
        "Enter (handle=0x%x, inBufs=0x%x, outBufs=0x%x, inArgs=0x%x, "
        "outArgs=0x%x)\n", handle, inBufs, outBufs, inArgs, outArgs);

    if (handle) {
        IVIDTRANSCODE_Handle alg = VISA_getAlgHandle((VISA_Handle)handle);

        if (alg != NULL) {
            if (checked) {
                /*
                 * Make a reference copy of inArgs so we can check that
                 * the codec didn't modify them during process().
                 */
                refInArgs = *inArgs;
            }

            retVal = processWait(alg, inBufs, outBufs, inArgs, outArgs,
                                 timeout);

            if (checked) {
                /* ensure the codec didn't modify the read-only inArgs */
                if (memcmp(&refInArgs, inArgs, sizeof(*inArgs)) != 0) {
                    GT_1trace(CURTRACE, GT_7CLASS,
                        "ERROR> codec (0x%x) modified read-only inArgs "
                        "struct!\n", handle);
                }
            }
        }
    }

    GT_2trace(CURTRACE, GT_ENTER, "VIDTRANSCODE_processWait> "
        "Exit (handle=0x%x, retVal=0x%x)\n", handle, retVal);

    return (retVal);
}
/*
 *  @(#) ti.sdo.ce.vidtranscode; 1, 0, 1,227; 12-2-2010 21:28:34; /db/atree/library/trees/ce/ce-r11x/src/ xlibrary

 */

