#include "fileflt.h"

NTSTATUS
SifsLockUserBuffer (
	__in  PFLT_CALLBACK_DATA Data,
       __in  ULONG            Length,
       __in  LOCK_OPERATION   Operation
       )
{
    NTSTATUS Status = STATUS_SUCCESS;
    PMDL         newMdl = NULL;
	
    ASSERT(Data != NULL);

    if(Data->Iopb->MajorFunction == IRP_MJ_READ) {
		
	    if (Data->Iopb->Parameters.Read.MdlAddress != NULL) {
			
	        return STATUS_SUCCESS;
	    }

	    newMdl = IoAllocateMdl(Data->Iopb->Parameters.Read.ReadBuffer, Length, FALSE, FALSE, NULL);

	    if(newMdl == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	    }

	    Data->Iopb->Parameters.Read.MdlAddress = newMdl;

	    MmBuildMdlForNonPagedPool(newMdl);
		
    }else if(Data->Iopb->MajorFunction == IRP_MJ_WRITE) {

	   if (Data->Iopb->Parameters.Write.MdlAddress != NULL) {
	   	
	        return STATUS_SUCCESS;
	    }

	   newMdl = IoAllocateMdl(Data->Iopb->Parameters.Write.WriteBuffer, Length, FALSE, FALSE, NULL);

	   if(newMdl == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	    }

	   Data->Iopb->Parameters.Write.MdlAddress = newMdl;

	   MmBuildMdlForNonPagedPool(newMdl);
    }else if((Data->Iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL)
               && (Data->Iopb->MinorFunction == IRP_MN_QUERY_DIRECTORY)) {

	   if(Data->Iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress != NULL) {

		return STATUS_SUCCESS;
	   }

	   newMdl = IoAllocateMdl(Data->Iopb->Parameters.DirectoryControl.QueryDirectory.DirectoryBuffer, Length, FALSE, FALSE, NULL);

	    if(newMdl == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	    }

	    Data->Iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress = newMdl;

	    MmBuildMdlForNonPagedPool(newMdl);
    }else if(Data->Iopb->MajorFunction == IRP_MJ_QUERY_EA) {

	   if(Data->Iopb->Parameters.QueryEa.MdlAddress != NULL) {

		return STATUS_SUCCESS;
	   }

	   newMdl = IoAllocateMdl(Data->Iopb->Parameters.QueryEa.EaBuffer, Length, FALSE, FALSE, NULL);

	    if(newMdl == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	    }

	    Data->Iopb->Parameters.QueryEa.MdlAddress = newMdl;

	    MmBuildMdlForNonPagedPool(newMdl);
    }else if(Data->Iopb->MajorFunction == IRP_MJ_SET_EA) {

	    if(Data->Iopb->Parameters.SetEa.MdlAddress != NULL) {

		return STATUS_SUCCESS;
	    }

	    newMdl = IoAllocateMdl(Data->Iopb->Parameters.SetEa.EaBuffer, Length, FALSE, FALSE, NULL);

	    if(newMdl == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	    }

	    Data->Iopb->Parameters.SetEa.MdlAddress = newMdl;

	    MmBuildMdlForNonPagedPool(newMdl);
    }else if((Data->Iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) 
                && (Data->Iopb->MinorFunction == IRP_MN_USER_FS_REQUEST)){

	    if(Data->Iopb->Parameters.FileSystemControl.Neither.OutputMdlAddress != NULL) {

		return STATUS_SUCCESS;
	    }

	    newMdl = IoAllocateMdl(Data->Iopb->Parameters.FileSystemControl.Neither.OutputBuffer, Length, FALSE, FALSE, NULL);

	    if(newMdl == NULL) {

		return STATUS_INSUFFICIENT_RESOURCES;
	    }

	    Data->Iopb->Parameters.FileSystemControl.Neither.OutputMdlAddress = newMdl;

	    MmBuildMdlForNonPagedPool(newMdl);
    }


    __try {

        MmProbeAndLockPages(newMdl, Data->RequestorMode, Operation);
        Status = STATUS_SUCCESS;

	 FltSetCallbackDataDirty( Data );

    } __except (EXCEPTION_EXECUTE_HANDLER) {

        DbgBreak();
        IoFreeMdl(newMdl);

	 if(Data->Iopb->MajorFunction == IRP_MJ_READ) {
	 	
        	Data->Iopb->Parameters.Read.MdlAddress = NULL;
	 }else if(Data->Iopb->MajorFunction == IRP_MJ_WRITE) {

	 	Data->Iopb->Parameters.Write.MdlAddress = NULL;
	 }else if((Data->Iopb->MajorFunction == IRP_MJ_DIRECTORY_CONTROL)
               && (Data->Iopb->MinorFunction == IRP_MN_QUERY_DIRECTORY)) {

		Data->Iopb->Parameters.DirectoryControl.QueryDirectory.MdlAddress = NULL;
	 }else if(Data->Iopb->MajorFunction == IRP_MJ_QUERY_EA) {

	 	Data->Iopb->Parameters.QueryEa.MdlAddress = NULL;
	 }else if(Data->Iopb->MajorFunction == IRP_MJ_SET_EA) {

	 	Data->Iopb->Parameters.SetEa.MdlAddress = NULL;
	 }else if((Data->Iopb->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) 
                && (Data->Iopb->MinorFunction == IRP_MN_USER_FS_REQUEST)){

		Data->Iopb->Parameters.FileSystemControl.Neither.OutputMdlAddress = NULL;
	 }
	 
        Status = STATUS_INVALID_USER_BUFFER;
    }

    return Status;
}

PVOID
SifsGetUserBufferOnRead (
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	)
{
    if (Iopb->Parameters.Read.MdlAddress) {

        return MmGetSystemAddressForMdlSafe(Iopb->Parameters.Read.MdlAddress, NormalPagePriority);
    } else {

        return Iopb->Parameters.Read.ReadBuffer;
    }
}

PVOID
SifsGetUserBufferOnWrite(
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	)
{
    if (Iopb->Parameters.Write.MdlAddress) {

        return MmGetSystemAddressForMdlSafe(Iopb->Parameters.Write.MdlAddress, NormalPagePriority);
    } else {

        return Iopb->Parameters.Write.WriteBuffer;
    }
}

PVOID
SifsGetUserBufferOnSetEa(
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	)
{
    if (Iopb->Parameters.SetEa.MdlAddress) {

        return MmGetSystemAddressForMdlSafe(Iopb->Parameters.SetEa.MdlAddress, NormalPagePriority);
    } else {

        return Iopb->Parameters.SetEa.EaBuffer;
    }
}

PVOID
SifsGetUserBufferOnQueryEa(
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	)
{
    if (Iopb->Parameters.QueryEa.MdlAddress) {

        return MmGetSystemAddressForMdlSafe(Iopb->Parameters.QueryEa.MdlAddress, NormalPagePriority);
    } else {

        return Iopb->Parameters.QueryEa.EaBuffer;
    }
}