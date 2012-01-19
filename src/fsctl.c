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
	PVOID InputBuffer = NULL;
	PVOID OutputBuffer = NULL;

	if(IsFlagOn(Data->Flags, FLTFL_CALLBACK_DATA_SYSTEM_BUFFER)) {

		InputBuffer = Data->Iopb->Parameters.FileSystemControl.Buffered.SystemBuffer;
		OutputBuffer = Data->Iopb->Parameters.FileSystemControl.Buffered.SystemBuffer;
	}else{

		InputBuffer = Data->Iopb->Parameters.FileSystemControl.Neither.InputBuffer;
		OutputBuffer = Data->Iopb->Parameters.FileSystemControl.Neither.OutputBuffer;
	}
	
 	Status = FltFsControlFile(IrpContext->FltObjects->Instance, Fcb->Mcb->Lower.FileObject
		, Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode
		, InputBuffer
		, Data->Iopb->Parameters.FileSystemControl.Common.InputBufferLength
		, OutputBuffer
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