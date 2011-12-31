#include "fileflt.h"

#define SafeZeroMemory(AT,BYTE_COUNT) {                                 \
    __try {                                                             \
        if (AT)                                                         \
            RtlZeroMemory((AT), (BYTE_COUNT));                          \
    } __except(EXCEPTION_EXECUTE_HANDLER) {                             \
         SifsRaiseStatus( IrpContext, STATUS_INVALID_USER_BUFFER );     \
    }                                                                   \
}

NTSTATUS
SifsCompleteIrpContext (
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in NTSTATUS Status 
    )
{
    BOOLEAN bPrint;

    if (IrpContext->Data != NULL) {

        if (NT_ERROR(Status)) {
            IrpContext->Data->IoStatus.Information = 0;
        }

        IrpContext->Data->IoStatus.Status = Status;
        IrpContext->Data = NULL;
    }

   SifsFreeIrpContext(IrpContext);

    return Status;
}

NTSTATUS
SifsReadFile(
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    NTSTATUS            Status = STATUS_UNSUCCESSFUL;

    PVOLUME_CONTEXT   Vcb = IrpContext->VolumeContext;
    PFLT_IO_PARAMETER_BLOCK Iopb = IrpContext->Data->Iopb;
    PSIFS_FCB           Fcb;
    PSIFS_CCB           Ccb;
    PFILE_OBJECT        FileObject;
    PFILE_OBJECT        CacheObject;

    ULONG               Length;
    ULONG               ReturnedLength;
    LARGE_INTEGER       ByteOffset;

    BOOLEAN             OpPostIrp = FALSE;
    BOOLEAN             PagingIo;
    BOOLEAN             Nocache;
    BOOLEAN             SynchronousIo;
    BOOLEAN             MainResourceAcquired = FALSE;
    BOOLEAN             PagingIoResourceAcquired = FALSE;

    PUCHAR              Buffer;

    __try {

        ASSERT(IrpContext);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileObject = IrpContext->FileObject;
        Fcb = (PSIFS_FCB) FileObject->FsContext;
        ASSERT(Fcb);
        ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
               (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

        Ccb = (PSIFS_CCB) FileObject->FsContext2;

        Length = Iopb->Parameters.Read.Length;
        ByteOffset = Iopb->Parameters.Read.ByteOffset;

        PagingIo = IsFlagOn(Iopb->IrpFlags, IRP_PAGING_IO);
        Nocache = IsFlagOn(Iopb->IrpFlags, IRP_NOCACHE);
        SynchronousIo = IsFlagOn(FileObject->Flags, FO_SYNCHRONOUS_IO);

        if (PagingIo) {
            ASSERT(Nocache);
        }

        if (Length == 0) {
            IrpContext->Data->IoStatus.Information = 0;
            Status = STATUS_SUCCESS;
            __leave;
        }

        if (Nocache &&
                (ByteOffset.LowPart & (Vcb->SectorSize - 1) ||
                 Length & (Vcb->SectorSize - 1))) {
            Status = STATUS_INVALID_PARAMETER;
            DbgBreak();
            __leave;
        }

        if (FlagOn(Iopb->MinorFunction, IRP_MN_DPC)) {
            ClearFlag(Iopb->MinorFunction, IRP_MN_DPC);
            Status = STATUS_PENDING;
            DbgBreak();
            __leave;
        }

        if (!PagingIo && Nocache && (FileObject->SectionObjectPointer->DataSectionObject != NULL)) {
            CcFlushCache( FileObject->SectionObjectPointer,
                          &ByteOffset,
                          Length,
                          &IrpContext->Data->IoStatus );

            if (!NT_SUCCESS(IrpContext->Data->IoStatus.Status)) {
                __leave;
            }
        }

        ReturnedLength = Length;

        if (PagingIo) {

            if (!ExAcquireResourceSharedLite(
                        &Fcb->PagingIoResource,
                        IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT) )) {
                Status = STATUS_PENDING;
                __leave;
            }
            PagingIoResourceAcquired = TRUE;

            if ((ByteOffset.QuadPart + (LONGLONG)Length) > Fcb->Header.AllocationSize.QuadPart) {
                if (ByteOffset.QuadPart >= Fcb->Header.AllocationSize.QuadPart) {
                    IrpContext->Data->IoStatus.Information = 0;
                    Status = STATUS_END_OF_FILE;
                    __leave;
                }
                ReturnedLength = (ULONG)(Fcb->Header.AllocationSize.QuadPart - ByteOffset.QuadPart);
            }
        } else {

            if (Nocache) {

                if (!ExAcquireResourceExclusiveLite(
                            &Fcb->MainResource,
                            IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_WAIT) )) {
                    Status = STATUS_PENDING;
                    __leave;
                }
                MainResourceAcquired = TRUE;

            } else {

                if (!ExAcquireResourceSharedLite(
                            &Fcb->MainResource,
                            IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_WAIT) )) {
                    Status = STATUS_PENDING;
                    __leave;
                }
                MainResourceAcquired = TRUE;

                if ((ByteOffset.QuadPart + (LONGLONG)Length) > Fcb->Header.ValidDataLength.QuadPart) {
                    if (ByteOffset.QuadPart >= Fcb->Header.ValidDataLength.QuadPart) {
                        IrpContext->Data->IoStatus.Information = 0;
                        Status = STATUS_END_OF_FILE;
                        __leave;
                    }
                    ReturnedLength = (ULONG)(Fcb->Header.ValidDataLength.QuadPart - ByteOffset.QuadPart);
                }
            }

            if (!FltCheckLockForReadAccess(
                        &Fcb->FileLockAnchor,
                        IrpContext->Data         )) {
                Status = STATUS_FILE_LOCK_CONFLICT;
                __leave;
            }
        }

        if (Ccb != NULL) {

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
        }

        if (!Nocache) {

            {
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
            }

            if (FlagOn(Iopb->MinorFunction, IRP_MN_MDL)) {
                CcMdlRead(
                    CacheObject,
                    (&ByteOffset),
                    ReturnedLength,
                    &Iopb->Parameters.Read.MdlAddress,
                    &IrpContext->Data->IoStatus );

                Status = IrpContext->Data->IoStatus.Status;

            } else {

                Buffer = SifsGetUserBufferOnRead(Iopb);

                if (Buffer == NULL) {
                    Status = STATUS_INVALID_USER_BUFFER;
                    DbgBreak();
                    __leave;
                }

                if (!CcCopyRead(
                            CacheObject,
                            (PLARGE_INTEGER)&ByteOffset,
                            ReturnedLength,
                            IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT),
                            Buffer,
                            &IrpContext->Data->IoStatus )) {
                    Status = STATUS_PENDING;
                    DbgBreak();
                    __leave;
                }

                Status = IrpContext->Data->IoStatus.Status;
            }

        } else {

            ULONG   VDLOffset, BytesRead = ReturnedLength;
            PVOID   SystemVA = SifsGetUserBufferOnRead(Iopb);
            BOOLEAN ZeroByte = FALSE;

            if (!PagingIo && ByteOffset.QuadPart + BytesRead > Fcb->Header.ValidDataLength.QuadPart) {

                ReturnedLength = (ULONG)(Fcb->Header.ValidDataLength.QuadPart - ByteOffset.QuadPart);
                if (ByteOffset.QuadPart < Fcb->Header.ValidDataLength.QuadPart) {

                    if (!IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT)) {
                        Status = STATUS_PENDING;
                        __leave;
                    }

                    VDLOffset = ReturnedLength;
                    ZeroByte = TRUE;

                } else {

                    if (SystemVA)
                        SafeZeroMemory(SystemVA, Length);

                    IrpContext->Data->IoStatus.Information = Length;
                    Status = STATUS_SUCCESS;
                    __leave;
                }

                BytesRead = (ReturnedLength + Vcb->SectorSize- 1) & (~(Vcb->SectorSize- 1));

                if (BytesRead > ReturnedLength ) {
                    if (!IsFlagOn(Iopb->IrpFlags, IRP_CONTEXT_FLAG_WAIT)) {
                        Status = STATUS_PENDING;
                        __leave;
                    }
                }
            }

            Status = SifsLockUserBuffer(
                         IrpContext->Data,
                         BytesRead,
                         IoReadAccess );

            if (!NT_SUCCESS(Status)) {
                __leave;
            }

            Status = FltReadFile(
                         IrpContext->FltObjects->Instance,
                         Fcb->Lower->FileObject,
                         &ByteOffset,
                         BytesRead,
                         SifsGetUserBufferOnRead(Iopb),
                         0,/* FLTFL_IO_OPERATION_NON_CACHED | FLTFL_IO_OPERATION_DO_NOT_UPDATE_BYTE_OFFSET */
                         &ReturnedLength, 
                         NULL,
                         NULL );

            /* we need re-queue this request in case STATUS_CANT_WAIT
               and fail it in other failure cases  */
            if (!NT_SUCCESS(Status)) {
                __leave;
            }

            /* pended by low level device */
            if (Status == STATUS_PENDING) {
                IrpContext->Data = NULL;
		  Iopb = NULL;
                __leave;
            }

            Status = IrpContext->Data->IoStatus.Status;

            if (!NT_SUCCESS(Status)) {
                SifsNormalizeAndRaiseStatus(IrpContext, Status);
            }
            ASSERT(ReturnedLength <= Length);
            if (SystemVA && ReturnedLength < Length) {
                SafeZeroMemory((PUCHAR)SystemVA + ReturnedLength,
                               Length - ReturnedLength);
                if (ZeroByte && VDLOffset < ReturnedLength) {
                    SafeZeroMemory((PUCHAR)SystemVA + VDLOffset,
                                   ReturnedLength - VDLOffset);
                }
            }
        }

        IrpContext->Data->IoStatus.Information = ReturnedLength;

    } __finally {

        if (Iopb) {
            if (PagingIoResourceAcquired) {
                ExReleaseResourceLite(&Fcb->PagingIoResource);
            }

            if (MainResourceAcquired) {
                ExReleaseResourceLite(&Fcb->MainResource);
            }
        }

        if (!OpPostIrp && !IrpContext->ExceptionInProgress) {

            if (Iopb) {
                if ( Status == STATUS_PENDING ||
                        Status == STATUS_CANT_WAIT) {

                    Status = SifsLockUserBuffer(
                                 IrpContext->Data,
                                 Length,
                                 IoWriteAccess );

                    if (NT_SUCCESS(Status)) {
                        Status = SifsQueueRequest(IrpContext);
                    } else {
                        SifsCompleteIrpContext(IrpContext, Status);
                    }
                } else {
                    if (NT_SUCCESS(Status)) {
                        if (!PagingIo) {
                            if (SynchronousIo) {
                                FileObject->CurrentByteOffset.QuadPart =
                                    ByteOffset.QuadPart + IrpContext->Data->IoStatus.Information;
                            }
                            FileObject->Flags |= FO_FILE_FAST_IO_READ;
                        }
                    }

                    SifsCompleteIrpContext(IrpContext, Status);
                }

            } else {

                SifsFreeIrpContext(IrpContext);
            }
        }
    }

    LOG_PRINT(LOGFL_READ, ("FileFlt!SifsReadFile: %wZ fetch at Off=%I64xh Len=%xh Paging=%xh Nocache=%xh Returned=%xh Status=%xh\n",
                  &Fcb->Mcb->ShortName, ByteOffset.QuadPart, Length, PagingIo, Nocache, ReturnedLength, Status));
	
    return Status;

}


