#include "fileflt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsCommonCleanup)
#endif


FLT_PREOP_CALLBACK_STATUS
SifsCommonCleanup (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
	
    NTSTATUS        Status = STATUS_SUCCESS;
    PFILE_OBJECT    FileObject = NULL;
    PSIFS_FCB       Fcb = NULL;
    PSIFS_CCB       Ccb = NULL;
    PSIFS_MCB       Mcb = NULL;
    PVOLUME_CONTEXT VolumeContext = IrpContext->VolumeContext;


    BOOLEAN         VcbResourceAcquired = FALSE;
    BOOLEAN         FcbResourceAcquired = FALSE;
    BOOLEAN         FcbPagingIoResourceAcquired = FALSE;

    __try {

        ASSERT(IrpContext != NULL);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileObject = IrpContext->FltObjects->FileObject;
        Fcb = (PSIFS_FCB) FileObject->FsContext;
        if (!Fcb || (Fcb->Identifier.Type != SIFSFCB)) {
            Status = STATUS_SUCCESS;
            __leave;
        }
        Mcb = Fcb->Mcb;
        Ccb = (PSIFS_CCB) FileObject->FsContext2;

        if (IsFlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE)) {
            Status = STATUS_SUCCESS;
            __leave;
        }

        VcbResourceAcquired =
            ExAcquireResourceExclusiveLite(
                &VolumeContext->MainResource,
                IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)
            );
        
        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        if (IsFlagOn(FileObject->Flags, FO_CLEANUP_COMPLETE)) {
            if (IsFlagOn(FileObject->Flags, FO_FILE_MODIFIED) &&
                    !IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED) ) {
                Status = SifsFlushFile(IrpContext, Fcb, Ccb);
            }
            __leave;
        }

        if (Ccb == NULL) {
            Status = STATUS_SUCCESS;
            __leave;
        }        

        ExReleaseResourceLite(&VolumeContext->MainResource);
        VcbResourceAcquired = FALSE;

        FcbResourceAcquired =
            ExAcquireResourceExclusiveLite(
                &Fcb->MainResource,
                IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)
            );

        ASSERT((Ccb->Identifier.Type == SIFSCCB) &&
               (Ccb->Identifier.Size == sizeof(SIFS_CCB)));

        SifsDerefXcb(&VolumeContext->OpenHandleCount);
        SifsDerefXcb(&Fcb->OpenHandleCount);

        if (IsFlagOn(FileObject->Flags, FO_FILE_MODIFIED)) {
            Fcb->Mcb->FileAttr |= FILE_ATTRIBUTE_ARCHIVE;
        }

        if ( IsFlagOn(FileObject->Flags, FO_FILE_MODIFIED) &&
                !IsFlagOn(Ccb->Flags, CCB_LAST_WRITE_UPDATED)) {

            LARGE_INTEGER   SysTime;
            KeQuerySystemTime(&SysTime);

            Fcb->Lower->CreationTime.QuadPart =
                Fcb->Lower->ChangeTime.QuadPart = FsLinuxTime(SysTime);
            Fcb->Lower->LastAccessTime =
                Fcb->Lower->LastWriteTime = FsNtTime(Fcb->Lower->CreationTime.QuadPart);

            SifsNotifyReportChange(
                IrpContext,
                VolumeContext,
                Fcb->Mcb,
                FILE_NOTIFY_CHANGE_ATTRIBUTES |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_LAST_ACCESS,
                FILE_ACTION_MODIFIED );
        }

	 FltCheckOplock(  &Fcb->Oplock,
                        IrpContext->Data,
                        IrpContext,
                        NULL,
                        NULL );

        Fcb->Header.IsFastIoPossible = SifsIsFastIoPossible(Fcb);

        if (!IsFlagOn(FileObject->Flags, FO_CACHE_SUPPORTED)) {
            Fcb->NonCachedOpenCount--;
        }

        if (IsFlagOn(Ccb->Flags, CCB_DELETE_ON_CLOSE))  {
            SetLongFlag(Fcb->Flags, FCB_DELETE_PENDING);
        }

        //
        // Drop any byte range locks this process may have on the file.
        //

        FsRtlFastUnlockAll(
            &Fcb->FileLockAnchor,
            FileObject,
            IoThreadToProcess(IrpContext->Data->Thread),
            NULL  );

        //
        // If there are no byte range locks owned by other processes on the
        // file the fast I/O read/write functions doesn't have to check for
        // locks so we set IsFastIoPossible to FastIoIsPossible again.
        //
        if (!FsRtlGetNextFileLock(&Fcb->FileLockAnchor, TRUE)) {
            if (Fcb->Header.IsFastIoPossible != FastIoIsPossible) {

                Fcb->Header.IsFastIoPossible = FastIoIsPossible;
            }
        }

        if (Fcb->OpenHandleCount == 0 &&
                (IsFlagOn(Fcb->Flags, FCB_ALLOC_IN_CREATE) ||
                 IsFlagOn(Fcb->Flags, FCB_ALLOC_IN_WRITE)) ) {

            LARGE_INTEGER Size;

            ExAcquireResourceExclusiveLite(&Fcb->PagingIoResource, TRUE);
            FcbPagingIoResourceAcquired = TRUE;

            Size.QuadPart = CEILING_ALIGNED(ULONGLONG,
                                            (ULONGLONG)Fcb->Mcb->Lower.AllocationSize.QuadPart,
                                            (ULONGLONG)VolumeContext->SectorSize);
            if (!IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING)) {

                SifsExpandAndTruncateFile(IrpContext, Fcb->Mcb, &Size);
                Fcb->Header.ValidDataLength.QuadPart =
                    Fcb->Header.FileSize.QuadPart = Fcb->Mcb->Lower.FileSize.QuadPart;
                Fcb->Header.AllocationSize = Size;
                if (CcIsFileCached(FileObject)) {
                    CcSetFileSizes(FileObject,
                                   (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
                }
            }
            ClearLongFlag(Fcb->Flags, FCB_ALLOC_IN_CREATE|FCB_ALLOC_IN_WRITE);
            ExReleaseResourceLite(&Fcb->PagingIoResource);
            FcbPagingIoResourceAcquired = FALSE;
        }

        if (IsFlagOn(Fcb->Flags, FCB_DELETE_PENDING)) {

            if (Fcb->OpenHandleCount == 0) {

                //
                // Ext2DeleteFile will acquire these lock inside
                //

                if (FcbResourceAcquired) {
                    ExReleaseResourceLite(&Fcb->MainResource);
                    FcbResourceAcquired = FALSE;
                }

                //
                //  this file is to be deleted ...
                //
                
                Status = SifsDeleteFile(IrpContext, Fcb, Mcb);

                if (NT_SUCCESS(Status)) {
                       SifsNotifyReportChange( IrpContext, VolumeContext, Mcb,
                                                FILE_NOTIFY_CHANGE_FILE_NAME,
                                                FILE_ACTION_REMOVED );
                }

                //
                // re-acquire the main resource lock
                //

                FcbResourceAcquired =
                    ExAcquireResourceExclusiveLite(
                        &Fcb->MainResource,
                        IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)
                    );

                if (CcIsFileCached(FileObject)) {
                    CcSetFileSizes(FileObject,
                                   (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
                    SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
                }
            }
        }

        
        if ( IsFlagOn(FileObject->Flags, FO_CACHE_SUPPORTED) &&
                (Fcb->NonCachedOpenCount + 1 == Fcb->ReferenceCount) &&
                (Fcb->SectionObject.DataSectionObject != NULL)) {

            if ( !IsFlagOn(VolumeContext->Flags, VCB_WRITE_PROTECTED) ) {
                CcFlushCache(&Fcb->SectionObject, NULL, 0, NULL);
            }

            if (ExAcquireResourceExclusiveLite(&(Fcb->PagingIoResource), TRUE)) {
                ExReleaseResourceLite(&(Fcb->PagingIoResource));
            }

            CcPurgeCacheSection( &Fcb->SectionObject,
                                 NULL,
                                 0,
                                 FALSE );
        }

        CcUninitializeCacheMap(FileObject, NULL, NULL);

        IoRemoveShareAccess(FileObject, &Fcb->ShareAccess);

        LOG_PRINT(LOGFL_CLOSE, ( "FileFlt!SifsCommonCleanup: OpenCount=%u ReferCount=%u NonCahcedCount=%xh %wZ\n",
                        Fcb->OpenHandleCount, Fcb->ReferenceCount, Fcb->NonCachedOpenCount, &Fcb->Mcb->FullName));

        Status = STATUS_SUCCESS;

        if (FileObject) {
            SetFlag(FileObject->Flags, FO_CLEANUP_COMPLETE);
        }

    } __finally {

        if (FcbPagingIoResourceAcquired) {
            ExReleaseResourceLite(&Fcb->PagingIoResource);
        }

        if (FcbResourceAcquired) {
            ExReleaseResourceLite(&Fcb->MainResource);
        }

        if (VcbResourceAcquired) {
            ExReleaseResourceLite(&VolumeContext->MainResource);
        }

        if (!IrpContext->ExceptionInProgress) {
            if (Status == STATUS_PENDING) {
				
                SifsQueueRequest(IrpContext);
		   retValue = FLT_PREOP_PENDING;
            } else {
                IrpContext->Data->IoStatus.Status = Status;
                SifsCompleteIrpContext(IrpContext, Status);
            }
        }else{

		IrpContext->Data->IoStatus.Status = Status;
        }
    }

    return retValue;
}
