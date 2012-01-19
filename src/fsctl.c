#include "fileflt.h"

FLT_PREOP_CALLBACK_STATUS
SifsCommonFileSystemControl (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
    PFLT_CALLBACK_DATA      Data = IrpContext->Data;
    NTSTATUS            	Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT  Vcb = IrpContext->VolumeContext;
    PFILE_OBJECT         FileObject = IrpContext->FileObject;
    PSIFS_FCB        	Fcb = FileObject->FsContext;   
    BOOLEAN                bCompleteRequest = TRUE;
    

    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

    __try {

	ULONG LengthReturned = 0;
	
	
 	Status = FltFsControlFile(IrpContext->FltObjects->Instance, Fcb->Mcb->Lower.FileObject
		, Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode
		, Data->Iopb->Parameters.FileSystemControl.Neither.InputBuffer
		, Data->Iopb->Parameters.FileSystemControl.Common.InputBufferLength
		, Data->Iopb->Parameters.FileSystemControl.Neither.OutputBuffer
		, Data->Iopb->Parameters.FileSystemControl.Common.OutputBufferLength
		, &LengthReturned);

	Data->IoStatus.Information = LengthReturned;
	 
    } __finally {

	 Data->IoStatus.Status = Status;
	 
        if (bCompleteRequest) {
           SifsCompleteIrpContext(IrpContext, Status);
        }

	 if((Status == STATUS_PENDING)
	 	|| (Status == STATUS_CANT_WAIT)){

		retValue = FLT_PREOP_PENDING;
	}
    }
 	
    return retValue;
}