NTSTATUS
SifsReadComplete (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;
    PFILE_OBJECT    FileObject;
    PFLT_IO_PARAMETER_BLOCK Iopb = IrpContext->Data->Iopb;

    __try {

        ASSERT(IrpContext);
        ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
               (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

        FileObject = IrpContext->FileObject;

        CcMdlReadComplete(FileObject, Iopb->Parameters.Read.MdlAddress);
        Iopb->Parameters.Read.MdlAddress = NULL;
        Status = STATUS_SUCCESS;

    } __finally {

        if (!IrpContext->ExceptionInProgress) {
            SifsCompleteIrpContext(IrpContext, Status);
        }
    }

    return Status;
}


FLT_PREOP_CALLBACK_STATUS
SifsCommonRead (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE; 
    PFLT_CALLBACK_DATA      Data = IrpContext->Data;
    PVOLUME_CONTEXT           Vcb = IrpContext->VolumeContext;
    
    NTSTATUS            Status = STATUS_SUCCESS;
    PSIFS_FCBVCB     FcbOrVcb;
    PFILE_OBJECT      FileObject;
    BOOLEAN             bCompleteRequest;

    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    __try {

        if (FlagOn(IrpContext->Data->Iopb->MinorFunction, IRP_MN_COMPLETE)) {

            Status =  SifsReadComplete(IrpContext);
            bCompleteRequest = FALSE;

        } else {

            if (IsFlagOn(Vcb->Flags, VCB_DISMOUNT_PENDING)) {

                Status = STATUS_TOO_LATE;
                bCompleteRequest = TRUE;
                __leave;
            }

            FileObject = IrpContext->FileObject;

            FcbOrVcb = (PSIFS_FCBVCB) FileObject->FsContext;

            if (FcbOrVcb->Identifier.Type == SIFSFCB) {
                Status = SifsReadFile(IrpContext);
                bCompleteRequest = FALSE;
            } else {
                DbgBreak();

                Status = STATUS_INVALID_PARAMETER;
                bCompleteRequest = TRUE;
            }
        }

    } __finally {

	 Data->IoStatus.Status = Status;
	 
        if (bCompleteRequest) {
            SifsCompleteIrpContext(IrpContext, Status);
        }

	 if(Status == STATUS_PENDING) {

		retValue = FLT_PREOP_PENDING;
	}
    }

    return retValue;
}

