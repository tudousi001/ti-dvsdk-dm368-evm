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
 *  ======== Queue_BIOS.c ========
 */
#include <ti/bios/include/std.h>
#include <ti/bios/include/que.h>
#include <ti/bios/include/hwi.h>

#include <ti/sdo/ce/osal/Queue.h>

Queue_Attrs Queue_ATTRS;

/*
 *  ======== Queue_exit ========
 */
Void Queue_exit(Void)
{
}

/*
 *  ======== Queue_get ========
 */
Ptr Queue_get(Queue_Handle queue)
{
    return (QUE_get((QUE_Handle)queue));
}

/*
 *  ======== Queue_init ========
 */
Bool Queue_init(Void)
{
    return (TRUE);
}

/*
 *  ======== Queue_put ========
 */
Void Queue_put(Queue_Handle queue, Ptr elem)
{
    QUE_put((QUE_Handle)queue, elem);
}

/*
 *  ======== Queue_extract ========
 */
Void Queue_extract(Queue_Elem *elem)
{
    Uns state = HWI_disable();

    QUE_remove(elem);

    HWI_restore(state);
}

/*
 *  @(#) ti.sdo.ce.osal.bios; 2, 0, 1,182; 12-2-2010 21:24:43; /db/atree/library/trees/ce/ce-r11x/src/ xlibrary

 */

