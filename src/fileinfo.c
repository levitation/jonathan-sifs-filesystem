#include "fileflt.h"

static NTSTATUS
SifsSetDispositionInfo(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_FCB Fcb,
    __in PSIFS_CCB Ccb,
    __in BOOLEAN bDelete
	);

static NTSTATUS
SifsSetRenameInfo(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_FCB         Fcb,
    __in PSIFS_CCB         Ccb
	);

FLT_PREOP_CALLBACK_STATUS
SifsCommonQueryFileInformation (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    NTSTATUS                Status = STATUS_UNSUCCESSFUL;
    PFILE_OBJECT          FileObject = NULL;
    PVOLUME_CONTEXT  Vcb = NULL;
    PSIFS_FCB               Fcb = NULL;
    PSIFS_MCB              Mcb = NULL;
    PSIFS_CCB              Ccb = NULL;
    FILE_INFORMATION_CLASS  FileInformationClass = IrpContext->Data->Iopb->Parameters.QueryFileInformation.FileInformationClass;
    ULONG                    Length = 0;
    PVOID                     Buffer = NULL;
    BOOLEAN                FcbResourceAcquired = FALSE;

     if(FLT_IS_FASTIO_OPERATION(IrpContext->Data)) {

	 if(FileInformationClass == FileBasicInformation){
	 	
        	retValue = SifsFastIoQueryBasicInfo(IrpContext);

		goto SifsCommonQueryFileInformationCleanup;
	 }else if(FileInformationClass == FileStandardInformation){

	 	retValue = SifsFastIoQueryStandardInfo(IrpContext);

		goto SifsCommonQueryFileInformationCleanup;
	 }
    }

    __try {

        ASSERT(IrpContext != NULL);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));
        
        FileObject = IrpContext->FileObject;
        Fcb = (PSIFS_FCB) FileObject->FsContext;
        if (Fcb == NULL) {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        if (!((Fcb->Identifier.Type == SIFSFCB) &&
                (Fcb->Identifier.Size == sizeof(SIFS_FCB)))) {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        Vcb = IrpContext->VolumeContext;

        {
            if (!ExAcquireResourceSharedLite(
                        &Fcb->MainResource,
                        IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)
                    )) {

                Status = STATUS_PENDING;
                __leave;
            }

            FcbResourceAcquired = TRUE;
        }

        Ccb = (PSIFS_CCB) FileObject->FsContext2;
        ASSERT(Ccb != NULL);
        ASSERT((Ccb->Identifier.Type == SIFSCCB) &&
               (Ccb->Identifier.Size == sizeof(SIFS_CCB)));
		
        Mcb = Fcb->Mcb;

        Length = IrpContext->Data->Iopb->Parameters.QueryFileInformation.Length;
        Buffer = IrpContext->Data->Iopb->Parameters.QueryFileInformation.InfoBuffer;
        RtlZeroMemory(Buffer, Length);

        switch (FileInformationClass) {

        case FileBasicInformation:
        {
            PFILE_BASIC_INFORMATION fileBasicInformation;
	     ULONG LengthReturned = 0;

            if (Length < sizeof(FILE_BASIC_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileBasicInformation = (PFILE_BASIC_INFORMATION) Buffer;

	     Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileBasicInformation, Length, FileBasicInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;	    
        }
        break;

        case FileStandardInformation:
        {
            PFILE_STANDARD_INFORMATION fileStandardInformation;
	     ULONG LengthReturned = 0;

            if (Length < sizeof(FILE_STANDARD_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileStandardInformation = (PFILE_STANDARD_INFORMATION) Buffer;
			
	     Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileStandardInformation, Length, FileStandardInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;

        case FileInternalInformation:
        {
            PFILE_INTERNAL_INFORMATION fileInternalInformation;
	     ULONG LengthReturned = 0;

            if (Length < sizeof(FILE_INTERNAL_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileInternalInformation = (PFILE_INTERNAL_INFORMATION) Buffer;

            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileInternalInformation, Length, FileInternalInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;


        case FileEaInformation:
        {
            PFILE_EA_INFORMATION fileEaInformation;
	     ULONG LengthReturned = 0;

            if (Length < sizeof(FILE_EA_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileEaInformation = (PFILE_EA_INFORMATION) Buffer;

            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileEaInformation, Length, FileEaInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;

        case FileNameInformation:
        {
            PFILE_NAME_INFORMATION fileNameInformation;
            ULONG   LengthReturned = 0;            

	     if (Length < sizeof(FILE_NAME_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }
		 
            fileNameInformation = (PFILE_NAME_INFORMATION) Buffer;
			
            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileNameInformation, Length, FileNameInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;	     
        }
        break;

        case FilePositionInformation:
        {
            PFILE_POSITION_INFORMATION filePositionInformation;
	     ULONG   LengthReturned = 0;

            if (Length < sizeof(FILE_POSITION_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            filePositionInformation = (PFILE_POSITION_INFORMATION) Buffer;

	     Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,filePositionInformation, Length, FilePositionInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;

        case FileAllInformation:
        {
            PFILE_ALL_INFORMATION       fileAllInformation;
	     ULONG   LengthReturned = 0;

            if (Length < sizeof(FILE_ALL_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileAllInformation = (PFILE_ALL_INFORMATION) Buffer;

            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileAllInformation, Length, FileAllInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;

	  /*
        case FileAlternateNameInformation:
        {
            
        }
	 break;       
       */
        case FileNetworkOpenInformation:
        {
            PFILE_NETWORK_OPEN_INFORMATION fileNetworkOpenInformation;
	     ULONG   LengthReturned = 0;

            if (Length < sizeof(FILE_NETWORK_OPEN_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileNetworkOpenInformation = (PFILE_NETWORK_OPEN_INFORMATION) Buffer;

            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileNetworkOpenInformation, Length, FileNetworkOpenInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;

        case FileAttributeTagInformation:
        {
            PFILE_ATTRIBUTE_TAG_INFORMATION fileAttributeTagInformation;
	     ULONG   LengthReturned = 0;

            if (Length < sizeof(FILE_ATTRIBUTE_TAG_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

            fileAttributeTagInformation = (PFILE_ATTRIBUTE_TAG_INFORMATION) Buffer;
			
            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileAttributeTagInformation, Length, FileAttributeTagInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;


        case FileStreamInformation:
        {
	     PFILE_STREAM_INFORMATION fileStreamInformation;
	     ULONG   LengthReturned = 0;

            if (Length < sizeof(FILE_STREAM_INFORMATION)) {
                Status = STATUS_BUFFER_OVERFLOW;
                __leave;
            }

	     fileStreamInformation = (PFILE_STREAM_INFORMATION) Buffer;
		 
            Status = FltQueryInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject
		 		,fileStreamInformation, Length, FileStreamInformation, &LengthReturned);

	     IrpContext->Data->IoStatus.Information = LengthReturned;
        }
        break;

        case FileCompressionInformation:
        case FileMoveClusterInformation:
        default:
            Status = STATUS_INVALID_PARAMETER; /* STATUS_INVALID_INFO_CLASS; */
            break;
        }

    } __finally {

        if (FcbResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        if (!IrpContext->ExceptionInProgress) {
            if (Status == STATUS_PENDING ||
                    Status == STATUS_CANT_WAIT) {
                Status = SifsQueueRequest(IrpContext);
            } else {

		  IrpContext->Data->IoStatus.Status = Status;
		  
                SifsCompleteIrpContext(IrpContext,  Status);
            }
        }else{

		IrpContext->Data->IoStatus.Status = Status;
        }

	if(Status == STATUS_PENDING) {

		retValue = FLT_PREOP_PENDING;
	}
    }

SifsCommonQueryFileInformationCleanup:
	
    return retValue;
}


FLT_PREOP_CALLBACK_STATUS
SifsCommonSetFileInformation (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    NTSTATUS                Status = STATUS_UNSUCCESSFUL;
    PFLT_IO_PARAMETER_BLOCK Iopb = IrpContext->Data->Iopb;
    PFILE_OBJECT            FileObject;
    PSIFS_FCB               Fcb;
    PSIFS_CCB               Ccb;
    PSIFS_MCB               Mcb;
    FILE_INFORMATION_CLASS  FileInformationClass;

    ULONG                   NotifyFilter = 0;

    ULONG                   Length;
    PVOID                   Buffer;

    BOOLEAN                 VcbMainResourceAcquired = FALSE;
    BOOLEAN                 FcbMainResourceAcquired = FALSE;
    BOOLEAN                 FcbPagingIoResourceAcquired = FALSE;
    BOOLEAN                 CacheInitialized = FALSE;

    __try {

        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileInformationClass = Iopb->Parameters.SetFileInformation.FileInformationClass;
        Length = Iopb->Parameters.SetFileInformation.Length;
        Buffer = Iopb->Parameters.SetFileInformation.InfoBuffer;

        /* we need grab Vcb in case it's a rename operation */
        if (FileInformationClass == FileRenameInformation) {
            if (!ExAcquireResourceExclusiveLite(
                        &IrpContext->VolumeContext->MainResource,
                        IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) )) {
                Status = STATUS_PENDING;
                __leave;
            }
            VcbMainResourceAcquired = TRUE;
        }

        FileObject = IrpContext->FileObject;
        Fcb = (PSIFS_FCB) FileObject->FsContext;

        // This request is issued to volumes, just return success
        if (Fcb == NULL || Fcb->Identifier.Type == SIFSVCB) {
            Status = STATUS_SUCCESS;
            __leave;
        }
        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        if (IsFlagOn(Fcb->Mcb->Flags, MCB_FILE_DELETED)) {
            Status = STATUS_FILE_DELETED;
            __leave;
        }

        Ccb = (PSIFS_CCB) FileObject->FsContext2;
        ASSERT(Ccb != NULL);
        ASSERT((Ccb->Identifier.Type == SIFSCCB) &&
               (Ccb->Identifier.Size == sizeof(SIFS_CCB)));
        Mcb = Fcb->Mcb;        

        if ( !FlagOn(Fcb->Flags, FCB_PAGE_FILE) &&
                ((FileInformationClass == FileEndOfFileInformation) ||
                 (FileInformationClass == FileAllocationInformation))) {

            Status =  FltCheckOplock( &Fcb->Oplock,
                                       IrpContext->Data,
                                       IrpContext,
                                       NULL,
                                       NULL );

            if (Status != STATUS_SUCCESS) {
                __leave;
            }

            //
            //  Set the flag indicating if Fast I/O is possible
            //

            Fcb->Header.IsFastIoPossible = SifsIsFastIoPossible(Fcb);
        }

        /* for renaming, we must not get any Fcb locks here, function
           SifsSetRenameInfo will get Dcb resource exclusively.  */
        if (!IsFlagOn(Fcb->Flags, FCB_PAGE_FILE) &&
                FileInformationClass != FileRenameInformation) {

            if (!ExAcquireResourceExclusiveLite(
                        &Fcb->MainResource,
                        IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) )) {
                Status = STATUS_PENDING;
                __leave;
            }

            FcbMainResourceAcquired = TRUE;
        }

        if (IsFlagOn(IrpContext->VolumeContext->Flags, VCB_READ_ONLY)) {

            if (FileInformationClass != FilePositionInformation) {
                Status = STATUS_MEDIA_WRITE_PROTECTED;
                __leave;
            }
        }

        if ( FileInformationClass == FileAllocationInformation ||
                FileInformationClass == FileEndOfFileInformation) {

            if (!ExAcquireResourceExclusiveLite(
                        &Fcb->PagingIoResource,
                        IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) )) {
                Status = STATUS_PENDING;
                DbgBreak();
                __leave;
            }

            FcbPagingIoResourceAcquired = TRUE;
        }

        /*
                if (FileInformationClass != FileDispositionInformation
                    && IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING))
                {
                    Status = STATUS_DELETE_PENDING;
                    __leave;
                }
        */
        switch (FileInformationClass) {

        case FileBasicInformation:
        {
            PFILE_BASIC_INFORMATION fileBasicInformation = (PFILE_BASIC_INFORMATION) Buffer;

	     Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileBasicInformation, Length, FileBasicInformation);

	     if(!NT_SUCCESS(Status)){

			__leave;
	     }
		 
            if ((fileBasicInformation->CreationTime.QuadPart != 0) && (fileBasicInformation->CreationTime.QuadPart != -1)) {
                Mcb->Lower.CreationTime = fileBasicInformation->CreationTime;
                NotifyFilter |= FILE_NOTIFY_CHANGE_CREATION;
            }

            if ((fileBasicInformation->LastAccessTime.QuadPart != 0) && (fileBasicInformation->LastAccessTime.QuadPart != -1)) {
                Mcb->Lower.LastAccessTime = fileBasicInformation->LastAccessTime;
                NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_ACCESS;
            }

            if ((fileBasicInformation->LastWriteTime.QuadPart != 0) && (fileBasicInformation->LastWriteTime.QuadPart != -1)) {
                Mcb->Lower.LastWriteTime = fileBasicInformation->LastWriteTime;
                NotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;
                SetFlag(Ccb->Flags, CCB_LAST_WRITE_UPDATED);
            }

            if ((fileBasicInformation->ChangeTime.QuadPart !=0) && (fileBasicInformation->ChangeTime.QuadPart != -1)) {
                Mcb->Lower.ChangeTime = fileBasicInformation->ChangeTime;
            }

            if (fileBasicInformation->FileAttributes != 0) {

                NotifyFilter |= FILE_NOTIFY_CHANGE_ATTRIBUTES;

                if (fileBasicInformation->FileAttributes & FILE_ATTRIBUTE_TEMPORARY) {
                    SetFlag(FileObject->Flags, FO_TEMPORARY_FILE);
                } else {
                    ClearFlag(FileObject->Flags, FO_TEMPORARY_FILE);
                }

                Mcb->FileAttr = fileBasicInformation->FileAttributes;
            }

            ClearFlag(NotifyFilter, FILE_NOTIFY_CHANGE_LAST_ACCESS);
            Status = STATUS_SUCCESS;
        }

        break;

        case FileAllocationInformation:
        {
            PFILE_ALLOCATION_INFORMATION fileAllocationInformation = (PFILE_ALLOCATION_INFORMATION)Buffer;
            LARGE_INTEGER  AllocationSize;

            if (IsMcbDirectory(Mcb) || IsMcbSpecialFile(Mcb)) {
                Status = STATUS_INVALID_DEVICE_REQUEST;
                __leave;
            } else {
                Status = STATUS_SUCCESS;
            }

	     Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileAllocationInformation, Length, FileAllocationInformation);

	     if(!NT_SUCCESS(Status)) {

			__leave;
	     }
		 
            Mcb = Fcb->Mcb;

            /* initialize cache map if needed */
            if ((FileObject->SectionObjectPointer->DataSectionObject != NULL) &&
                    (FileObject->SectionObjectPointer->SharedCacheMap == NULL) &&
                    !IsFlagOn(IrpContext->Data->Iopb->IrpFlags, IRP_PAGING_IO)) {

                ASSERT(!IsFlagOn( FileObject->Flags, FO_CLEANUP_COMPLETE));

                CcInitializeCacheMap(
                    FileObject,
                    (PCC_FILE_SIZES)&(Fcb->Header.AllocationSize),
                    FALSE,
                    &(g_FileFltContext.CacheManagerNoOpCallbacks),
                    Fcb );

                CacheInitialized = TRUE;
            }

            /* get user specified allocationsize aligned with BLOCK_SIZE */
            AllocationSize.QuadPart = CEILING_ALIGNED(ULONGLONG,
                                      (ULONGLONG)fileAllocationInformation->AllocationSize.QuadPart,
                                      (ULONGLONG)IrpContext->VolumeContext->SectorSize);

            if (AllocationSize.QuadPart > Fcb->Header.AllocationSize.QuadPart) {

                Status = SifsExpandAndTruncateFile(IrpContext, Mcb, &AllocationSize);
                Fcb->Header.AllocationSize = AllocationSize;
                NotifyFilter = FILE_NOTIFY_CHANGE_SIZE;

            } else if (AllocationSize.QuadPart < Fcb->Header.AllocationSize.QuadPart) {

                if (MmCanFileBeTruncated(&(Fcb->SectionObject), &AllocationSize))  {

                    /* truncate file blocks */
                    Status = SifsExpandAndTruncateFile(IrpContext, Mcb, &AllocationSize);

                    if (NT_SUCCESS(Status)) {
                        ClearLongFlag(Fcb->Flags, FCB_ALLOC_IN_CREATE);
                    }

                    NotifyFilter = FILE_NOTIFY_CHANGE_SIZE;
                    Fcb->Header.AllocationSize.QuadPart = AllocationSize.QuadPart;
                    if (Mcb->AllocationSize.QuadPart > AllocationSize.QuadPart) {
                        Mcb->AllocationSize = AllocationSize;
                    }
                    Fcb->Header.FileSize = Mcb->FileSize;
                    if (Fcb->Header.ValidDataLength.QuadPart > Fcb->Header.FileSize.QuadPart) {
                        Fcb->Header.ValidDataLength.QuadPart = Fcb->Header.FileSize.QuadPart;
                    }

                } else {

                    Status = STATUS_USER_MAPPED_FILE;
                    DbgBreak();
                    __leave;
                }
            }

            if (NotifyFilter) {

                SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
                SetLongFlag(Fcb->Flags, FCB_FILE_MODIFIED);               
				
                if (CcIsFileCached(FileObject)) {
                    CcSetFileSizes(FileObject, (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
                }
            }
        }

        break;

        case FileEndOfFileInformation:
        {
            PFILE_END_OF_FILE_INFORMATION fileEndOfFileInformation = (PFILE_END_OF_FILE_INFORMATION) Buffer;
            LARGE_INTEGER NewSize, OldSize, EndOfFile;

            if (IsMcbDirectory(Mcb) || IsMcbSpecialFile(Mcb)) {
                Status = STATUS_INVALID_DEVICE_REQUEST;
                __leave;
            } else {
                Status = STATUS_SUCCESS;
            }

	    Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileEndOfFileInformation, Length, FileEndOfFileInformation);

	    if(!NT_SUCCESS(Status)){
			
			__leave;
	    }

            /* set Mcb to it's target */
            Mcb = Fcb->Mcb;

            /* initialize cache map if needed */
            if ((FileObject->SectionObjectPointer->DataSectionObject != NULL) &&
                    (FileObject->SectionObjectPointer->SharedCacheMap == NULL) &&
                    !IsFlagOn(IrpContext->Data->Iopb->IrpFlags, IRP_PAGING_IO)) {

                ASSERT(!IsFlagOn( FileObject->Flags, FO_CLEANUP_COMPLETE));

                CcInitializeCacheMap(
                    FileObject,
                    (PCC_FILE_SIZES)&(Fcb->Header.AllocationSize),
                    FALSE,
                    &(g_FileFltContext.CacheManagerNoOpCallbacks),
                    Fcb );

                CacheInitialized = TRUE;
            }

            OldSize = Fcb->Header.AllocationSize;
            EndOfFile = fileEndOfFileInformation->EndOfFile;

            if (Iopb->Parameters.SetFileInformation.AdvanceOnly) {

                if (IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING)) {
                    __leave;
                }

                if (EndOfFile.QuadPart > Fcb->Header.FileSize.QuadPart) {
                    EndOfFile.QuadPart = Fcb->Header.FileSize.QuadPart;
                }

                if (EndOfFile.QuadPart > Fcb->Header.ValidDataLength.QuadPart) {

                    Fcb->Header.ValidDataLength.QuadPart = EndOfFile.QuadPart;
                    NotifyFilter = FILE_NOTIFY_CHANGE_SIZE;
                }

                __leave;
            }

            NewSize.QuadPart = CEILING_ALIGNED(ULONGLONG,
                                               EndOfFile.QuadPart, IrpContext->VolumeContext->SectorSize);

            if (NewSize.QuadPart > OldSize.QuadPart) {

                Fcb->Header.AllocationSize = NewSize;
                Status = SifsExpandAndTruncateFile(
                             IrpContext,
                             Mcb,
                             &(Fcb->Header.AllocationSize)
                         );
                NotifyFilter = FILE_NOTIFY_CHANGE_SIZE;

            } else if (NewSize.QuadPart == OldSize.QuadPart) {

                /* we are luck ;) */
                Status = STATUS_SUCCESS;

            } else {

                /* don't truncate file data since it's still being written */
                if (IsFlagOn(Fcb->Flags, FCB_ALLOC_IN_WRITE)) {

                    Status = STATUS_SUCCESS;

                } else {

                    if (!MmCanFileBeTruncated(&(Fcb->SectionObject), &NewSize)) {
                        Status = STATUS_USER_MAPPED_FILE;
                        DbgBreak();
                        __leave;
                    }

                    /* truncate file blocks */
                    Status = SifsExpandAndTruncateFile(IrpContext, Mcb, &NewSize);

                    /* restore original file size */
                    if (NT_SUCCESS(Status)) {
                        ClearLongFlag(Fcb->Flags, FCB_ALLOC_IN_CREATE);
                    }

                    /* update file allocateion size */
                    Fcb->Header.AllocationSize.QuadPart = NewSize.QuadPart;

                    ASSERT(NewSize.QuadPart >= Mcb->FileSize.QuadPart);
                    if (Fcb->Header.FileSize.QuadPart < Mcb->FileSize.QuadPart) {
                        Fcb->Header.FileSize.QuadPart = Mcb->FileSize.QuadPart;
                    }
                    if (Fcb->Header.ValidDataLength.QuadPart > Fcb->Header.FileSize.QuadPart) {
                        Fcb->Header.ValidDataLength.QuadPart = Fcb->Header.FileSize.QuadPart;
                    }

                    SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
                    SetLongFlag(Fcb->Flags, FCB_FILE_MODIFIED);
                }

                NotifyFilter = FILE_NOTIFY_CHANGE_SIZE;
            }

            if (NT_SUCCESS(Status)) {

                Fcb->Header.ValidDataLength.QuadPart = Fcb->Header.FileSize.QuadPart = Mcb->Lower.FileSize.QuadPart = EndOfFile.QuadPart;
                if (Fcb->Header.ValidDataLength.QuadPart > Fcb->Header.FileSize.QuadPart)
                    Fcb->Header.ValidDataLength.QuadPart = Fcb->Header.FileSize.QuadPart;
                
                SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
                SetLongFlag(Fcb->Flags, FCB_FILE_MODIFIED);
                NotifyFilter = FILE_NOTIFY_CHANGE_SIZE;
            }           

            if (CcIsFileCached(FileObject)) {
                CcSetFileSizes(FileObject, (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
            }
        }

        break;

        case FileDispositionInformation:
        {
            PFILE_DISPOSITION_INFORMATION fileDispositionInformation = (PFILE_DISPOSITION_INFORMATION)Buffer;

	     Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileDispositionInformation, Length, FileDispositionInformation);

	     if(NT_SUCCESS(Status)) {
		 	
	         Status = SifsSetDispositionInfo(IrpContext, Fcb, Ccb, fileDispositionInformation->DeleteFile);
			 
                Status = STATUS_SUCCESS;
	     }		 
        }
        break;

        case FileRenameInformation:
        {
	     PFILE_RENAME_INFORMATION fileRenameInformation = (PFILE_RENAME_INFORMATION)Buffer;

	     Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileRenameInformation, Length, FileRenameInformation);

	     if(NT_SUCCESS(Status)) {
		 	
            		Status = SifsSetRenameInfo(IrpContext, Fcb, Ccb);
	     }
        }
        break;

        //
        // This is the only set file information request supported on read
        // only file systems
        //
        case FilePositionInformation:
        {
            PFILE_POSITION_INFORMATION filePositionInformation = (PFILE_POSITION_INFORMATION) Buffer;

            Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, filePositionInformation, Length, FilePositionInformation);

	     if(NT_SUCCESS(Status)) {
		 	
	         FileObject->CurrentByteOffset =
	                filePositionInformation->CurrentByteOffset;

                Status = STATUS_SUCCESS;
	     }
        }
        break;

        case FileLinkInformation:
	 {
   	     PFILE_LINK_INFORMATION fileLinkInformation = (PFILE_LINK_INFORMATION)Buffer;
		 
            Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileLinkInformation, Length, FileLinkInformation);

        }
        break;

	case FileValidDataLengthInformation:
	{
	    PFILE_VALID_DATA_LENGTH_INFORMATION fileValidDataLengthInformation = (PFILE_VALID_DATA_LENGTH_INFORMATION)Buffer;
		 
           Status = FltSetInformationFile(IrpContext->FltObjects->Instance, Mcb->Lower.FileObject, fileValidDataLengthInformation, Length, FileValidDataLengthInformation);
	}
       break;
       default:
            Status = STATUS_INVALID_PARAMETER;/* STATUS_INVALID_INFO_CLASS; */
       }

    } __finally {

        if (FcbPagingIoResourceAcquired) {
            ExReleaseResourceLite(&Fcb->PagingIoResource);
        }

        if (NT_SUCCESS(Status) && (NotifyFilter != 0)) {
            SifsNotifyReportChange(
                IrpContext,
                IrpContext->VolumeContext,
                Mcb,
                NotifyFilter,
                FILE_ACTION_MODIFIED );

        }

        if (FcbMainResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        if (CacheInitialized) {
            CcUninitializeCacheMap(FileObject, NULL, NULL);
        }

        if (VcbMainResourceAcquired) {
            ExReleaseResourceLite(&IrpContext->VolumeContext->MainResource);
        }

        if (!IrpContext->ExceptionInProgress) {
            if (Status == STATUS_PENDING ||
                    Status == STATUS_CANT_WAIT ) {
                DbgBreak();
                Status = SifsQueueRequest(IrpContext);
            } else {

		  IrpContext->Data->IoStatus.Status = Status;
                SifsCompleteIrpContext(IrpContext,  Status);
            }
        }else{
        
            IrpContext->Data->IoStatus.Status = Status;
        }

	 if(Status == STATUS_PENDING) {

		retValue = FLT_PREOP_PENDING;
	 }
    }

    return retValue;
}


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

static NTSTATUS
SifsSetDispositionInfo(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_FCB Fcb,
    __in PSIFS_CCB Ccb,
    __in BOOLEAN bDelete
	)
{
    NTSTATUS status = STATUS_SUCCESS;
    PSIFS_MCB  Mcb = Fcb->Mcb;

    if (bDelete) {

        /* always allow deleting on symlinks */
        status = SifsIsFileRemovable(Fcb);

        if (NT_SUCCESS(status)) {
            SetLongFlag(Fcb->Flags, FCB_DELETE_PENDING);
            IrpContext->FltObjects->FileObject->DeletePending = TRUE;
        }

    } else {

        ClearLongFlag(Fcb->Flags, FCB_DELETE_PENDING);
        IrpContext->FltObjects->FileObject->DeletePending = FALSE;
    }

    return status;
}

static NTSTATUS
SifsSetRenameInfo(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_FCB         Fcb,
    __in PSIFS_CCB         Ccb
	)
{
    PSIFS_MCB               Mcb = Fcb->Mcb;

    NTSTATUS                Status = STATUS_SUCCESS;
    PFILE_RENAME_INFORMATION    fileRenameInformation = NULL;    

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

