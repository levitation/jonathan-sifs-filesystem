#include "fileflt.h"

#ifdef ALLOC_PRAGMA

#pragma alloc_text(PAGE, SifsFastIoRead)
#pragma alloc_text(PAGE, SifsFastIoWrite)
#pragma alloc_text(PAGE, SifsFastIoCheckIfPossible)
#pragma alloc_text(PAGE, SifsFastIoQueryBasicInfo)
#pragma alloc_text(PAGE, SifsFastIoQueryStandardInfo)
#pragma alloc_text(PAGE, SifsFastIoQueryNetworkOpenInformation)

#endif

FAST_IO_POSSIBLE
SifsIsFastIoPossible(
    __in PSIFS_FCB Fcb
	)
{
    FAST_IO_POSSIBLE IsPossible = FastIoIsNotPossible;

    if (!Fcb || !FltOplockIsFastIoPossible(&Fcb->Oplock))
        return IsPossible;

    IsPossible = FastIoIsQuestionable;

    if (!FsRtlAreThereCurrentFileLocks(&Fcb->FileLockAnchor)) {
        if (/* !IsFlagOn(VolumeContext->Flags, VCB_READ_ONLY) */ 1 ) {
            IsPossible = FastIoIsPossible;
        }
    }

    return IsPossible;
}

