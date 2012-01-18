#include "fileflt.h"

BOOLEAN
SifsZeroHoles (
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PVOLUME_CONTEXT VolumeContext,
    __in PFILE_OBJECT FileObject,
    __in LONGLONG Offset,
    __in LONGLONG Count
)
{
    LARGE_INTEGER Start, End;

    Start.QuadPart = Offset;
    End.QuadPart = Offset + Count;

    if (Count > 0) {
        return CcZeroData(FileObject, &Start, &End, PIN_WAIT);
    }

    return TRUE;
}

VOID
SifsDeferWrite(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PFLT_CALLBACK_DATA Data
	)
{
    ASSERT(IrpContext->Data == Data);

    SifsQueueRequest(IrpContext);
}

NTSTATUS
SifsWriteFile(
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    NTSTATUS            Status = STATUS_UNSUCCESSFUL;

    PFLT_CALLBACK_DATA Data = IrpContext->Data;
    PVOLUME_CONTEXT Vcb = IrpContext->VolumeContext;
    PFLT_IO_PARAMETER_BLOCK Iopb = IrpContext->Data->Iopb;
    PSIFS_FCB           Fcb;
    PSIFS_CCB           Ccb;
    PFILE_OBJECT        FileObject;
    PFILE_OBJECT        CacheObject;

    ULONG               Length;
    LARGE_INTEGER       ByteOffset;
    ULONG               ReturnedLength = 0;

    BOOLEAN             OpPostIrp = FALSE;
    BOOLEAN             PagingIo;
    BOOLEAN             Nocache;
    BOOLEAN             SynchronousIo;

    BOOLEAN             RecursiveWriteThrough = FALSE;
    BOOLEAN             MainResourceAcquired = FALSE;
    BOOLEAN             PagingIoResourceAcquired = FALSE;

    BOOLEAN             bDeferred = FALSE;
    BOOLEAN             FileSizesChanged = FALSE;

    PUCHAR              Buffer;

    __try {

        ASSERT(IrpContext);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileObject = IrpContext->FileObject;
        Fcb = (PSIFS_FCB) FileObject->FsContext;
        Ccb = (PSIFS_CCB) FileObject->FsContext2;
        ASSERT(Fcb);
        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        Length = Iopb->Parameters.Write.Length;
        ByteOffset = Iopb->Parameters.Write.ByteOffset;

        PagingIo = IsFlagOn(Iopb->IrpFlags, IRP_PAGING_IO);
        Nocache = IsFlagOn(Iopb->IrpFlags, IRP_NOCACHE);
        SynchronousIo = IsFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

        if (PagingIo) {
            ASSERT(Nocache);
        }

        if (IsFileDeleted(Fcb->Mcb)) {
            Status = STATUS_FILE_DELETED;
            __leave;
        }

        if (Length == 0) {
            IrpContext->Data->IoStatus.Information = 0;
            Status = STATUS_SUCCESS;
            __leave;
        }

        if (Nocache && ( (ByteOffset.LowPart & (Vcb->SectorSize - 1)) ||
                         (Length & (Vcb->SectorSize - 1))) ) {
            Status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        if (FlagOn(Iopb->MinorFunction, IRP_MN_DPC)) {
            ClearFlag(Iopb->MinorFunction, IRP_MN_DPC);
            Status = STATUS_PENDING;
            __leave;
        }

        if (!Nocache) {

            BOOLEAN bAgain = IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_DEFERRED);
            BOOLEAN bWait  = IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_WAIT);
            BOOLEAN bQueue = IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_REQUEUED);

            if ( !CcCanIWrite(
                        FileObject,
                        Length,
                        (bWait && bQueue),
                        bAgain ) ) {

                Status = SifsLockUserBuffer(
                             IrpContext->Data,
                             Length,
                             IoReadAccess);

                if (NT_SUCCESS(Status)) {
                    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_DEFERRED);
                    CcDeferWrite( FileObject,
                                  (PCC_POST_DEFERRED_WRITE)SifsDeferWrite,
                                  IrpContext,
                                  IrpContext->Data,
                                  Length,
                                  bAgain );
                    bDeferred = TRUE;
                    Status = STATUS_PENDING;
                    __leave;
                }
            }
        }

        if (IsEndOfFile(ByteOffset)) {
            ByteOffset.QuadPart = Fcb->Header.FileSize.QuadPart;
        }

        if (IsFlagOn(Iopb->IrpFlags, IRP_SYNCHRONOUS_PAGING_IO) && !IrpContext->IsTopLevel) {

            PFLT_CALLBACK_DATA TopData = IoGetTopLevelIrp();

	     if ( ((ULONG_PTR)TopData > FSRTL_MAX_TOP_LEVEL_IRP_FLAG) &&
                    (TopData == IrpContext->Data)) {
                    
	            if ((TopData->Iopb->MajorFunction == IRP_MJ_WRITE) &&
	                        (TopData->Iopb->TargetFileObject->FsContext == FileObject->FsContext)) {

	                    RecursiveWriteThrough = TRUE;
	            }
	     	}
        }

        //
        //  Do flushing for such cases
        //
        if (Nocache && !PagingIo && Ccb != NULL &&
                (Fcb->SectionObject.DataSectionObject != NULL))  {

            MainResourceAcquired =
                ExAcquireResourceExclusiveLite( &Fcb->MainResource,
                                                IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT));

            ExAcquireSharedStarveExclusive( &Fcb->PagingIoResource, TRUE);
            ExReleaseResourceLite(&Fcb->PagingIoResource);

            CcFlushCache( &(Fcb->SectionObject),
                          &ByteOffset,
                          CEILING_ALIGNED(ULONG, Length, Vcb->SectorSize),
                          &(IrpContext->Data->IoStatus));
            ClearLongFlag(Fcb->Flags, FCB_FILE_MODIFIED);

            if (!NT_SUCCESS(IrpContext->Data->IoStatus.Status)) {
                Status = IrpContext->Data->IoStatus.Status;
                __leave;
            }

            ExAcquireSharedStarveExclusive( &Fcb->PagingIoResource, TRUE);
            ExReleaseResourceLite(&Fcb->PagingIoResource);

            CcPurgeCacheSection( &(Fcb->SectionObject),
                                 (PLARGE_INTEGER)&(ByteOffset),
                                 CEILING_ALIGNED(ULONG, Length, Vcb->SectorSize),
                                 FALSE );

            if (MainResourceAcquired) {
                ExReleaseResourceLite(&Fcb->MainResource);
                MainResourceAcquired = FALSE;
            }
        }

        if (!PagingIo) {

            if (!ExAcquireResourceExclusiveLite(
                        &Fcb->MainResource,
                        IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_WAIT) )) {
                Status = STATUS_PENDING;
                __leave;
            }

            MainResourceAcquired = TRUE;

            if (!FltCheckLockForWriteAccess(
                        &Fcb->FileLockAnchor,
                        IrpContext->Data)) {
                Status = STATUS_FILE_LOCK_CONFLICT;
                __leave;
            }

        } else {

            if (!ExAcquireResourceSharedLite(
                        &Fcb->PagingIoResource,
                        IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_WAIT) )) {
                Status = STATUS_PENDING;
                __leave;
            }

            PagingIoResourceAcquired = TRUE;
        }

        Status = FltCheckOplock( &Fcb->Oplock,
                                   IrpContext->Data,
                                   IrpContext,
                                   SifsOplockComplete,
                                   SifsLockIrp );

        if (Status != STATUS_SUCCESS) {
            OpPostIrp = TRUE;
            __leave;
        }

        //
        //  Set the flag indicating if Fast I/O is possible
        //

        Fcb->Header.IsFastIoPossible = SifsIsFastIoPossible(Fcb);

        if (PagingIo) {

            if ( (ByteOffset.QuadPart + Length) > Fcb->Header.AllocationSize.QuadPart) {

                if ( ByteOffset.QuadPart >= Fcb->Header.AllocationSize.QuadPart) {

                    Status = STATUS_END_OF_FILE;
                    IrpContext->Data->IoStatus.Information = 0;
                    __leave;

                } else {

                    Length = (ULONG)(Fcb->Header.AllocationSize.QuadPart - ByteOffset.QuadPart);
                }
            }

        } else {

            //
            //  Extend the inode size when the i/o is beyond the file end ?
            //

            if ((ByteOffset.QuadPart + Length) > Fcb->Header.FileSize.QuadPart) {

                LARGE_INTEGER AllocationSize, Last;

                Last.QuadPart = Fcb->Header.AllocationSize.QuadPart;
                AllocationSize.QuadPart = (LONGLONG)(ByteOffset.QuadPart + Length);
                AllocationSize.QuadPart = CEILING_ALIGNED(ULONGLONG,
                                          (ULONGLONG)AllocationSize.QuadPart,
                                          (ULONGLONG)Vcb->SectorSize);
                Status = SifsExpandAndTruncateFile(IrpContext, Fcb->Mcb, &AllocationSize);
                if (AllocationSize.QuadPart > Last.QuadPart) {
                    Fcb->Header.AllocationSize.QuadPart = AllocationSize.QuadPart;
                    SetLongFlag(Fcb->Flags, FCB_ALLOC_IN_WRITE);
                }

                if (ByteOffset.QuadPart >= Fcb->Header.AllocationSize.QuadPart) {
                    if (NT_SUCCESS(Status)) {
                        DbgBreak();
                        Status = STATUS_UNSUCCESSFUL;
                    }
                    __leave;
                }

                if (ByteOffset.QuadPart + Length > Fcb->Header.AllocationSize.QuadPart) {
                    Length = (ULONG)(Fcb->Header.AllocationSize.QuadPart - ByteOffset.QuadPart);
                }

                Fcb->Header.FileSize.QuadPart = Fcb->Lower->FileSize.QuadPart = ByteOffset.QuadPart + Length;

                if (CcIsFileCached(FileObject)) {
                    CcSetFileSizes(FileObject, (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
                    if (AllocationSize.QuadPart > Last.QuadPart)
                        CcZeroData(FileObject, &Last, &AllocationSize, FALSE);
                }

                FileSizesChanged = TRUE;

            }
        }

        ReturnedLength = Length;

        if (!Nocache) {

            if (FileObject->PrivateCacheMap == NULL) {
                CcInitializeCacheMap(
                    FileObject,
                    (PCC_FILE_SIZES)(&Fcb->Header.AllocationSize),
                    FALSE,
                    &g_FileFltContext.CacheManagerCallbacks,
                    Fcb );

                CcSetReadAheadGranularity(
                    FileObject,
                    READ_AHEAD_GRANULARITY );
            }

            CacheObject = FileObject;

            if (FlagOn(Iopb->MinorFunction, IRP_MN_MDL)) {

                CcPrepareMdlWrite(
                    CacheObject,
                    (&ByteOffset),
                    Length,
                    &Iopb->Parameters.Write.MdlAddress,
                    &IrpContext->Data->IoStatus );

                Status = IrpContext->Data->IoStatus.Status;

            } else {

                Buffer = SifsGetUserBufferOnWrite(Iopb);
                if (Buffer == NULL) {
                    DbgBreak();
                    Status = STATUS_INVALID_USER_BUFFER;
                    __leave;
                }

                if (!CcCopyWrite(
                            CacheObject,
                            (PLARGE_INTEGER)&ByteOffset,
                            Length,
                            IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                            Buffer  )) {
                    Status = STATUS_PENDING;
                    DbgBreak();
                    __leave;
                }

                Status = STATUS_SUCCESS;
            }

            if (NT_SUCCESS(Status)) {
                IrpContext->Data->IoStatus.Information = Length;
            }

        } else {

            if (!PagingIo && CcIsFileCached(FileObject) && !RecursiveWriteThrough &&
                    Fcb->LazyWriterThread != PsGetCurrentThread() &&
                    ByteOffset.QuadPart > Fcb->Header.ValidDataLength.QuadPart) {

                SifsZeroHoles( IrpContext, Vcb, FileObject, Fcb->Header.ValidDataLength.QuadPart,
                               ByteOffset.QuadPart - Fcb->Header.ValidDataLength.QuadPart );
            }

            Status = SifsLockUserBuffer(
                         IrpContext->Data,
                         Length,
                         IoReadAccess );

            if (!NT_SUCCESS(Status)) {
                __leave;
            }

            IrpContext->Data->IoStatus.Status = STATUS_SUCCESS;
            IrpContext->Data->IoStatus.Information = ReturnedLength;

            Status = FltWriteFile(
                         IrpContext->FltObjects->Instance,
                         Fcb->Lower->FileObject,
                         &ByteOffset,
                         ReturnedLength,
                         SifsGetUserBufferOnWrite(Iopb),
                         0,
                         &Length,
                         NULL,
                         NULL
                     );

            Data = IrpContext->Data;
        }

        /* Update files's ValidDateLength */
        if (NT_SUCCESS(Status) && !PagingIo && CcIsFileCached(FileObject) &&
                !RecursiveWriteThrough && Fcb->LazyWriterThread != PsGetCurrentThread()) {

            if (Fcb->Header.ValidDataLength.QuadPart < ByteOffset.QuadPart + Length) {
                Fcb->Header.ValidDataLength.QuadPart = ByteOffset.QuadPart + Length;
                FileSizesChanged = TRUE;
            }

            if (Fcb->Mcb->ValidDataLength.QuadPart < (Fcb->Header.ValidDataLength.QuadPart)) {
                Fcb->Header.ValidDataLength = Fcb->Mcb->ValidDataLength;
                FileSizesChanged = TRUE;
            }

            if (CcIsFileCached(FileObject)) {
                CcSetFileSizes(FileObject, (PCC_FILE_SIZES)(&(Fcb->Header.AllocationSize)));
            }

            LOG_PRINT(LOGFL_WRITE, ("FileFlt!SifsWriteFile: %wZ written FS: %I64xh FA: %I64xh BO: %I64xh LEN: %u\n",
                          &Fcb->Mcb->ShortName, Fcb->Header.FileSize.QuadPart,
                          Fcb->Header.AllocationSize.QuadPart, ByteOffset.QuadPart, Length));
        }

        if (FileSizesChanged) {
            SifsNotifyReportChange( IrpContext,  Vcb, Fcb->Mcb,
                                    FILE_NOTIFY_CHANGE_SIZE,
                                    FILE_ACTION_MODIFIED );
        }

    } __finally {

        if (Data) {
            if (PagingIoResourceAcquired) {
                ExReleaseResourceLite(&Fcb->PagingIoResource);
            }

            if (MainResourceAcquired) {
                ExReleaseResourceLite(&Fcb->MainResource);
            }
        }

        if (!OpPostIrp && !IrpContext->ExceptionInProgress) {

            if (Data) {

                if ((Status == STATUS_PENDING) ||
                        (Status == STATUS_CANT_WAIT) ) {

                    if (!bDeferred) {
                        Status = SifsQueueRequest(IrpContext);
                    }

                } else {

                    if (NT_SUCCESS(Status)) {

                        if (SynchronousIo && !PagingIo) {
                            FileObject->CurrentByteOffset.QuadPart =
                                ByteOffset.QuadPart + IrpContext->Data->IoStatus.Information;
                        }

                        if (!PagingIo) {
                            SetFlag(FileObject->Flags, FO_FILE_MODIFIED);
                            SetLongFlag(Fcb->Flags, FCB_FILE_MODIFIED);
                        }
                    }

                    SifsCompleteIrpContext(IrpContext, Status);
                }
            } else {
                SifsFreeIrpContext(IrpContext);
            }
        }
    }

    return Status;
}

