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
#include <xdc/std.h>
#include <ti/sdo/fc/rman/rman.h>
#include <ti/sdo/fc/utils/api/alg.h>
#include <ti/sdo/utils/trace/gt.h>

#ifdef xdc_target__isaCompatible_64P
#include <ti/sdo/fc/ires/vicp/iresman_vicp.h>
#else
#include <ti/sdo/fc/ires/vicp/iresman_vicp2.h>
#include <ti/sdo/linuxutils/cmem/include/cmem.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "ires_algorithm.h"
/*
 * extern declaration
 */
extern Int smain(Int argc, Char * argv[]);



Int main(Int argc, Char * argv[])
{
    /* To see internal tracing details, uncomment this line */
    GT_set("*=01234567");

#ifndef xdc_target__isaCompatible_64P
    /* non-BIOS based system; call smain() here */
    return (smain(argc, argv));
#else
    /* BIOS based system; return here and the static smain TSK will run */
    return (0);
#endif

}

Int smain(Int argc, Char * argv[])
{

    IRES_Status status;
    Int scratchId = 2;
    IALG_Fxns * algFxns = &DUMRES_TI_IALG;
    IRES_Fxns * resFxns = &DUMRES_TI_IRES;
    DUMRES_TI_Handle dumHandle = NULL;
    IALG_Status algStatus;
    Bool result = TRUE;

    /* Parse cmd line options */
    if (argc > 1) {
        scratchId = atoi(argv[1]);
    }

    /* Initialize the framework modules we're going to use */
    ALG_init();

    status = RMAN_init();
    if (IRES_OK != status) {
        printf("RMAN initialization Failed\n");
        return (-1);
    }

    /* Create an instance of the algorithm using "algFxns" */
    dumHandle = (DUMRES_TI_Handle)ALG_create( (IALG_Fxns *)algFxns,
            (IALG_Handle)NULL, (IALG_Params *)NULL);
    if (dumHandle == NULL) {
        printf("Alg creation failed\n");
        return -1;
    }
    else {
        printf("Alg 0x%x created scratch group %d\n", (unsigned int)dumHandle,
                (unsigned int)scratchId);
    }

    /* Create an instance of an algorithm that implements IALG and IRES_Fxns */
    status = RMAN_assignResources((IALG_Handle)dumHandle, resFxns,scratchId);
    if (IRES_OK != status) {
        printf("Assign Resource Failed\n");
        result = FALSE;
    }
    else {
        /* Activate the Algorithm */
        ALG_activate((IALG_Handle)dumHandle);

        /* Activate All Resources */
        RMAN_activateAllResources((IALG_Handle)dumHandle, resFxns, -1);

        /* Typically, here the app would use the alg to do something ... */

/* TODO: Call control API */
        (dumHandle)->alg.fxns->algControl((IALG_Handle)dumHandle,
                (IALG_Cmd )0, &algStatus);

        /* Deactivate All Resources */
        RMAN_deactivateAllResources((IALG_Handle)dumHandle, resFxns, -1);

        /* Deactivate algorithm */
        ALG_deactivate((IALG_Handle)dumHandle);

        /* Free resources assigned to this algorihtm */
        status = RMAN_freeResources((IALG_Handle)(dumHandle), resFxns,
                scratchId);
        if (IRES_OK != status) {
            printf("Free Resource Failed\n");
            result = FALSE;
        }
    }

    /* Delete the algorithm instance */
    if (dumHandle) {
        ALG_delete((IALG_Handle)dumHandle);
    }

    RMAN_exit();

    if (result) {
        printf("Application exiting normally\n");
    }
    else {
        printf("Error somewhere in the app.\n");
    }
    return (0);
}
/*
 *  @(#) ti.sdo.fc.rman.examples.vicpsync; 1, 0, 0,101; 12-1-2010 17:26:00; /db/atree/library/trees/fc/fc-p01x/src/ xlibrary

 */

