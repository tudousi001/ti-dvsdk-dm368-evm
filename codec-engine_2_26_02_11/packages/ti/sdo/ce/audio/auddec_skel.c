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
 *  ======== auddec_skel.c ========
 *  This file contains the implemenation of the SKEL interface for the
 *  audio decoder class of algorithms.
 *
 *  These functions are the "server-side" of the the stubs defined in
 *  auddec_stubs.c
 */
#include <xdc/std.h>
#include <ti/sdo/ce/skel.h>
#include <ti/sdo/ce/osal/Memory.h>

#include "auddec.h"
#include "_auddec.h"

/*
 *  ======== call ========
 */
static VISA_Status call(VISA_Handle visaHandle, VISA_Msg visaMsg)
{
    _AUDDEC_Msg *msg  = (_AUDDEC_Msg *)visaMsg;
    AUDDEC_Handle handle = (AUDDEC_Handle)visaHandle;
    Int i;
    XDM_BufDesc inBufs, outBufs;
    IAUDDEC_OutArgs *pOutArgs;
    IAUDDEC_Status *pStatus;

    /* perform the requested AUDDEC operation by parsing message. */
    switch (msg->visa.cmd) {

        case _AUDDEC_CPROCESS: {
            /* unmarshall inBufs and outBufs since they differ in shape
             * from what their flattened versions passed in the message
             */
            inBufs.bufs      = msg->cmd.process.inBufs;
            inBufs.numBufs   = msg->cmd.process.numInBufs;
            inBufs.bufSizes  = msg->cmd.process.inBufSizes;
            outBufs.bufs     = msg->cmd.process.outBufs;
            outBufs.numBufs  = msg->cmd.process.numOutBufs;
            outBufs.bufSizes = msg->cmd.process.outBufSizes;

            if (SKEL_cachingPolicy == SKEL_LOCALBUFFERINVWB) {
                /* invalidate cache for all input buffers */
                for (i = 0; i < inBufs.numBufs; i++) {
                    Memory_cacheInv(inBufs.bufs[i], inBufs.bufSizes[i]);
                }

                /* invalidate cache for all output buffers */
                for (i = 0; i < outBufs.numBufs; i++) {
                    Memory_cacheInv(outBufs.bufs[i], outBufs.bufSizes[i]);
                }
            }

            /* unmarshall outArgs based on the "size" of inArgs */
            pOutArgs = (IAUDDEC_OutArgs *)((UInt)(&(msg->cmd.process.inArgs)) +
                msg->cmd.process.inArgs.size);

            /* make the process call */
            msg->visa.status = AUDDEC_process(handle,
                &inBufs, &outBufs, &(msg->cmd.process.inArgs), pOutArgs);

            if (SKEL_cachingPolicy == SKEL_WBINVALL) {
                Memory_cacheWbInvAll();
            }
            else if (SKEL_cachingPolicy == SKEL_LOCALBUFFERINVWB) {
                /* writeback cache for all output buffers  */
                for (i = 0; i < outBufs.numBufs; i++) {
                    Memory_cacheWb(outBufs.bufs[i], outBufs.bufSizes[i]);
                }
            }

            /*
             * Note that any changes to individual outBufs[i] values made by
             * the codec will automatically update msg->cmd.process.outBufs
             * as we pass the outBufs array by reference.
             */

            break;
        }

        case _AUDDEC_CCONTROL: {
            /* unmarshall status based on the "size" of params */
            pStatus = (IAUDDEC_Status *)((UInt)(&(msg->cmd.control.params)) +
                msg->cmd.control.params.size);

            msg->visa.status = AUDDEC_control(handle, msg->cmd.control.id,
                &(msg->cmd.control.params), pStatus);

             break;
        }

        default: {
            msg->visa.status = VISA_EFAIL;

            break;
        }
    }
    return (VISA_EOK);
}

/*
 *  ======== AUDDEC_SKEL ========
 */
SKEL_Fxns AUDDEC_SKEL = {
    call,
    (SKEL_CREATEFXN)&AUDDEC_create,
    (SKEL_DESTROYFXN)&AUDDEC_delete,
};
/*
 *  @(#) ti.sdo.ce.audio; 1, 0, 2,409; 12-2-2010 21:18:54; /db/atree/library/trees/ce/ce-r11x/src/ xlibrary

 */

