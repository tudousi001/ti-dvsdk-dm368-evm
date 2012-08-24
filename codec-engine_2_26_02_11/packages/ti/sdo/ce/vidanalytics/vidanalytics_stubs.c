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
 *  ======== vidanalytics_stubs.c ========
 *  This file contains an implemenation of the IVIDANALYTICS interface for the
 *  video analytics class of algorithms.
 *
 *  These functions are the "client-side" of a "remote" implementation.
 *
 */
#include <xdc/std.h>
#include <ti/sdo/ce/visa.h>
#include <ti/xdais/dm/ividanalytics.h>
#include <ti/sdo/ce/osal/Memory.h>
#include <ti/sdo/utils/trace/gt.h>

#include <string.h>  /* for memcpy and memset.
                      * (TODO:L Should we introduce these in Memory_*? */

#include <ti/sdo/ce/utils/xdm/XdmUtils.h>

#include "vidanalytics.h"
#include "_vidanalytics.h"

static XDAS_Int32 process(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs);
static XDAS_Int32 processAsync(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs);
static XDAS_Int32 processWait(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, UInt timeout);
static XDAS_Int32 control(IVIDANALYTICS_Handle h, IVIDANALYTICS_Cmd id,
    IVIDANALYTICS_DynamicParams *dynParams, IVIDANALYTICS_Status *status);
static XDAS_Int32 marshallMsg(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, _VIDANALYTICS_Msg **pmsg);
static Void unmarshallMsg(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, _VIDANALYTICS_Msg *msg);


IVIDANALYTICS_Fxns VIDANALYTICS_STUBS = {
    {&VIDANALYTICS_STUBS, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    process, control,
};

extern GT_Mask ti_sdo_ce_vidanalytics_VIDANALYTICS_curTrace;
#define CURTRACE ti_sdo_ce_vidanalytics_VIDANALYTICS_curTrace

/*
 *  ======== marshallMsg ========
 */
static XDAS_Int32 marshallMsg(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, _VIDANALYTICS_Msg **pmsg)
{
    XDAS_Int32 retVal = IVIDANALYTICS_EOK;
    VISA_Handle visa = (VISA_Handle)h;
    _VIDANALYTICS_Msg *msg;
    Int i;
    IVIDANALYTICS_OutArgs *pMsgOutArgs;
    Int numBufs;
    Int payloadSize;

    /*
     * Validate arguments.  Do we want to do this _every_ time, or just in
     * checked builds?
     */
    if ((inArgs == NULL) || (inArgs->size < sizeof(IVIDANALYTICS_InArgs)) ||
        (outArgs == NULL) || (outArgs->size < sizeof(IVIDANALYTICS_OutArgs))) {

        /* invalid args, could even assert here, it's a spec violation. */
        return (IVIDANALYTICS_EFAIL);
    }

    if (pmsg == NULL) {
        return (IVIDANALYTICS_EFAIL);
    }

    /* make sure it'll all fit! */
    payloadSize = sizeof(VISA_MsgHeader) + (sizeof(*inBufs)) +
            (sizeof(*outBufs)) + inArgs->size + outArgs->size;

    if (payloadSize > VISA_getMaxMsgSize(visa)) {
        /* Can't handle these large extended args. */
        GT_2trace(CURTRACE, GT_6CLASS,
                "process> invalid arguments - too big (0x%x > 0x%x).  "
                "Validate .size fields\n", payloadSize,
                VISA_getMaxMsgSize(visa));

        return (IVIDANALYTICS_EUNSUPPORTED);
    }

    /* get a message appropriate for this algorithm */
    if ((msg = (_VIDANALYTICS_Msg *)VISA_allocMsg(visa)) == NULL) {
        return (IVIDANALYTICS_EFAIL);
    }

    /* zero out msg->cmd (not msg->visa!) */
    memset(&(msg->cmd), 0, sizeof(msg->cmd));

    /*
     * Marshall the command: copy the client-passed arguments into flattened
     * message data structures, converting every pointer address to alg.
     * data buffer into physical address.
     */

    /* First specify the processing command that the skeleton should do */
    msg->visa.cmd = _VIDANALYTICS_CPROCESS;

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
                retVal = IVIDANALYTICS_EFAIL;
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
    msg->cmd.process.outBufs.numBufs = outBufs->numBufs;

    for (i = 0, numBufs = 0;
         ((numBufs < outBufs->numBufs) && (i < XDM_MAX_IO_BUFFERS)); i++) {

        if (outBufs->descs[i].buf != NULL) {
            /* valid member of sparse array, convert it */
            msg->cmd.process.outBufs.descs[i].bufSize =
                outBufs->descs[i].bufSize;

            msg->cmd.process.outBufs.descs[i].buf = (XDAS_Int8 *)
                Memory_getBufferPhysicalAddress(outBufs->descs[i].buf,
                    outBufs->descs[i].bufSize, NULL);

            if (msg->cmd.process.outBufs.descs[i].buf == NULL) {
                /* TODO:M - should add at least a trace statement when trace
                 * is supported.  Another good idea is to return something
                 * more clear than EFAIL.
                 */
                retVal = IVIDANALYTICS_EFAIL;
                goto exit;
            }

            /* Clear .accessMask; the local processor won't access this buf */
            outBufs->descs[i].accessMask = 0;

            /* found, and handled, another buffer. */
            numBufs++;
        }
        else {
            /* empty member of sparse array, no conversion needed */
            msg->cmd.process.outBufs.descs[i].bufSize = 0;
            msg->cmd.process.outBufs.descs[i].buf = NULL;
        }
    }

    if (VISA_isChecked()) {
        /* check that we found outBufs->numBufs pointers in outBufs->bufs[] */
        GT_assert(CURTRACE, outBufs->numBufs == numBufs);
    }

    /* inArgs has no pointers so simply memcpy "size" bytes into the msg */
    memcpy(&(msg->cmd.process.inArgs), inArgs, inArgs->size);

    /* point at outArgs and set the "size" */
    pMsgOutArgs = (IVIDANALYTICS_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
        inArgs->size);

    /* set the size field - the rest is filled in by the codec */
    pMsgOutArgs->size = outArgs->size;

    *pmsg = msg;

    return (retVal);

exit:
    VISA_freeMsg(visa, (VISA_Msg)msg);

    return (retVal);
}

/*
 *  ======== unmarshallMsg ========
 */
static Void unmarshallMsg(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, _VIDANALYTICS_Msg *msg)
{
    VISA_Handle visa = (VISA_Handle)h;
    IVIDANALYTICS_OutArgs *pMsgOutArgs;

    /*
     * Do a wholesale replace of skeleton returned structure.
     */
    pMsgOutArgs = (IVIDANALYTICS_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
        inArgs->size);

    if (VISA_isChecked()) {
        /* ensure the codec didn't change outArgs->size */
        GT_assert(CURTRACE, pMsgOutArgs->size == outArgs->size);
    }

    /* outArgs simply contains scalars, so just memcpy them */
    memcpy(outArgs, pMsgOutArgs, outArgs->size);

    /* Note that we did *nothing* with inBufs nor inArgs.  This should be ok. */

    VISA_freeMsg(visa, (VISA_Msg)msg);

    return;
}

/*
 *  ======== process ========
 *  This is the sync stub-implementation for the process method
 */
static XDAS_Int32 process(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs)
{
    XDAS_Int32 retVal;
    _VIDANALYTICS_Msg *msg;
    VISA_Handle visa = (VISA_Handle)h;

    retVal = marshallMsg(h, inBufs, outBufs, inArgs, outArgs, &msg);
    if (retVal != IVIDANALYTICS_EOK) {
        return retVal;
    }

    /* send the message to the skeleton and wait for completion */
    retVal = VISA_call(visa, (VISA_Msg *)&msg);

    /*
     * Regardless of return value, unmarshall outArgs.
     */
    unmarshallMsg(h, inBufs, outBufs, inArgs, outArgs, msg);

    return (retVal);
}

/*
 *  ======== processAsync ========
 *  This is the async stub-implementation for the process method
 */
static XDAS_Int32 processAsync(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs)
{
    XDAS_Int32 retVal;
    _VIDANALYTICS_Msg *msg;
    VISA_Handle visa = (VISA_Handle)h;

    retVal = marshallMsg(h, inBufs, outBufs, inArgs, outArgs, &msg);
    if (retVal != IVIDANALYTICS_EOK) {
        return (retVal);
    }

    /* send the message to the skeleton without waiting for completion */
    retVal = VISA_callAsync(visa, (VISA_Msg *)&msg);

    return (retVal);
}

/*
 *  ======== processWait ========
 */
static XDAS_Int32 processWait(IVIDANALYTICS_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, UInt timeout)
{
    XDAS_Int32 retVal;
    _VIDANALYTICS_Msg *msg;
    VISA_Handle visa = (VISA_Handle)h;

    GT_assert(CURTRACE, !VISA_isLocal(visa));

    /* wait for completion of "last" message */
    retVal = VISA_wait(visa, (VISA_Msg *)&msg, timeout);

    /*
     * Unmarshall outArgs if there is a msg to unmarshall.
     */
    if (msg != NULL) {
        GT_assert(CURTRACE, msg->visa.cmd == _VIDANALYTICS_CPROCESS);

        unmarshallMsg(h, inBufs, outBufs, inArgs, outArgs, msg);
    }

    return (retVal);
}

/*
 *  ======== control ========
 *  This is the stub-implementation for the control method
 */
static XDAS_Int32 control(IVIDANALYTICS_Handle h, IVIDANALYTICS_Cmd id,
    IVIDANALYTICS_DynamicParams *dynParams, IVIDANALYTICS_Status *status)
{
    XDAS_Int32 retVal;
    VISA_Handle visa = (VISA_Handle)h;
    _VIDANALYTICS_Msg *msg;
    IVIDANALYTICS_Status *pMsgStatus;
    XDAS_Int8 *virtAddr[XDM_MAX_IO_BUFFERS];
    Int i;
    Int numBufs;
    Int payloadSize;

    /*
     * Validate arguments.  Do we want to do this _every_ time, or just in
     * checked builds?
     */
    if ((dynParams == NULL) ||
            (dynParams->size < sizeof(IVIDANALYTICS_DynamicParams)) ||
            (status == NULL) || (status->size < sizeof(IVIDANALYTICS_Status))) {

        /* invalid args, could even assert here, it's a spec violation. */
        return (IVIDANALYTICS_EFAIL);
    }

    /* make sure it'll all fit! */
    payloadSize = sizeof(VISA_MsgHeader) + sizeof(id) + dynParams->size +
            status->size;

    if (payloadSize > VISA_getMaxMsgSize(visa)) {
        /* Can't handle these large extended args. */
        GT_2trace(CURTRACE, GT_6CLASS,
                "process> invalid arguments - too big (0x%x > 0x%x).  "
                "Validate .size fields\n", payloadSize,
                VISA_getMaxMsgSize(visa));

        return (IVIDANALYTICS_EUNSUPPORTED);
    }

    /* get a message appropriate for this algorithm */
    if ((msg = (_VIDANALYTICS_Msg *)VISA_allocMsg(visa)) == NULL) {
        return (IVIDANALYTICS_EFAIL);
    }

    /* marshall the command */
    msg->visa.cmd = _VIDANALYTICS_CCONTROL;

    msg->cmd.control.id = id;

    /* dynParams has no pointers so simply memcpy "size" bytes into the msg */
    memcpy(&(msg->cmd.control.dynParams), dynParams, dynParams->size);

    /* point at status based on the "size" of dynParams */
    pMsgStatus =
        (IVIDANALYTICS_Status *)((UInt)(&(msg->cmd.control.dynParams)) +
            dynParams->size);

    /*
     * Initialize the .size and .data fields - the rest are filled in by
     * the codec.
     */
    pMsgStatus->size = status->size;

    /* 1) pMsgStatus->data.numBufs is a plain integer, we just copy it */
    pMsgStatus->data.numBufs = status->data.numBufs;

    /*
     * status->data.descs[] is a sparse array of buffer descriptors.  Convert
     * them if non-NULL.
     */
    for (i = 0, numBufs = 0;
         ((numBufs < status->data.numBufs) && (i < XDM_MAX_IO_BUFFERS)); i++) {

        if (status->data.descs[i].buf != NULL) {
            /* valid member of sparse array, convert it */
            pMsgStatus->data.descs[i].bufSize = status->data.descs[i].bufSize;

            /* save it for later */
            virtAddr[i] = status->data.descs[i].buf;

            pMsgStatus->data.descs[i].buf = (XDAS_Int8 *)
                Memory_getBufferPhysicalAddress(status->data.descs[i].buf,
                    status->data.descs[i].bufSize, NULL);

            if (pMsgStatus->data.descs[i].buf == NULL) {
                retVal = IVIDANALYTICS_EFAIL;
                goto exit;
            }

            /* found, and handled, another buffer. */
            numBufs++;
        }
        else {
            /* empty member of sparse array, no conversion needed. */
            pMsgStatus->data.descs[i].bufSize = 0;
            pMsgStatus->data.descs[i].buf = NULL;

            virtAddr[i] = NULL;

        }
    }

    if (VISA_isChecked()) {
        /* check that we found inBufs->numBufs pointers in inBufs->bufs[] */
        GT_assert(CURTRACE, status->data.numBufs == numBufs);
    }

    /* send the message to the skeleton and wait for completion */
    retVal = VISA_call(visa, (VISA_Msg *)&msg);

    /* ensure we get CCONTROL msg (ensure async CPROCESS pipeline drained) */
    GT_assert(CURTRACE, msg->visa.cmd == _VIDANALYTICS_CCONTROL);

    /* unmarshall status */
    pMsgStatus =
        (IVIDANALYTICS_Status *)((UInt)(&(msg->cmd.control.dynParams)) +
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
     * And finally, restore status->data.descs[].buf's to their original values.
     *
     * While potentially more confusing, this is just as correct as
     * (and faster than!) calling Memory_getVirtualBuffer().
     */
    for (i = 0, numBufs = 0;
         (numBufs < status->data.numBufs) && (i < XDM_MAX_IO_BUFFERS); i++) {

        status->data.descs[i].buf = virtAddr[i];

        /* Clear .accessMask; the local processor didn't access the buffer */
        status->data.descs[i].accessMask = 0;

        if (virtAddr[i] != NULL) {
            numBufs++;
        }
    }

exit:
    VISA_freeMsg(visa, (VISA_Msg)msg);

    return (retVal);
}

/*
 *  ======== VIDANALYTICS_processAsync ========
 */
XDAS_Int32 VIDANALYTICS_processAsync(VIDANALYTICS_Handle handle,
    XDM1_BufDesc *inBufs, XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs)
{
    XDAS_Int32 retVal = VIDANALYTICS_EFAIL;

    /*
     * Note, we do this because someday we may allow dynamically changing
     * the global 'VISA_isChecked()' value on the fly.  If we allow that,
     * we need to ensure the value stays consistent in the context of this call.
     */
    Bool checked = VISA_isChecked();

    GT_5trace(CURTRACE, GT_ENTER, "VIDANALYTICS_processAsync> "
        "Enter (handle=0x%x, inBufs=0x%x, outBufs=0x%x, inArgs=0x%x, "
        "outArgs=0x%x)\n", handle, inBufs, outBufs, inArgs, outArgs);

    if (handle) {
        IVIDANALYTICS_Handle alg = VISA_getAlgHandle((VISA_Handle)handle);

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
                XdmUtils_validateSparseBufDesc1(outBufs, "outBufs");

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

    GT_2trace(CURTRACE, GT_ENTER, "VIDANALYTICS_processAsync> "
        "Exit (handle=0x%x, retVal=0x%x)\n", handle, retVal);

    return (retVal);
}

/*
 *  ======== VIDANALYTICS_processWait ========
 */
XDAS_Int32 VIDANALYTICS_processWait(VIDANALYTICS_Handle handle,
    XDM1_BufDesc *inBufs, XDM1_BufDesc *outBufs, IVIDANALYTICS_InArgs *inArgs,
    IVIDANALYTICS_OutArgs *outArgs, UInt timeout)
{
    XDAS_Int32 retVal = VIDANALYTICS_EFAIL;
    VIDANALYTICS_InArgs refInArgs;

    /*
     * Note, we do this because someday we may allow dynamically changing
     * the global 'VISA_isChecked()' value on the fly.  If we allow that,
     * we need to ensure the value stays consistent in the context of this call.
     */
    Bool checked = VISA_isChecked();

    GT_5trace(CURTRACE, GT_ENTER, "VIDANALYTICS_processWait> "
        "Enter (handle=0x%x, inBufs=0x%x, outBufs=0x%x, inArgs=0x%x, "
        "outArgs=0x%x)\n", handle, inBufs, outBufs, inArgs, outArgs);

    if (handle) {
        IVIDANALYTICS_Handle alg = VISA_getAlgHandle((VISA_Handle)handle);

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

    GT_2trace(CURTRACE, GT_ENTER, "VIDANALYTICS_processWait> "
        "Exit (handle=0x%x, retVal=0x%x)\n", handle, retVal);

    return (retVal);
}
/*
 *  @(#) ti.sdo.ce.vidanalytics; 1, 0, 1,209; 12-2-2010 21:28:17; /db/atree/library/trees/ce/ce-r11x/src/ xlibrary

 */