NTSTATUS
SifsWriteComplete (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;
    PFLT_IO_PARAMETER_BLOCK Iopb = IrpContext->Data->Iopb;
    PFILE_OBJECT    FileObject;

    __try {

        ASSERT(IrpContext);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileObject = IrpContext->FileObject;

        CcMdlWriteComplete(FileObject, &(Iopb->Parameters.Write.ByteOffset), Iopb->Parameters.Write.MdlAddress);
        Iopb->Parameters.Write.MdlAddress = NULL;
        Status = STATUS_SUCCESS;

    } __finally {

        if (!IrpContext->ExceptionInProgress) {
            SifsCompleteIrpContext(IrpContext, Status);
        }
    }

    return Status;
}


FLT_PREOP_CALLBACK_STATUS
SifsCommonWrite (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
    PFLT_CALLBACK_DATA      Data = IrpContext->Data;
    NTSTATUS            	Status = STATUS_SUCCESS;
    PVOLUME_CONTEXT  Vcb = IrpContext->VolumeContext;
    PSIFS_FCBVCB        FcbOrVcb;
    PFILE_OBJECT         FileObject;    
    BOOLEAN                bCompleteRequest = TRUE;
    

    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));


    if(FLT_IS_FASTIO_OPERATION(IrpContext->Data)) {

        retValue = SifsFastIoWrite(IrpContext);

	 goto SifsCommonWriteCleanup;
    }

	
    __try {

        if (IsFlagOn(IrpContext->Data->Iopb->MinorFunction, IRP_MN_COMPLETE)) {

            Status =  SifsWriteComplete(IrpContext);
            bCompleteRequest = FALSE;

        } else {
            
            if (IsFlagOn(Vcb->Flags, VCB_DISMOUNT_PENDING)) {
                Status = STATUS_TOO_LATE;
                __leave;
            }

            if (IsFlagOn(Vcb->Flags, VCB_READ_ONLY)) {
                Status = STATUS_MEDIA_WRITE_PROTECTED;
                __leave;
            }

            FileObject = IrpContext->FileObject;

            FcbOrVcb = (PSIFS_FCBVCB) FileObject->FsContext;

            if (FcbOrVcb->Identifier.Type == SIFSFCB) {

                Status = SifsWriteFile(IrpContext);
                if (!NT_SUCCESS(Status)) {
                    DbgBreak();
                }

                bCompleteRequest = FALSE;
            } else {
                Status = STATUS_INVALID_PARAMETER;
            }
        }

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

SifsCommonWriteCleanup:
	
    return retValue;
}

