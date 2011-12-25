#include "fileflt.h"

static NTSTATUS
SifsSupersedeOrOverWriteFile(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PFILE_OBJECT      FileObject,
    __in PVOLUME_CONTEXT         Vcb,
    __in PSIFS_FCB         Fcb,
    __in PLARGE_INTEGER    AllocationSize,
    __in ULONG             Disposition
	)
{
    LARGE_INTEGER   CurrentTime;
    LARGE_INTEGER   Size;
    FILE_END_OF_FILE_INFORMATION   fileEndOfFileInformation ;

    KeQuerySystemTime(&CurrentTime);

    Size.QuadPart = 0;
    if (!MmCanFileBeTruncated(&(Fcb->SectionObject), &(Size))) {
        return STATUS_USER_MAPPED_FILE;
    }

    /* purge all file cache and shrink cache windows size */
    CcPurgeCacheSection(&Fcb->SectionObject, NULL, 0, FALSE);
    Fcb->Header.AllocationSize.QuadPart =
        Fcb->Header.FileSize.QuadPart =
            Fcb->Header.ValidDataLength.QuadPart = 0;
    CcSetFileSizes(FileObject,
                   (PCC_FILE_SIZES)&Fcb->Header.AllocationSize);

    Size.QuadPart = CEILING_ALIGNED(ULONGLONG,
                                    (ULONGLONG)AllocationSize->QuadPart,
                                    (ULONGLONG)Vcb->SectorSize);

    SifsExpandAndTruncateFile(IrpContext, Fcb->Mcb, &Size);
	
    Fcb->Header.AllocationSize = Size;
    if (Fcb->Header.AllocationSize.QuadPart > 0) {
        SetLongFlag(Fcb->Flags, FCB_ALLOC_IN_CREATE);
        CcSetFileSizes(FileObject,
                       (PCC_FILE_SIZES)&Fcb->Header.AllocationSize );
    }
    
    Fcb->Lower->FileSize.QuadPart = 0;

    if (Disposition == FILE_SUPERSEDE) {
        Fcb->Lower->CreationTime.QuadPart = FsLinuxTime(CurrentTime);
    }
    Fcb->Lower->LastAccessTime.QuadPart =
        Fcb->Lower->LastWriteTime.QuadPart = FsLinuxTime(CurrentTime);

    //save
    fileEndOfFileInformation.EndOfFile = Fcb->Lower->FileSize;
    FltSetInformationFile(IrpContext->FltObjects->Instance, Fcb->Lower->FileObject
			, &fileEndOfFileInformation, sizeof(fileEndOfFileInformation), FileEndOfFileInformation);    

    return STATUS_SUCCESS;
}


