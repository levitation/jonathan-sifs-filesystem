#include "fileflt.h"

FLT_PREOP_CALLBACK_STATUS
SifsCommonQueryEa (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;

#if FLT_MGR_AFTER_XPSP2

	NTSTATUS Status = STATUS_SUCCESS;
	PSIFS_FCB     Fcb;
    	PFILE_OBJECT      FileObject;
	ULONG  LengthReturned = 0;
	
	ASSERT(IrpContext);

       ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

	__try{
		
	       FileObject = IrpContext->FileObject;

		Fcb = FileObject->FsContext;

		 if (Fcb->Identifier.Type == SIFSFCB) {
	                Status = FltQueryEaFile(IrpContext->FltObjects->Instance, Fcb->Lower->FileObject
						, SifsGetUserBufferOnQueryEa(IrpContext->Data->Iopb)
						, IrpContext->Data->Iopb->Parameters.QueryEa.Length
						, FlagOn(IrpContext->Data->Iopb->OperationFlags, SL_RETURN_SINGLE_ENTRY)
						, IrpContext->Data->Iopb->Parameters.QueryEa.EaList
						, IrpContext->Data->Iopb->Parameters.QueryEa.EaListLength
						, IrpContext->Data->Iopb->Parameters.QueryEa.EaIndex
						, IrpContext->Data->Iopb->Parameters.SetEa.Length
						, FlagOn(IrpContext->Data->Iopb->OperationFlags, SL_RESTART_SCAN)
						, &LengthReturned); 

			  IrpContext->Data->IoStatus.Information = LengthReturned;
			  
	        } else {
	            DbgBreak();

	            Status = STATUS_INVALID_PARAMETER;
	        }
	}__finally{

		IrpContext->Data->IoStatus.Status = Status;

		SifsCompleteIrpContext(IrpContext, Status);
	}

#endif /* FLT_MGR_AFTER_XPSP2 */

	return retValue;
}


FLT_PREOP_CALLBACK_STATUS
SifsCommonSetEa (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;

#if FLT_MGR_AFTER_XPSP2

	NTSTATUS Status = STATUS_SUCCESS;
	PSIFS_FCB     Fcb;
    	PFILE_OBJECT      FileObject;
	
	ASSERT(IrpContext);

       ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

	__try{
		
	       FileObject = IrpContext->FileObject;

		Fcb = FileObject->FsContext;

		 if (Fcb->Identifier.Type == SIFSFCB) {
	                Status = FltSetEaFile(IrpContext->FltObjects->Instance, Fcb->Lower->FileObject
						, SifsGetUserBufferOnSetEa(IrpContext->Data->Iopb), IrpContext->Data->Iopb->Parameters.SetEa.Length);               
	        } else {
	            DbgBreak();

	            Status = STATUS_INVALID_PARAMETER;
	        }
	}__finally{

		IrpContext->Data->IoStatus.Status = Status;

		SifsCompleteIrpContext(IrpContext, Status);
	}

#endif /* FLT_MGR_AFTER_XPSP2 */

	return retValue;
}
