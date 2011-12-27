#include "fileflt.h"

NTSTATUS
SifsFlushFile (
    __in PSIFS_IRP_CONTEXT    IrpContext,
    __in PSIFS_FCB            Fcb,
    __in PSIFS_CCB            Ccb
	)
{
    IO_STATUS_BLOCK    IoStatus;

    ASSERT(Fcb != NULL);
    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

    /* update timestamp and achieve attribute */
    if (Ccb != NULL) {

        if (!IsFlagOn(Ccb->Flags, CCB_LAST_WRITE_UPDATED)) {

            LARGE_INTEGER   SysTime;
            KeQuerySystemTime(&SysTime);

            Fcb->Lower->ChangeTime.QuadPart = FsLinuxTime(SysTime);
            Fcb->Lower->LastWriteTime = FsNtTime(Fcb->Lower->ChangeTime.QuadPart);            
        }
    }

    CcFlushCache(&(Fcb->SectionObject), NULL, 0, &IoStatus);
    ClearFlag(Fcb->Flags, FCB_FILE_MODIFIED);

    return IoStatus.Status;
}

FLT_PREOP_CALLBACK_STATUS
SifsCommonFlushBuffers (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;

	NTSTATUS                Status = STATUS_SUCCESS;

	PSIFS_FCB               Fcb = NULL;
	PSIFS_FCBVCB            FcbOrVcb = NULL;
	PSIFS_CCB               Ccb = NULL;
	PFILE_OBJECT            FileObject = NULL;
	PVOLUME_CONTEXT   VolumeContext = IrpContext->VolumeContext;
	PFLT_CALLBACK_DATA Data = IrpContext->Data;

	BOOLEAN                 MainResourceAcquired = FALSE;

	__try {

	    ASSERT(IrpContext);

	    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
	           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

	    if ( IsFlagOn(VolumeContext->Flags, VCB_READ_ONLY) ||
	            IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED)) {
	        Status =  STATUS_SUCCESS;
	        __leave;
	    }

	    FileObject = IrpContext->FileObject;
	    FcbOrVcb = (PSIFS_FCBVCB) FileObject->FsContext;
	    ASSERT(FcbOrVcb != NULL);

	    Ccb = (PSIFS_CCB) FileObject->FsContext2;
	    if (Ccb == NULL) {
	        Status =  STATUS_SUCCESS;
	        __leave;
	    }

	    MainResourceAcquired =
	        ExAcquireResourceExclusiveLite(&FcbOrVcb->MainResource,
	                                       IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT));

	    ASSERT(MainResourceAcquired);

	    if (FcbOrVcb->Identifier.Type == SIFSFCB) {

	        Fcb = (PSIFS_FCB)(FcbOrVcb);

	        Status = SifsFlushFile(IrpContext, Fcb, Ccb);
	        if (NT_SUCCESS(Status)) {
	            if (IsFlagOn(FileObject->Flags, FO_FILE_MODIFIED)) {
	                Fcb->Mcb->FileAttr |= FILE_ATTRIBUTE_ARCHIVE;
	                ClearFlag(FileObject->Flags, FO_FILE_MODIFIED);
	            }
	        }
	    }

	} __finally {

		
	    if (MainResourceAcquired) {
	        ExReleaseResourceLite(&FcbOrVcb->MainResource);
	    }

	    if (!IrpContext->ExceptionInProgress) {

		if(Fcb) {
			
			Status = FltFlushBuffers(IrpContext->FltObjects->Instance, Fcb->Lower->FileObject);
		}
		
	        SifsCompleteIrpContext(IrpContext, Status);
	    }

	    Data->IoStatus.Status = Status;
		
           if(Status == STATUS_PENDING) {

			retValue = FLT_PREOP_PENDING;
	    }
	}

	return retValue;
}
