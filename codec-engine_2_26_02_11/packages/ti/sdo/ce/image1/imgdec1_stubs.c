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
 *  ======== imgdec1_stubs.c ========
 *  This file contains an implemenation of the IIMGDEC1 interface for the
 *  image decoder class of algorithms.
 *
 *  These functions are the "client-side" of a "remote" implementation.
 *
 */
#include <xdc/std.h>
#include <ti/sdo/ce/visa.h>
#include <ti/xdais/dm/iimgdec1.h>
#include <ti/sdo/ce/osal/Memory.h>
#include <ti/sdo/utils/trace/gt.h>

#include <string.h>  /* for memcpy and memset.
                      * (TODO:L Should we introduce these in Memory_*? */

#include <ti/sdo/ce/utils/xdm/XdmUtils.h>

#include "imgdec1.h"
#include "_imgdec1.h"

static XDAS_Int32 process(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs);
static XDAS_Int32 processAsync(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs);
static XDAS_Int32 processWait(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs,
    UInt timeout);
static XDAS_Int32 control(IIMGDEC1_Handle h, IIMGDEC1_Cmd id,
    IIMGDEC1_DynamicParams *params, IIMGDEC1_Status *status);
static XDAS_Int32 marshallMsg(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs,
    _IMGDEC1_Msg **pmsg);
static Void unmarshallMsg(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs,
    _IMGDEC1_Msg *msg);


IIMGDEC1_Fxns IMGDEC1_STUBS = {
    {&IMGDEC1_STUBS, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL},
    process, control,
};

extern GT_Mask ti_sdo_ce_image1_IMGDEC1_curTrace;
#define CURTRACE ti_sdo_ce_image1_IMGDEC1_curTrace

/*
 *  ======== marshallMsg ========
 */
static XDAS_Int32 marshallMsg(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs,
    _IMGDEC1_Msg **pmsg)
{
    XDAS_Int32 retVal = IIMGDEC1_EOK;
    VISA_Handle visa = (VISA_Handle)h;
    _IMGDEC1_Msg *msg;
    Int i;
    IIMGDEC1_OutArgs *pMsgOutArgs;
    Int numBufs;
    Int payloadSize;

    /*
     * Validate arguments.  Do we want to do this _every_ time, or just in
     * checked builds?
     */
    if ((inArgs == NULL) || (inArgs->size < sizeof(IIMGDEC1_InArgs)) ||
            (outArgs == NULL) || (outArgs->size < sizeof(IIMGDEC1_OutArgs))) {

        /* invalid args, could even assert here, it's a spec violation. */
        return (IIMGDEC1_EFAIL);
    }

    if (pmsg == NULL) {
        return (IIMGDEC1_EFAIL);
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

        return (IIMGDEC1_EUNSUPPORTED);
    }

    /* get a message appropriate for this algorithm */
    if ((msg = (_IMGDEC1_Msg *)VISA_allocMsg(visa)) == NULL) {
        return (IIMGDEC1_EFAIL);
    }

    /* zero out msg->cmd (not msg->visa!) */
    memset(&(msg->cmd), 0, sizeof(msg->cmd));

    /*
     * Marshall the command: copy the client-passed arguments into flattened
     * message data structures, converting every pointer address to alg.
     * data buffer into physical address.
     */

    /* First specify the processing command that the skeleton should do */
    msg->visa.cmd = _IMGDEC1_CPROCESS;

    /* commentary follows for marshalling the inBufs argument: */

    /* 1) inBufs->numBufs is a plain integer, we just copy it */
    msg->cmd.process.inBufs.numBufs = inBufs->numBufs;

    /*
     * 2) inBufs->bufSizes is a sparse array of integers, we copy them all.
     *
     * 3) inBufs->bufs is a pointer to a sparse array of pointers, so we take
     * individual pointers, convert them if non-NULL, and store them in the
     * message counterpart of inBufs->bufs.
     */
    for (i = 0, numBufs = 0; i < XDM_MAX_IO_BUFFERS; i++) {
        if (inBufs->descs[i].buf != NULL) {
            /* valid member of sparse array, convert it */
            msg->cmd.process.inBufs.descs[i].bufSize = inBufs->descs[i].bufSize;

            msg->cmd.process.inBufs.descs[i].buf = (XDAS_Int8 *)
                Memory_getBufferPhysicalAddress(inBufs->descs[i].buf,
                    inBufs->descs[i].bufSize, NULL);

            if (msg->cmd.process.inBufs.descs[i].buf == NULL) {
                retVal = IIMGDEC1_EFAIL;
                goto exit;
            }

            /* Clear .accessMask; the local processor won't access this buf */
            inBufs->descs[i].accessMask = 0;

            /* found, and handled, another buffer.  See if it's the last one */
            if (++numBufs == inBufs->numBufs) {
                break;
            }
        }
        else {
            /* empty member of sparse array, no conversion needed. */
            msg->cmd.process.inBufs.descs[i].bufSize = 0;
            msg->cmd.process.inBufs.descs[i].buf = NULL;
        }
    }

    /* we're done (with inBufs). Because msg->cmd.process is non-cacheable
     * and contiguous (it has been allocated by MSGQ), we don't have to do
     * anything else.
     */

    /* Now we repeat the procedure for outBufs. Note that
     * inArgs and outArgs contain no pointers, so we can simply copy the
     * entire original structure, accounting for the first "size" field.
     */
    msg->cmd.process.outBufs.numBufs = outBufs->numBufs;

    for (i = 0, numBufs = 0; i < XDM_MAX_IO_BUFFERS; i++) {
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
                retVal = IIMGDEC1_EFAIL;
                goto exit;
            }

            /* Clear .accessMask; the local processor won't access this buf */
            outBufs->descs[i].accessMask = 0;

            /* found, and handled, another buffer.  See if it's the last one */
            if (++numBufs == outBufs->numBufs) {
                break;
            }
        }
        else {
            /* empty member of sparse array, no conversion needed */
            msg->cmd.process.outBufs.descs[i].bufSize = 0;
            msg->cmd.process.outBufs.descs[i].buf = NULL;
        }
    }

    /* inArgs has no pointers so simply memcpy "size" bytes into the msg */
    memcpy(&(msg->cmd.process.inArgs), inArgs, inArgs->size);

    /* point at outArgs and set the "size" */
    pMsgOutArgs = (IIMGDEC1_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
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
static Void unmarshallMsg(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs,
    _IMGDEC1_Msg *msg)
{
    VISA_Handle visa = (VISA_Handle)h;
    IIMGDEC1_OutArgs *pMsgOutArgs;

    /*
     * Do a wholesale replace of skeleton returned structure.
     */
    pMsgOutArgs = (IIMGDEC1_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
        inArgs->size);

    if (VISA_isChecked()) {
        /* ensure the codec didn't change outArgs->size */
        GT_assert(CURTRACE, pMsgOutArgs->size == outArgs->size);
    }

    memcpy(outArgs, pMsgOutArgs, outArgs->size);

    /*
     * Note, we can keep the original outBufs, since image
     * decoder codecs return buffers in order, and the caller's
     * outBufs already contains the virtual buffer pointers.
     *
     * Note also that we did *nothing* with inBufs nor inArgs.
     *
     * Note also that we _don't_ update inBufs->descs[].accessMask.
     * That's accurate, since the local processor  _didn't_
     * read the inBuf.
     */

    VISA_freeMsg(visa, (VISA_Msg)msg);

    return;
}

/*
 *  ======== process ========
 *  This is the stub-implementation for the process method
 */
static XDAS_Int32 process(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs)
{
    XDAS_Int32 retVal;
    VISA_Handle visa = (VISA_Handle)h;
    _IMGDEC1_Msg *msg;

    retVal = marshallMsg(h, inBufs, outBufs, inArgs, outArgs, &msg);
    if (retVal != IIMGDEC1_EOK) {
        return (retVal);
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
 *  This is the async stub implementation for the process method
 */
static XDAS_Int32 processAsync(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs)
{
    XDAS_Int32 retVal;
    VISA_Handle visa = (VISA_Handle)h;
    _IMGDEC1_Msg *msg;

    retVal = marshallMsg(h, inBufs, outBufs, inArgs, outArgs, &msg);
    if (retVal != IIMGDEC1_EOK) {
        return (retVal);
    }

    /* send the message to the skeleton without waiting for completion */
    retVal = VISA_callAsync(visa, (VISA_Msg *)&msg);

    return (retVal);
}

/*
 *  ======== processWait ========
 */
static XDAS_Int32 processWait(IIMGDEC1_Handle h, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IIMGDEC1_InArgs *inArgs, IIMGDEC1_OutArgs *outArgs,
    UInt timeout)
{
    XDAS_Int32 retVal;
    VISA_Handle visa = (VISA_Handle)h;
    _IMGDEC1_Msg *msg;

    GT_assert(CURTRACE, !VISA_isLocal(visa));

    /* wait for completion of "last" message */
    retVal = VISA_wait(visa, (VISA_Msg *)&msg, timeout);

    /*
     * Unmarshall outArgs if there is a msg to unmarshall.
     */
    if (msg != NULL) {
        GT_assert(CURTRACE, msg->visa.cmd == _IMGDEC1_CPROCESS);

        unmarshallMsg(h, inBufs, outBufs, inArgs, outArgs, msg);
    }

    return (retVal);
}

/*
 *  ======== control ========
 *  This is the stub-implementation for the control method
 */
static XDAS_Int32 control(IIMGDEC1_Handle h, IIMGDEC1_Cmd id,
    IIMGDEC1_DynamicParams *params, IIMGDEC1_Status *status)
{
    XDAS_Int32 retVal;
    VISA_Handle visa = (VISA_Handle)h;
    _IMGDEC1_Msg *msg;
    IIMGDEC1_Status *pMsgStatus;
    XDAS_Int8 *virtAddr = NULL;
    Int payloadSize;

    /*
     * Validate arguments.  Do we want to do this _every_ time, or just in
     * checked builds?
     */
    if ((params == NULL) || (params->size < sizeof(IIMGDEC1_DynamicParams)) ||
            (status == NULL) || (status->size < sizeof(IIMGDEC1_Status))) {

        /* invalid args, could even assert here, it's a spec violation. */
        return (IIMGDEC1_EFAIL);
    }

    /* make sure it'll all fit! */
    payloadSize = sizeof(VISA_MsgHeader) + sizeof(id) + params->size +
            status->size;

    if (payloadSize > VISA_getMaxMsgSize(visa)) {
        /* Can't handle these large extended args. */
        GT_2trace(CURTRACE, GT_6CLASS,
                "process> invalid arguments - too big (0x%x > 0x%x).  "
                "Validate .size fields\n", payloadSize,
                VISA_getMaxMsgSize(visa));

        return (IIMGDEC1_EUNSUPPORTED);
    }

    /* get a message appropriate for this algorithm */
    if ((msg = (_IMGDEC1_Msg *)VISA_allocMsg(visa)) == NULL) {
        return (IIMGDEC1_EFAIL);
    }

    /* marshall the command */
    msg->visa.cmd = _IMGDEC1_CCONTROL;

    msg->cmd.control.id = id;

    /* params has no pointers so simply memcpy "size" bytes into the msg */
    memcpy(&(msg->cmd.control.params), params, params->size);

    /* unmarshall status based on the "size" of params */
    pMsgStatus = (IIMGDEC1_Status *)((UInt)(&(msg->cmd.control.params)) +
        params->size);

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
            retVal = IIMGDEC1_EFAIL;
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
    GT_assert(CURTRACE, msg->visa.cmd == _IMGDEC1_CCONTROL);

    /* unmarshall status */
    pMsgStatus = (IIMGDEC1_Status *)((UInt)(&(msg->cmd.control.params)) +
        params->size);

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
 *  ======== IMGDEC1_processAsync ========
 */
XDAS_Int32 IMGDEC1_processAsync(IMGDEC1_Handle handle, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IMGDEC1_InArgs *inArgs, IMGDEC1_OutArgs *outArgs)
{
    XDAS_Int32 retVal = IMGDEC1_EFAIL;

    /*
     * Note, we do this because someday we may allow dynamically changing
     * the global 'VISA_isChecked()' value on the fly.  If we allow that,
     * we need to ensure the value stays consistent in the context of this call.
     */
    Bool checked = VISA_isChecked();

    GT_5trace(CURTRACE, GT_ENTER, "IMGDEC1_processAsync> "
        "Enter (handle=0x%x, inBufs=0x%x, outBufs=0x%x, inArgs=0x%x, "
        "outArgs=0x%x)\n", handle, inBufs, outBufs, inArgs, outArgs);

    if (handle) {
        IIMGDEC1_Handle alg = VISA_getAlgHandle((VISA_Handle)handle);

        if (alg != NULL) {
            if (checked) {
                /* ensure valid sizes */
                if (inArgs->size < sizeof(IIMGDEC1_InArgs)) {
                    GT_2trace(CURTRACE, GT_7CLASS,
                        "ERROR> app provided codec (0x%x) with incorrectly "
                        "sized InArgs struct (0x%x)"
                        "field!\n", handle, inArgs->size);
                }

                if (outArgs->size < sizeof(IIMGDEC1_OutArgs)) {
                    GT_2trace(CURTRACE, GT_7CLASS,
                        "ERROR> app provided codec (0x%x) with incorrectly "
                        "sized outArgs struct (0x%x)"
                        "field!\n", handle, outArgs->size);
                }

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

    GT_2trace(CURTRACE, GT_ENTER, "IMGDEC1_processAsync> "
        "Exit (handle=0x%x, retVal=0x%x)\n", handle, retVal);

    return (retVal);
}

/*
 *  ======== IMGDEC1_processWait ========
 */
XDAS_Int32 IMGDEC1_processWait(IMGDEC1_Handle handle, XDM1_BufDesc *inBufs,
    XDM1_BufDesc *outBufs, IMGDEC1_InArgs *inArgs, IMGDEC1_OutArgs *outArgs,
    UInt timeout)
{
    XDAS_Int32 retVal = IMGDEC1_EFAIL;
    IMGDEC1_InArgs refInArgs;

    /*
     * Note, we do this because someday we may allow dynamically changing
     * the global 'VISA_isChecked()' value on the fly.  If we allow that,
     * we need to ensure the value stays consistent in the context of this call.
     */
    Bool checked = VISA_isChecked();

    GT_5trace(CURTRACE, GT_ENTER, "IMGDEC1_processWait> "
        "Enter (handle=0x%x, inBufs=0x%x, outBufs=0x%x, inArgs=0x%x, "
        "outArgs=0x%x)\n", handle, inBufs, outBufs, inArgs, outArgs);

    if (handle) {
        IIMGDEC1_Handle alg = VISA_getAlgHandle((VISA_Handle)handle);

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

    GT_2trace(CURTRACE, GT_ENTER, "IMGDEC1_processWait> "
        "Exit (handle=0x%x, retVal=0x%x)\n", handle, retVal);

    return (retVal);
}
/*
 *  @(#) ti.sdo.ce.image1; 1, 0, 1,276; 12-2-2010 21:24:04; /db/atree/library/trees/ce/ce-r11x/src/ xlibrary

 */