FLT_PREOP_CALLBACK_STATUS
SifsCreateFile(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PBOOLEAN PostIrp
    )
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    NTSTATUS 				Status = STATUS_SUCCESS;
    PFLT_CALLBACK_DATA 	Data = IrpContext->Data;
    PUNICODE_STRING 		FileName = &(IrpContext->Parameters.Create.NameInfo->Name);
    PUNICODE_STRING 		LastFileName = &(IrpContext->Parameters.Create.NameInfo->FinalComponent);
    PVOLUME_CONTEXT 		VolumeContext = IrpContext->VolumeContext;

    PSIFS_MCB		   Mcb = NULL;
    PSIFS_FCB  	   Fcb = NULL;    
    PSIFS_CCB          Ccb = NULL;

    ULONG                 Options;
    ULONG                 CreateDisposition;

    BOOLEAN             bParentFcbCreated = FALSE;

    BOOLEAN             bDir = FALSE;
    BOOLEAN             bFcbAllocated = FALSE;
    BOOLEAN             bCreated = FALSE;
    BOOLEAN             bMainResourceAcquired = FALSE;

    BOOLEAN             SequentialOnly;
    BOOLEAN             NoIntermediateBuffering;
    BOOLEAN             IsPagingFile;
    BOOLEAN             NonDirectoryFile;
    BOOLEAN             NoEaKnowledge;
    BOOLEAN             DeleteOnClose;
    BOOLEAN             TemporaryFile;
    BOOLEAN             CaseSensitive;

    ACCESS_MASK     DesiredAccess;
    ULONG                 ShareAccess;

    Options  = Data->Iopb->Parameters.Create.Options;

    NonDirectoryFile = IsFlagOn(Options, FILE_NON_DIRECTORY_FILE);
    SequentialOnly = IsFlagOn(Options, FILE_SEQUENTIAL_ONLY);
    NoIntermediateBuffering = IsFlagOn( Options, FILE_NO_INTERMEDIATE_BUFFERING );
    NoEaKnowledge = IsFlagOn(Options, FILE_NO_EA_KNOWLEDGE);
    DeleteOnClose = IsFlagOn(Options, FILE_DELETE_ON_CLOSE);

    CaseSensitive = IsFlagOn(Data->Iopb->OperationFlags, SL_CASE_SENSITIVE);

    TemporaryFile = IsFlagOn(Data->Iopb->Parameters.Create.FileAttributes,
                             FILE_ATTRIBUTE_TEMPORARY );

    CreateDisposition = (Options >> 24) & 0x000000ff;

    IsPagingFile = IsFlagOn(Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE);

    DesiredAccess = Data->Iopb->Parameters.Create.SecurityContext->DesiredAccess;
    ShareAccess   = Data->Iopb->Parameters.Create.ShareAccess;

    __try{		

    	 Status = SifsLookupFile(IrpContext, VolumeContext, FileName, LastFileName, &Mcb);

	 if(!NT_SUCCESS(Status)) {

	     if(IrpContext->Parameters.Create.FileExist == FALSE) {
		 	
		     // We need to create a new one ?
	            if ((CreateDisposition == FILE_CREATE ) ||
	                    (CreateDisposition == FILE_SUPERSEDE) ||
	                    (CreateDisposition == FILE_OPEN_IF) ||
	                    (CreateDisposition == FILE_OVERWRITE_IF)) {

			  Status = SifsCreateUnderlyingFile(IrpContext, VolumeContext, FileName, LastFileName, &Mcb);

			  if(!NT_SUCCESS(Status)) {

				__leave;
			  }
	               
	                bCreated = TRUE;	               

	                Data->IoStatus.Information = FILE_CREATED;					
	                
	            }  else {

			  Status = STATUS_OBJECT_NAME_NOT_FOUND;	                
	                __leave;
	            }		     
			 
	     	}else{

		      if (CreateDisposition == FILE_CREATE) {

			   Status = STATUS_OBJECT_NAME_COLLISION;                
	                 __leave;
	             } 
			
			Status = SifsOpenUnderlyingFile(IrpContext, VolumeContext, FileName, LastFileName, &Mcb);

			if(!NT_SUCCESS(Status)) {

				__leave;
			}

		       Data->IoStatus.Information = FILE_OPENED;
	     	}        

		SifsReferMcb(Mcb);
		
	       SifsInsertMcb(VolumeContext, Mcb);
		   
	 }else{ //must IrpContext->Parameters.Create.FileExist == TRUE
	 	
            // We can not create if one exists
            if (CreateDisposition == FILE_CREATE) {

		  Status = STATUS_OBJECT_NAME_COLLISION;                
                __leave;
            }

            Data->IoStatus.Information = FILE_OPENED;
	 } 

       if(Mcb) {

		Fcb = Mcb->Fcb;

		if(Fcb == NULL) {
			
			Fcb = SifsAllocateFcb (Mcb);
			
			if (Fcb) {
				
			    bFcbAllocated = TRUE;
			} else {
			
			    Status = STATUS_INSUFFICIENT_RESOURCES;
			    __leave;
			}
		}

		SifsDerefMcb(Mcb);
       }
				
	if(Fcb) {

		/* grab Fcb's reference first to avoid the race between
               SifsClose  (it could free the Fcb we are accessing) */
            SifsReferXcb(&Fcb->ReferenceCount);

            ExAcquireResourceExclusiveLite(&Fcb->MainResource, TRUE);
            bMainResourceAcquired = TRUE;

            /* file delted ? */
            if (IsFlagOn(Fcb->Mcb->Flags, MCB_FILE_DELETED)) {
                Status = STATUS_FILE_DELETED;
                __leave;
            }

            if (DeleteOnClose) {
                Status = SifsIsFileRemovable(Fcb);
                if (!NT_SUCCESS(Status)) {
                    __leave;
                }
            }

            /* check access and oplock access for opened files */
            if (!bFcbAllocated) {

                /* whether there's batch oplock grabed on the file */
                if (FltCurrentBatchOplock (&Fcb->Oplock)) {

                    Data->IoStatus.Information = FILE_OPBATCH_BREAK_UNDERWAY;

                    /* break the batch lock if the sharing check fails */
                    FltCheckOplock ( &Fcb->Oplock,
                                               IrpContext->Data,
                                               IrpContext,
                                               SifsOplockComplete,
                                               SifsLockIrp );

		      if((IrpContext->Data->IoStatus.Status != STATUS_SUCCESS) &&
                            (IrpContext->Data->IoStatus.Status != STATUS_OPLOCK_BREAK_IN_PROGRESS)){

				*PostIrp = TRUE;
				__leave;
		      }
                }
            }

            if (bCreated) {

                //
                //  This file is just created.
                //
                 
                Fcb->Header.AllocationSize =
                    Data->Iopb->Parameters.Create.AllocationSize;

                if (Fcb->Header.AllocationSize.QuadPart > 0) {
                    Status = SifsExpandAndTruncateFile(IrpContext,
                                            Fcb->Mcb,
                                            &(Fcb->Header.AllocationSize)
                                           );
                    SetLongFlag(Fcb->Flags, FCB_ALLOC_IN_CREATE);
                    if (!NT_SUCCESS(Status)) {
                        Fcb->Header.AllocationSize.QuadPart = 0;
                        SifsExpandAndTruncateFile(IrpContext, Fcb->Mcb,
                                         &Fcb->Header.AllocationSize);
                        __leave;
                    }
                }
            } else {

                //
                //  This file alreayd exists.
                //

                if (DeleteOnClose) {

                    if (IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED)) {
						
                        Status = STATUS_MEDIA_WRITE_PROTECTED;
                        __leave;
                    }

                } else {

                    //
                    // Just to Open file (Open/OverWrite ...)
                    //

                    if ((IsFlagOn(IrpContext->FltObjects->FileObject->Flags,
                                                         FO_NO_INTERMEDIATE_BUFFERING))) {
                        Fcb->Header.IsFastIoPossible = FastIoIsPossible;

                        if (Fcb->SectionObject.DataSectionObject != NULL) {

                            if (Fcb->NonCachedOpenCount == Fcb->OpenHandleCount) {

                                if (!IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED)) {
                                    CcFlushCache(&Fcb->SectionObject, NULL, 0, NULL);
                                    ClearLongFlag(Fcb->Flags, FCB_FILE_MODIFIED);
                                }

                                CcPurgeCacheSection(&Fcb->SectionObject,
                                                    NULL,
                                                    0,
                                                    FALSE );
                            }
                        }
                    }
                }
            }


            if (!IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED)) {
                if ((CreateDisposition == FILE_SUPERSEDE) && !IsPagingFile) {
                    DesiredAccess |= DELETE;
                } else if (((CreateDisposition == FILE_OVERWRITE) ||
                            (CreateDisposition == FILE_OVERWRITE_IF)) && !IsPagingFile) {
                    DesiredAccess |= (FILE_WRITE_DATA | FILE_WRITE_EA |
                                      FILE_WRITE_ATTRIBUTES );
                }
            }

            if (!bFcbAllocated) {

                //
                //  check the oplock state of the file
                //

                FltCheckOplock(  &Fcb->Oplock,
                                            IrpContext->Data,
                                            IrpContext,
                                            SifsOplockComplete,
                                            SifsLockIrp );

                if ( (IrpContext->Data->IoStatus.Status != STATUS_SUCCESS) &&
                            (IrpContext->Data->IoStatus.Status != STATUS_OPLOCK_BREAK_IN_PROGRESS)) {
                    *PostIrp = TRUE;
                    __leave;
                }
            }

            if (Fcb->OpenHandleCount > 0) {

                /* check the shrae access conflicts */
                Status = IoCheckShareAccess( DesiredAccess,
                                             ShareAccess,
                                             IrpContext->FltObjects->FileObject,
                                             &(Fcb->ShareAccess),
                                             TRUE );
                if (!NT_SUCCESS(Status)) {
                    __leave;
                }

            } else {

                /* set share access rights */
                IoSetShareAccess( DesiredAccess,
                                  ShareAccess,
                                  IrpContext->FltObjects->FileObject,
                                  &(Fcb->ShareAccess) );
            }

            Ccb = SifsAllocateCcb();
            if (!Ccb) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                DbgBreak();
                __leave;
            }

            if (DeleteOnClose)
                SetLongFlag(Ccb->Flags, CCB_DELETE_ON_CLOSE);

            SifsReferXcb(&Fcb->OpenHandleCount);
            SifsReferXcb(&Fcb->ReferenceCount);

            if (NoIntermediateBuffering) {
                Fcb->NonCachedOpenCount++;
            } else {
                SetFlag(IrpContext->FltObjects->FileObject->Flags, FO_CACHE_SUPPORTED);
            }

            SifsReferXcb(&VolumeContext->OpenHandleCount);
            SifsReferXcb(&VolumeContext->ReferenceCount);

            IrpContext->FltObjects->FileObject->FsContext = (void*) Fcb;
            IrpContext->FltObjects->FileObject->FsContext2 = (void*) Ccb;
            IrpContext->FltObjects->FileObject->PrivateCacheMap = NULL;
            IrpContext->FltObjects->FileObject->SectionObjectPointer = &(Fcb->SectionObject);

            LOG_PRINT(LOGFL_CREATE, ( "FileFlt!SifsCreateFile: %wZ OpenCount=%u ReferCount=%u NonCachedCount=%u\n",
                            &Fcb->Mcb->FullName, Fcb->OpenHandleCount, Fcb->ReferenceCount, Fcb->NonCachedOpenCount));

            Status = STATUS_SUCCESS;

            if (bCreated) {

                SifsNotifyReportChange(
                        IrpContext,
                        VolumeContext,
                        Fcb->Mcb,
                        FILE_NOTIFY_CHANGE_FILE_NAME,
                        FILE_ACTION_ADDED );

            } else {

                if ( DeleteOnClose ||
                        IsFlagOn(DesiredAccess, FILE_WRITE_DATA) ||
                        (CreateDisposition == FILE_OVERWRITE) ||
                        (CreateDisposition == FILE_OVERWRITE_IF)) {
                    if (!MmFlushImageSection( &Fcb->SectionObject,
                                              MmFlushForWrite )) {

                        Status = DeleteOnClose ? STATUS_CANNOT_DELETE :
                                 STATUS_SHARING_VIOLATION;
                        __leave;
                    }
                }

                if ((CreateDisposition == FILE_SUPERSEDE) ||
                        (CreateDisposition == FILE_OVERWRITE) ||
                        (CreateDisposition == FILE_OVERWRITE_IF)) {

                    if (IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED)) {
                        Status = STATUS_MEDIA_WRITE_PROTECTED;
                        __leave;
                    }

                    Status = SifsSupersedeOrOverWriteFile(
                                 IrpContext,
                                 IrpContext->FltObjects->FileObject,
                                 VolumeContext,
                                 Fcb,
                                 &Data->Iopb->Parameters.Create.AllocationSize,
                                 CreateDisposition );

                    if (!NT_SUCCESS(Status)) {
                        DbgBreak();
                        __leave;
                    }

                    SifsNotifyReportChange(
                        IrpContext,
                        VolumeContext,
                        Fcb->Mcb,
                        FILE_NOTIFY_CHANGE_LAST_WRITE |
                        FILE_NOTIFY_CHANGE_ATTRIBUTES |
                        FILE_NOTIFY_CHANGE_SIZE,
                        FILE_ACTION_MODIFIED );


                    if (CreateDisposition == FILE_SUPERSEDE) {
                        Data->IoStatus.Information = FILE_SUPERSEDED;
                    } else {
                        Data->IoStatus.Information = FILE_OVERWRITTEN;
                    }
                }
            }
	}else{

		
	}
	 
    }__finally{

	 if(!NT_SUCCESS(Status)) {

		 if (Ccb != NULL) {

                DbgBreak();

                ASSERT(Fcb != NULL);
                ASSERT(Fcb->Mcb != NULL);

                SifsDerefXcb(&Fcb->OpenHandleCount);
                SifsDerefXcb(&Fcb->ReferenceCount);

                if (NoIntermediateBuffering) {
                    Fcb->NonCachedOpenCount--;
                } else {
                    ClearFlag(IrpContext->FltObjects->FileObject->Flags, FO_CACHE_SUPPORTED);
                }

                SifsDerefXcb(&VolumeContext->OpenHandleCount);
                SifsDerefXcb(&VolumeContext->ReferenceCount);

                IoRemoveShareAccess(IrpContext->FltObjects->FileObject, &Fcb->ShareAccess);

                IrpContext->FltObjects->FileObject->FsContext = NULL;
                IrpContext->FltObjects->FileObject->FsContext2 = NULL;
                IrpContext->FltObjects->FileObject->PrivateCacheMap = NULL;
                IrpContext->FltObjects->FileObject->SectionObjectPointer = NULL;

                SifsFreeCcb(Ccb);
            }
        }

        if (Fcb && SifsDerefXcb(&Fcb->ReferenceCount) == 0) {

            if (IsFlagOn(Fcb->Flags, FCB_ALLOC_IN_CREATE)) {

                LARGE_INTEGER Size;
                ExAcquireResourceExclusiveLite(&Fcb->PagingIoResource, TRUE);
                __try {
                    Size.QuadPart = 0;
                    SifsExpandAndTruncateFile(IrpContext,  Fcb->Mcb, &Size);
                } __finally {
                    ExReleaseResourceLite(&Fcb->PagingIoResource);
                }
            }

            if (bCreated) {
                SifsDeleteFile(IrpContext, Fcb, Mcb);
            }

            SifsFreeFcb(Fcb);
            Fcb = NULL;
            bMainResourceAcquired = FALSE;
        }

        if (bMainResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }       

	 Data->IoStatus.Status = Status;
    }    
	
    return retValue ;	 
}
	
