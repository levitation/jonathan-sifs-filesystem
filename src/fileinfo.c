#include "fileflt.h"

NTSTATUS
SifsIsFileRemovable(
    __in PSIFS_FCB            Fcb
	)
{
    if (!MmFlushImageSection(&Fcb->SectionObject,
                             MmFlushForDelete )) {
        return STATUS_CANNOT_DELETE;
    }    

    return STATUS_SUCCESS;
}

NTSTATUS
SifsExpandAndTruncateFile(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_MCB         Mcb,
    __in PLARGE_INTEGER    Size
	)
{
    NTSTATUS Status = STATUS_SUCCESS;
    FILE_ALLOCATION_INFORMATION fileAllocationInformation; 

    LONGLONG    Start = 0;
    LONGLONG    End = 0;

    Start = Mcb->Lower.AllocationSize.QuadPart;
    End = Size->QuadPart ;

    fileAllocationInformation.AllocationSize.QuadPart = End;

    Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
			, &fileAllocationInformation, sizeof(fileAllocationInformation), FileAllocationInformation);

    if(!NT_SUCCESS(Status)) {				

    	Size->QuadPart = Start;
    }else{

	Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
			, &fileAllocationInformation, sizeof(fileAllocationInformation), FileAllocationInformation, NULL);

	if(NT_SUCCESS(Status)) {

		Size->QuadPart = fileAllocationInformation.AllocationSize.QuadPart ;
		Mcb->Lower.AllocationSize.QuadPart = fileAllocationInformation.AllocationSize.QuadPart;
	}else{

	   	Size->QuadPart = End;
		Mcb->Lower.AllocationSize.QuadPart = End;
	}

	Status = STATUS_SUCCESS;
    }

    return Status;
}

NTSTATUS
SifsDeleteFile(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_FCB         Fcb,
    __in PSIFS_MCB         Mcb
	)
{
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;

    if (IsFlagOn(Mcb->Flags, MCB_FILE_DELETED)) {
        return STATUS_SUCCESS;
    }    
	
    __try {

        if(FsDeleteFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject) == 0){

		Status = STATUS_SUCCESS;

		SifsDerefXcb(Fcb);
		SifsDerefMcb(Mcb);
        }
    }__finally{
    
    }


    return Status;
}