FLT_PREOP_CALLBACK_STATUS
SifsFastIoCheckIfPossible (
    __in PSIFS_IRP_CONTEXT IrpContext
)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    BOOLEAN          bPossible = FastIoIsNotPossible;
    PFLT_IO_PARAMETER_BLOCK Iopb = IrpContext->Data->Iopb;
    PSIFS_FCB        Fcb;
    PSIFS_CCB        Ccb;
    LARGE_INTEGER    lLength;

    lLength.QuadPart = Iopb->Parameters.FastIoCheckIfPossible.Length;

    __try {

        FsRtlEnterFileSystem();

        __try {

            Fcb = (PSIFS_FCB) IrpContext->FileObject->FsContext;
            if (Fcb == NULL || Fcb->Identifier.Type == SIFSVCB) {
                __leave;
            }

            ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
                   (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

            Ccb = (PSIFS_CCB) IrpContext->FileObject->FsContext2;
            if (Ccb == NULL) {
                __leave;
            }

            if (Iopb->Parameters.FastIoCheckIfPossible.CheckForReadOperation) {

                bPossible = FsRtlFastCheckLockForRead(
                                &Fcb->FileLockAnchor,
                                &Iopb->Parameters.FastIoCheckIfPossible.FileOffset,
                                &lLength,
                                Iopb->Parameters.FastIoCheckIfPossible.LockKey,
                                IrpContext->FileObject,
                                PsGetCurrentProcess());

            } else {

                if (!(IsFlagOn(IrpContext->VolumeContext->Flags, VCB_READ_ONLY) ||
                        IsFlagOn(IrpContext->VolumeContext->Flags, VCB_WRITE_PROTECTED))) {
                    bPossible = FsRtlFastCheckLockForWrite(
                                    &Fcb->FileLockAnchor,
                                    &Iopb->Parameters.FastIoCheckIfPossible.FileOffset,
                                    &lLength,
                                    Iopb->Parameters.FastIoCheckIfPossible.LockKey,
                                    IrpContext->FileObject,
                                    PsGetCurrentProcess());
                }
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            bPossible = FastIoIsNotPossible;
        }

    } __finally {

        FsRtlExitFileSystem();

	 if(bPossible == TRUE) {

	     IrpContext->Data->IoStatus.Status = STATUS_SUCCESS;
	 }else{

	     IrpContext->Data->IoStatus.Status = STATUS_UNSUCCESSFUL;
	 }
    }

    return retValue;
}


FLT_PREOP_CALLBACK_STATUS
SifsFastIoRead (
	__in PSIFS_IRP_CONTEXT IrpContext
       )
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    PSIFS_FCB    Fcb;
    NTSTATUS      Status = STATUS_UNSUCCESSFUL;
    PDEVICE_OBJECT DeviceObject = NULL;

    __try{
		
	    Fcb = (PSIFS_FCB) IrpContext->FltObjects->FileObject->FsContext;
	    if (Fcb == NULL) {

		   __leave;
	    }

	    Status = FltGetDeviceObject(IrpContext->FltObjects->Volume,&DeviceObject);

	    if(!NT_SUCCESS(Status)) {

		  __leave;
	    }

	    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
	           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

	    FsRtlCopyRead (IrpContext->FltObjects->FileObject, &IrpContext->Data->Iopb->Parameters.Read.ByteOffset, IrpContext->Data->Iopb->Parameters.Read.Length, TRUE,
	                 IrpContext->Data->Iopb->Parameters.Read.Key, SifsGetUserBufferOnRead(IrpContext->Data->Iopb), &(IrpContext->Data->IoStatus), DeviceObject);
		
    	}__finally{

		if(Status != STATUS_SUCCESS) {

			IrpContext->Data->IoStatus.Status = Status;
		}
    	}

    return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsFastIoWrite (
    __in PSIFS_IRP_CONTEXT IrpContext
    )
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    PSIFS_FCB   Fcb = NULL;
    BOOLEAN     Locked = FALSE;
    PLARGE_INTEGER FileOffset = &(IrpContext->Data->Iopb->Parameters.Write.ByteOffset);
    ULONG Length = IrpContext->Data->Iopb->Parameters.Write.Length;
    NTSTATUS      Status = STATUS_UNSUCCESSFUL;
    PDEVICE_OBJECT DeviceObject = NULL;

    
    __try {

	Fcb = (PSIFS_FCB) IrpContext->FileObject->FsContext;
	
    	if (Fcb == NULL) {
			
            __leave;
    	}


	 Status = FltGetDeviceObject(IrpContext->FltObjects->Volume,&DeviceObject);

	 if(!NT_SUCCESS(Status)) {

		  __leave;
	  }
	
        FsRtlEnterFileSystem();

        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        if (IsFlagOn(IrpContext->VolumeContext->Flags, VCB_READ_ONLY)) {

	    Status = STATUS_UNSUCCESSFUL;
		
            __leave;
        }

        ExAcquireResourceSharedLite(&Fcb->MainResource, TRUE);
        Locked = TRUE;

        if (IsEndOfFile(*FileOffset) || ((LONGLONG)(Fcb->Mcb->FileSize.QuadPart) <
                                         (FileOffset->QuadPart + Length)) ) {
        } else {
            ExReleaseResourceLite(&Fcb->MainResource);
            Locked = FALSE;
            FsRtlCopyWrite(IrpContext->FileObject, FileOffset, Length, TRUE,
                                    IrpContext->Data->Iopb->Parameters.Write.Key, SifsGetUserBufferOnWrite(IrpContext->Data->Iopb)
                                    , &(IrpContext->Data->IoStatus), DeviceObject);
        }

    } __finally {

        if (Locked) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

	if(Status != STATUS_SUCCESS) {

		IrpContext->Data->IoStatus.Status = Status;
	}

        FsRtlExitFileSystem();
    }

    return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsFastIoQueryBasicInfo (
    __in PSIFS_IRP_CONTEXT IrpContext
    )
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    PSIFS_FCB   Fcb = NULL;
    BOOLEAN     FcbMainResourceAcquired = FALSE;
    PFILE_BASIC_INFORMATION fileBasicInformation = IrpContext->Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;

    __try {

        if(IrpContext->Data->Iopb->Parameters.QueryFileInformation.Length < sizeof(FILE_BASIC_INFORMATION)) {
		
		IrpContext->Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
		__leave;
	 }
		
        FsRtlEnterFileSystem();

        __try {

            Fcb = (PSIFS_FCB) IrpContext->FileObject->FsContext;
            if (Fcb == NULL || Fcb->Identifier.Type == SIFSVCB) {
                IrpContext->Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
                __leave;
            }

            ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
                   (Fcb->Identifier.Size == sizeof(SIFS_FCB)));
			
            if (!IsFlagOn(Fcb->Flags, FCB_PAGE_FILE)) {
                if (!ExAcquireResourceSharedLite(
                            &Fcb->MainResource,
                            TRUE)) {
                    __leave;
                }
                FcbMainResourceAcquired = TRUE;
            }

            RtlZeroMemory(fileBasicInformation, sizeof(FILE_BASIC_INFORMATION));

            /*
            typedef struct _FILE_BASIC_INFORMATION {
            LARGE_INTEGER   CreationTime;
            LARGE_INTEGER   LastAccessTime;
            LARGE_INTEGER   LastWriteTime;
            LARGE_INTEGER   ChangeTime;
            ULONG           FileAttributes;
            } FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;
            */

            fileBasicInformation->CreationTime = Fcb->Mcb->CreationTime;
            fileBasicInformation->LastAccessTime = Fcb->Mcb->LastAccessTime;
            fileBasicInformation->LastWriteTime = Fcb->Mcb->LastWriteTime;
            fileBasicInformation->ChangeTime = Fcb->Mcb->ChangeTime;

            fileBasicInformation->FileAttributes = Fcb->Mcb->FileAttr;
            if (fileBasicInformation->FileAttributes == 0) {
                fileBasicInformation->FileAttributes = FILE_ATTRIBUTE_NORMAL;
            }

            IrpContext->Data->IoStatus.Information = sizeof(FILE_BASIC_INFORMATION);
            IrpContext->Data->IoStatus.Status = STATUS_SUCCESS;

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            IrpContext->Data->IoStatus.Status = GetExceptionCode();
        }

    } __finally {

        if (FcbMainResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        FsRtlExitFileSystem();
    }

    return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsFastIoQueryStandardInfo (
    __in PSIFS_IRP_CONTEXT IrpContext
)
{

    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    PSIFS_FCB   Fcb = NULL;
    BOOLEAN     FcbMainResourceAcquired = FALSE;
    PFILE_STANDARD_INFORMATION fileStandardInformation = IrpContext->Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;

    __try {

	if(IrpContext->Data->Iopb->Parameters.QueryFileInformation.Length < sizeof(FILE_STANDARD_INFORMATION)) {
		
		IrpContext->Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
		__leave;
	}
		
        FsRtlEnterFileSystem();

        __try {

            Fcb = (PSIFS_FCB) IrpContext->FileObject->FsContext;
            if (Fcb == NULL || Fcb->Identifier.Type == SIFSVCB)  {
                IrpContext->Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
                __leave;
            }

            ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
                   (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

            if (!IsFlagOn(Fcb->Flags, FCB_PAGE_FILE)) {
                if (!ExAcquireResourceSharedLite(
                            &Fcb->MainResource,
                            TRUE        )) {
                    __leave;
                }
                FcbMainResourceAcquired = TRUE;
            }

            RtlZeroMemory(fileStandardInformation, sizeof(FILE_STANDARD_INFORMATION));

            /*
            typedef struct _FILE_STANDARD_INFORMATION {
            LARGE_INTEGER   AllocationSize;
            LARGE_INTEGER   EndOfFile;
            ULONG           NumberOfLinks;
            BOOLEAN         DeletePending;
            BOOLEAN         Directory;
            } FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;
            */

            fileStandardInformation->NumberOfLinks = Fcb->Mcb->NumberOfLinks;
            fileStandardInformation->DeletePending = IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING);

            fileStandardInformation->Directory = FALSE;
            fileStandardInformation->AllocationSize = Fcb->Header.AllocationSize;
            fileStandardInformation->EndOfFile = Fcb->Header.FileSize;

            IrpContext->Data->IoStatus.Information = sizeof(FILE_STANDARD_INFORMATION);
            IrpContext->Data->IoStatus.Status = STATUS_SUCCESS;

        } __except (EXCEPTION_EXECUTE_HANDLER) {
            IrpContext->Data->IoStatus.Status = GetExceptionCode();
        }

    } __finally {

        if (FcbMainResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        FsRtlExitFileSystem();
    }

    return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsFastIoQueryNetworkOpenInformation (
    __in PSIFS_IRP_CONTEXT IrpContext
)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    PSIFS_FCB   Fcb = NULL;
    BOOLEAN FcbResourceAcquired = FALSE;
    PFILE_NETWORK_OPEN_INFORMATION fileNetworkOpenInformation = IrpContext->Data->Iopb->Parameters.NetworkQueryOpen.NetworkInformation;

    __try {

        FsRtlEnterFileSystem();

        Fcb = (PSIFS_FCB) IrpContext->FltObjects->FileObject->FsContext;
        if (Fcb == NULL || Fcb->Identifier.Type == SIFSVCB) {
            DbgBreak();
            IrpContext->Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        if (IrpContext->FltObjects->FileObject->FsContext2) {

	    IrpContext->Data->IoStatus.Status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        if (!IsFlagOn(Fcb->Flags, FCB_PAGE_FILE)) {

            if (!ExAcquireResourceSharedLite(
                        &Fcb->MainResource,
                        TRUE
                    )) {
                __leave;
            }

            FcbResourceAcquired = TRUE;
        }

        fileNetworkOpenInformation->AllocationSize = Fcb->Header.AllocationSize;
        fileNetworkOpenInformation->EndOfFile      = Fcb->Header.FileSize;

        fileNetworkOpenInformation->FileAttributes = Fcb->Mcb->FileAttr;
        if (fileNetworkOpenInformation->FileAttributes == 0) {
            fileNetworkOpenInformation->FileAttributes = FILE_ATTRIBUTE_NORMAL;
        }

       
        fileNetworkOpenInformation->CreationTime   = Fcb->Mcb->CreationTime;
        fileNetworkOpenInformation->LastAccessTime = Fcb->Mcb->LastAccessTime;
        fileNetworkOpenInformation->LastWriteTime  = Fcb->Mcb->LastWriteTime;
        fileNetworkOpenInformation->ChangeTime     = Fcb->Mcb->ChangeTime;

        IrpContext->Data->IoStatus.Status = STATUS_SUCCESS;
        IrpContext->Data->IoStatus.Information = sizeof(FILE_NETWORK_OPEN_INFORMATION);

    } __finally {

        if (FcbResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        FsRtlExitFileSystem();
    }

    return retValue;
}

