#include "fileflt.h"

VOID
SifsQueueCloseRequest (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

VOID
SifsDeQueueCloseRequest (
	__in PVOID Context
	);


#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsCommonClose)
#pragma alloc_text(PAGE, SifsQueueCloseRequest)
#pragma alloc_text(PAGE, SifsDeQueueCloseRequest)
#endif

FLT_PREOP_CALLBACK_STATUS
SifsCommonClose (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    NTSTATUS        Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT       Vcb = IrpContext->VolumeContext;
    BOOLEAN         VcbResourceAcquired = FALSE;
    PFILE_OBJECT    FileObject;
    PSIFS_FCB       Fcb;
    BOOLEAN         FcbResourceAcquired = FALSE;
    PSIFS_CCB       Ccb;
    BOOLEAN         bDeleteVcb = FALSE;
    BOOLEAN         bBeingClosed = FALSE;
    BOOLEAN         bSkipLeave = FALSE;

    __try {

        ASSERT(IrpContext != NULL);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        if (!ExAcquireResourceExclusiveLite(
                    &Vcb->MainResource,
                    IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) )) {

            Status = STATUS_PENDING;
            __leave;
        }
        VcbResourceAcquired = TRUE;

        bSkipLeave = TRUE;
        if (IsFlagOn(Vcb->Flags, VCB_BEING_CLOSED)) {
            bBeingClosed = TRUE;
        } else {
            SetLongFlag(Vcb->Flags, VCB_BEING_CLOSED);
            bBeingClosed = FALSE;
        }

        if (IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_DELAY_CLOSE)) {

            FileObject = NULL;
            Fcb = IrpContext->Fcb;
            Ccb = IrpContext->Ccb;

        } else {

            FileObject = IrpContext->FileObject;
            Fcb = (PSIFS_FCB) FileObject->FsContext;
            if (!Fcb) {
                Status = STATUS_SUCCESS;
                __leave;
            }
            ASSERT(Fcb != NULL);
            Ccb = (PSIFS_CCB) FileObject->FsContext2;
        }

        if ( (Fcb->Identifier.Type != SIFSFCB) ||
                (Fcb->Identifier.Size != sizeof(SIFS_FCB))) {
            __leave;
        }

        if (!ExAcquireResourceExclusiveLite(
                    &Fcb->MainResource,
                    IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) )) {
            Status = STATUS_PENDING;
            __leave;
        }
        FcbResourceAcquired = TRUE;

        Fcb->Header.IsFastIoPossible = FastIoIsNotPossible;

        if (!Ccb) {
            Status = STATUS_SUCCESS;
            __leave;
        }

        ASSERT((Ccb->Identifier.Type == SIFSCCB) &&
               (Ccb->Identifier.Size == sizeof(SIFS_CCB)));

        if (IsFlagOn(Fcb->Flags, FCB_STATE_BUSY)) {
            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_FILE_BUSY);
            Status = STATUS_PENDING;
            __leave;
        }

        LOG_PRINT(LOGFL_CLOSE, ( "fileFlt!SifsCommonClose: Fcb = %p OpenHandleCount= %u ReferenceCount=%u NonCachedCount=%u %wZ\n",
                        Fcb, Fcb->OpenHandleCount, Fcb->ReferenceCount, Fcb->NonCachedOpenCount, &Fcb->Mcb->FullName ));

        if (Ccb) {

            SifsFreeCcb(Ccb);

            if (FileObject) {
                FileObject->FsContext2 = Ccb = NULL;
            }
        }

        if (0 == SifsDerefXcb(&Fcb->ReferenceCount)) {

            //
            // Remove Fcb from Vcb->FcbList ...
            //

            if (FcbResourceAcquired) {
                ExReleaseResourceLite(&Fcb->MainResource);
                FcbResourceAcquired = FALSE;
            }           
			
            SifsFreeFcb(Fcb);

            if (FileObject) {
                FileObject->FsContext = Fcb = NULL;
            }	    
        }

        SifsDerefXcb(&Vcb->ReferenceCount);
        Status = STATUS_SUCCESS;

    } __finally {

        if (bSkipLeave && !bBeingClosed) {
            ClearFlag(Vcb->Flags, VCB_BEING_CLOSED);
        }

        if (FcbResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        if (VcbResourceAcquired) {
            ExReleaseResourceLite(&Vcb->MainResource);
        }

        if (!IrpContext->ExceptionInProgress) {

            if (Status == STATUS_PENDING) {

                SifsQueueCloseRequest(IrpContext);

		  retValue = FLT_PREOP_PENDING;

            } else {

		  IrpContext->Data->IoStatus.Status = Status;
		  
                SifsCompleteIrpContext(IrpContext, Status);
            }
        }else{

		IrpContext->Data->IoStatus.Status = Status;
        }
    }

    return retValue;
}

VOID
SifsQueueCloseRequest (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    ASSERT(IrpContext);
    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    if (IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_DELAY_CLOSE)) {

        if (IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_FILE_BUSY)) {
            FsSleep(500); /* 0.5 sec*/
        } else {
            FsSleep(50);  /* 0.05 sec*/
        }

    } else {

        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_DELAY_CLOSE);

        IrpContext->Fcb = (PSIFS_FCB) IrpContext->FileObject->FsContext;
        IrpContext->Ccb = (PSIFS_CCB) IrpContext->FileObject->FsContext2;
    }

    ExInitializeWorkItem(
        &IrpContext->WorkQueueItem,
        SifsDeQueueCloseRequest,
        IrpContext);

    ExQueueWorkItem(&IrpContext->WorkQueueItem, DelayedWorkQueue);
}

VOID
SifsDeQueueCloseRequest (
	__in PVOID Context
	)
{
    PSIFS_IRP_CONTEXT IrpContext;

    IrpContext = (PSIFS_IRP_CONTEXT) Context;
    ASSERT(IrpContext);
    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    __try {

        __try {

            FsRtlEnterFileSystem();
            SifsCommonClose(IrpContext);

        } __except (SifsExceptionFilter(IrpContext, GetExceptionInformation())) {

            SifsExceptionHandler(IrpContext);
        }

    } __finally {

        FsRtlExitFileSystem();
    }
}

