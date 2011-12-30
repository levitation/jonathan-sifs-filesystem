#include "fileflt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsCommonLockControl)
#endif

FLT_PREOP_CALLBACK_STATUS
SifsCommonLockControl (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    BOOLEAN         CompleteContext = TRUE;
    BOOLEAN         CompleteIrp = TRUE;
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;
    PFILE_OBJECT    FileObject = NULL;
    PSIFS_FCB       Fcb = NULL;

    __try {

        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileObject = IrpContext->FileObject;

        Fcb = (PSIFS_FCB) FileObject->FsContext;

        ASSERT(Fcb != NULL);

        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        if (FlagOn(Fcb->Mcb->FileAttr, FILE_ATTRIBUTE_DIRECTORY)) {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        CompleteIrp = FALSE;

        Status = FltCheckOplock( &Fcb->Oplock,
                                   IrpContext->Data,
                                   IrpContext,
                                   SifsOplockComplete,
                                   NULL );

        if (Status != STATUS_SUCCESS) {
            CompleteContext = FALSE;
            __leave;
        }

        //
        // FsRtlProcessFileLock acquires FileObject->FsContext->Resource while
        // modifying the file locks and calls IoCompleteRequest when it's done.
        //

        Status = FsRtlProcessFileLock(
                     &Fcb->FileLockAnchor,
                     IrpContext->Data,
                     NULL );
        Fcb->Header.IsFastIoPossible = SifsIsFastIoPossible(Fcb);

    } __finally {

        IrpContext->Data->IoStatus.Status = Status;
		
	 if(Status == STATUS_PENDING) {

		retValue = FLT_PREOP_PENDING;
	 }	 
	 
        if (!IrpContext->ExceptionInProgress) {
			
            if (!CompleteIrp) {
                IrpContext->Data = NULL;
            }

            if (CompleteContext) {
                SifsCompleteIrpContext(IrpContext, Status);
            }
        }	
    }

    return retValue;
}