FLT_PREOP_CALLBACK_STATUS
SifsCommonCreate(
    __in PSIFS_IRP_CONTEXT IrpContext
    )
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
    BOOLEAN             PostIrp = FALSE;
    NTSTATUS 		   Status = STATUS_SUCCESS;
    BOOLEAN             VcbResourceAcquired = FALSE;
    PVOLUME_CONTEXT VolumeContext = IrpContext->VolumeContext;
    
    __try{

	if (!ExAcquireResourceExclusiveLite(
                    &VolumeContext->MainResource, TRUE)) {
                    
            Status = STATUS_PENDING;
			
            __leave;
        }
        VcbResourceAcquired = TRUE;	 

	 retValue = SifsCreateFile(IrpContext, &PostIrp);

	 Status = IrpContext->Data->IoStatus.Status;
		
    }__finally{

        if (VcbResourceAcquired) {
			
            ExReleaseResourceLite(&VolumeContext->MainResource);
        }
		
	if (!IrpContext->ExceptionInProgress && !PostIrp)  {
		
            if ( (Status == STATUS_PENDING) ||
                    (Status == STATUS_CANT_WAIT)) {
                    
                Status = SifsQueueRequest(IrpContext);

            } else {
            
                SifsCompleteIrpContext(IrpContext, Status);
            }
        }

	if(Status == STATUS_PENDING) {

		retValue = FLT_PREOP_PENDING;
	}
    }
    
    return retValue;
}

