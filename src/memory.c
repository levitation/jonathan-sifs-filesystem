#include "fileflt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsAllocateIrpContext)
#pragma alloc_text(PAGE, SifsFreeIrpContext)
#endif



PSIFS_IRP_CONTEXT
SifsAllocateIrpContext (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PSIFS_PARAMETERS SifsParameters
    )
{
    PSIFS_IRP_CONTEXT    IrpContext;

    IrpContext = (PSIFS_IRP_CONTEXT) (
                     ExAllocateFromNPagedLookasideList(
                         &(g_FileFltContext.SifsIrpContextLookasideList)));

    if (IrpContext == NULL) {
        return NULL;
    }

    RtlZeroMemory(IrpContext, sizeof(SIFS_IRP_CONTEXT) );    

    IrpContext->Identifier.Type = SIFSICX;
    IrpContext->Identifier.Size = sizeof(SIFS_IRP_CONTEXT);

    IrpContext->Data = Data;
    IrpContext->FltObjects = FltObjects;
    RtlCopyMemory(&(IrpContext->Parameters), SifsParameters, sizeof(IrpContext->Parameters));

    IrpContext->MajorFunction = Data->Iopb->MajorFunction;
    IrpContext->MinorFunction = Data->Iopb->MinorFunction;
    IrpContext->FileObject = FltObjects->FileObject;
    if (NULL != IrpContext->FileObject) {
        IrpContext->Fcb = (PSIFS_FCB)IrpContext->FileObject->FsContext;
        IrpContext->Ccb = (PSIFS_CCB)IrpContext->FileObject->FsContext2;
    }    

    if ( IrpContext->MajorFunction == IRP_MJ_CLEANUP ||
            IrpContext->MajorFunction == IRP_MJ_CLOSE ||
            IrpContext->MajorFunction == IRP_MJ_SHUTDOWN ||
            IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL ||
            IrpContext->MajorFunction == IRP_MJ_PNP ) {

        if ( IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL ||
                IrpContext->MajorFunction == IRP_MJ_PNP) {
            if (FltObjects->FileObject == NULL ||
                    FltIsOperationSynchronous(Data)) {
                SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
            }
        } else {
            SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
        }

    } else if (FltIsOperationSynchronous(Data)) {

        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    IrpContext->IsTopLevel = (IoGetTopLevelIrp() == Data);
    IrpContext->ExceptionInProgress = FALSE;

    return IrpContext;
}

VOID
SifsFreeIrpContext (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    ASSERT(IrpContext != NULL);

    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    /* free the IrpContext to NonPagedList */
    IrpContext->Identifier.Type = 0;
    IrpContext->Identifier.Size = 0;

    if(IrpContext->MajorFunction == IRP_MJ_CREATE) {

	 if (IrpContext->Parameters.Create.NameInfo != NULL) {

		FltReleaseFileNameInformation(IrpContext->Parameters.Create.NameInfo);
	 }
    }

    if(IrpContext->VolumeContext != NULL) {

	FltReleaseContext(IrpContext->VolumeContext);
    }
	
    ExFreeToNPagedLookasideList(&(g_FileFltContext.SifsIrpContextLookasideList), IrpContext);
	
}

PSIFS_FCB
SifsAllocateFcb (
    __in PSIFS_MCB   Mcb
	)
{
    PSIFS_FCB Fcb;

    Fcb = (PSIFS_FCB) ExAllocateFromNPagedLookasideList(
              &(g_FileFltContext.SifsFcbLookasideList));

    if (!Fcb) {
        return NULL;
    }

    RtlZeroMemory(Fcb, sizeof(SIFS_FCB));
    Fcb->Identifier.Type = SIFSFCB;
    Fcb->Identifier.Size = sizeof(SIFS_FCB);

    ExInitializeFastMutex(&Fcb->Mutex);
    FsRtlSetupAdvancedHeader(&Fcb->Header,  &Fcb->Mutex);

    FltInitializeOplock(&Fcb->Oplock);
 //   FsRtlInitializeOplock(&Fcb->Oplock);
    FsRtlInitializeFileLock (
        &Fcb->FileLockAnchor,
        NULL,
        NULL );

    Fcb->OpenHandleCount = 0;
    Fcb->ReferenceCount = 0;
    Fcb->Lower = &(Mcb->Lower);

    ASSERT(Mcb->Fcb == NULL);
    SifsReferMcb(Mcb);
    Fcb->Mcb = Mcb;
    Mcb->Fcb = Fcb;

    RtlZeroMemory(&Fcb->Header, sizeof(FSRTL_COMMON_FCB_HEADER));
    Fcb->Header.NodeTypeCode = (USHORT) SIFSFCB;
    Fcb->Header.NodeByteSize = sizeof(SIFS_FCB);
    Fcb->Header.IsFastIoPossible = FastIoIsNotPossible;
    Fcb->Header.Resource = &(Fcb->MainResource);
    Fcb->Header.PagingIoResource = &(Fcb->PagingIoResource);

    Fcb->Header.FileSize = Mcb->Lower.FileSize;
    Fcb->Header.ValidDataLength = Mcb->Lower.ValidDataLength;
    Fcb->Header.AllocationSize = Mcb->Lower.AllocationSize;

    Fcb->SectionObject.DataSectionObject = NULL;
    Fcb->SectionObject.SharedCacheMap = NULL;
    Fcb->SectionObject.ImageSectionObject = NULL;

    ExInitializeResourceLite(&(Fcb->MainResource));
    ExInitializeResourceLite(&(Fcb->PagingIoResource));

    return Fcb;
}

VOID
SifsFreeFcb (
	__in PSIFS_FCB Fcb
	)
{
    ASSERT((Fcb != NULL) && (Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));
    ASSERT((Fcb->Mcb->Identifier.Type == SIFSMCB) &&
           (Fcb->Mcb->Identifier.Size == sizeof(SIFS_MCB)));

    FsRtlTeardownPerStreamContexts(&Fcb->Header);

    if ((Fcb->Mcb->Identifier.Type == SIFSMCB) &&
            (Fcb->Mcb->Identifier.Size == sizeof(SIFS_MCB))) {

        ASSERT (Fcb->Mcb->Fcb == Fcb);
		
        Fcb->Mcb->Fcb = NULL;
        SifsDerefMcb(Fcb->Mcb);

    } else {
        DbgBreak();
    }

    FsRtlUninitializeFileLock(&Fcb->FileLockAnchor);
 //   FsRtlUninitializeOplock(&Fcb->Oplock);
    FltUninitializeOplock(&Fcb->Oplock);
    ExDeleteResourceLite(&Fcb->MainResource);
    ExDeleteResourceLite(&Fcb->PagingIoResource);

    Fcb->Identifier.Type = 0;
    Fcb->Identifier.Size = 0;

    ExFreeToNPagedLookasideList(&(g_FileFltContext.SifsFcbLookasideList), Fcb);
}

PSIFS_CCB
SifsAllocateCcb (
	VOID
	)
{
    PSIFS_CCB Ccb;

    Ccb = (PSIFS_CCB) (ExAllocateFromPagedLookasideList(
                           &(g_FileFltContext.SifsCcbLookasideList)));
    if (!Ccb) {
        return NULL;
    }

    RtlZeroMemory(Ccb, sizeof(SIFS_CCB));

    Ccb->Identifier.Type = SIFSCCB;
    Ccb->Identifier.Size = sizeof(SIFS_CCB);

    Ccb->DirectorySearchPattern.Length = 0;
    Ccb->DirectorySearchPattern.MaximumLength = 0;
    Ccb->DirectorySearchPattern.Buffer = 0;

    return Ccb;
}

VOID
SifsFreeCcb (
	__in PSIFS_CCB Ccb
	)
{
    ASSERT(Ccb != NULL);

    ASSERT((Ccb->Identifier.Type == SIFSCCB) &&
           (Ccb->Identifier.Size == sizeof(SIFS_CCB)));

    if (Ccb->DirectorySearchPattern.Buffer != NULL) {
		
        FsFreeUnicodeString(&(Ccb->DirectorySearchPattern));
    }

    ExFreeToPagedLookasideList(&(g_FileFltContext.SifsCcbLookasideList), Ccb);
}

BOOLEAN
SifsBuildName(
	__in PUNICODE_STRING Source,
	__in PUNICODE_STRING Dest
	)
{
	BOOLEAN rc = FALSE;

	Dest->MaximumLength = Source->Length + sizeof(WCHAR);
	
	if(NT_SUCCESS(FsAllocateUnicodeString(Dest))) {

		RtlCopyMemory(Dest->Buffer, Source->Buffer, Source->Length);

		rc = TRUE;
	}

	return rc;
}

PSIFS_MCB
SifsAllocateMcb (
    __in PUNICODE_STRING  FileName,
    __in PUNICODE_STRING  ShortName,
    __in ULONG            FileAttr
	)
{
    PSIFS_MCB   Mcb = NULL;
    NTSTATUS    Status = STATUS_SUCCESS;

    /* allocate Mcb from LookasideList */
    Mcb = (PSIFS_MCB) (ExAllocateFromPagedLookasideList(
                           &(g_FileFltContext.SifsMcbLookasideList)));

    if (Mcb == NULL) {
        return NULL;
    }

    /* initialize Mcb header */
    RtlZeroMemory(Mcb, sizeof(SIFS_MCB));
    Mcb->Identifier.Type = SIFSMCB;
    Mcb->Identifier.Size = sizeof(SIFS_MCB);
    Mcb->FileAttr = FileAttr;

    /* initialize Mcb names */
    if (FileName) {

        if (!SifsBuildName(&Mcb->ShortName, ShortName)) {
            goto errorout;
        }
        if (!SifsBuildName(&Mcb->FullName, FileName)) {
            goto errorout;
        }
    }       
	
    return Mcb;

errorout:

    if (Mcb) {

        if (Mcb->ShortName.Buffer) {
			
	     FsFreeUnicodeString(&Mcb->ShortName);
        }

        if (Mcb->FullName.Buffer) {
			
            FsFreeUnicodeString(&Mcb->FullName.Buffer);
        }

        ExFreeToPagedLookasideList(&(g_FileFltContext.SifsMcbLookasideList), Mcb);
    }

    return NULL;
}

VOID
SifsFreeMcb (
	__in PSIFS_MCB Mcb
	)
{
    ASSERT(Mcb != NULL);

    ASSERT((Mcb->Identifier.Type == SIFSMCB) &&
           (Mcb->Identifier.Size == sizeof(SIFS_MCB)));

    if ((Mcb->Identifier.Type != SIFSMCB) ||
            (Mcb->Identifier.Size != sizeof(SIFS_MCB))) {
        return;
    }

    ClearLongFlag(Mcb->Flags, MCB_ZONE_INIT);

    if (Mcb->ShortName.Buffer) {
		
        FsFreeUnicodeString(&Mcb->ShortName);
    }

    if (Mcb->FullName.Buffer) {
		
        FsFreeUnicodeString(&Mcb->FullName);
    }

    Mcb->Identifier.Type = 0;
    Mcb->Identifier.Size = 0;

    ExFreeToPagedLookasideList(&(g_FileFltContext.SifsMcbLookasideList), Mcb);
}

VOID
SifsInsertMcb (
    __in PVOLUME_CONTEXT  Vcb,
    __in PSIFS_MCB Child
	)
{
    BOOLEAN     LockAcquired = FALSE;
    PSIFS_MCB   Mcb = NULL;

    __try {

        ExAcquireResourceExclusiveLite(
            &Vcb->McbLock,
            TRUE );
        LockAcquired = TRUE;

	 InsertHeadList(&(Vcb->McbList), &(Child->Next));
        SetLongFlag(Child->Flags, MCB_ENTRY_TREE);
	 SifsReferXcb(&Vcb->NumOfMcb);

    } __finally {

        if (LockAcquired) {
            ExReleaseResourceLite(&Vcb->McbLock);
        }
    }
}

BOOLEAN
SifsRemoveMcb (
    __in PVOLUME_CONTEXT  Vcb,
    __in PSIFS_MCB Mcb
	)
{
    BOOLEAN     LockAcquired = FALSE;

    __try {

        ExAcquireResourceExclusiveLite(&Vcb->McbLock, TRUE);
        LockAcquired = TRUE;

	if (IsFlagOn(Mcb->Flags, MCB_ENTRY_TREE)) {
		
             ClearLongFlag(Mcb->Flags, MCB_ENTRY_TREE);
	}

	SifsDerefXcb(&Vcb->NumOfMcb);
	RemoveEntryList(&(Mcb->Next));

    } __finally {

        if (LockAcquired) {
            ExReleaseResourceLite(&Vcb->McbLock);
        }
    }

    return TRUE;
}

static int
SifsFirstUnusedMcb(
	__in PVOLUME_CONTEXT Vcb, 
	__in BOOLEAN Wait, 
	__in ULONG Number,
	__out PLIST_ENTRY Head
	)
{
    int rc = -1;
	
    PSIFS_MCB   Mcb = NULL;
    PLIST_ENTRY List = NULL;
    ULONG       i = 0;

    if (!ExAcquireResourceExclusiveLite(&Vcb->McbLock, Wait)) {
        return rc;
    }

    while (Number--) {

        if (!IsListEmpty(&Vcb->McbList)) {

            while (i++ < Vcb->NumOfMcb) {

                List = RemoveHeadList(&Vcb->McbList);
                Mcb = CONTAINING_RECORD(List, SIFS_MCB, Next);
                ASSERT(IsFlagOn(Mcb->Flags, MCB_VCB_LINK));

                if ((Mcb->Fcb == NULL) &&
                        (Mcb->Refercount == 1) ) {

                    SifsRemoveMcb(Vcb, Mcb);
                    ClearLongFlag(Mcb->Flags, MCB_VCB_LINK);                    

                    /* attach all Mcb into a chain*/
                    InsertHeadList(Head, &(Mcb->Next));

                } else {

                    InsertTailList(&Vcb->McbList, &Mcb->Next);
                    Mcb = NULL;
                }
            }
        }

	rc = 0;
    }
    ExReleaseResourceLite(&Vcb->McbLock);

    return rc;
}


VOID
SifsCleanupAllMcbs(
	__in PVOLUME_CONTEXT Vcb
	)
{
    BOOLEAN   LockAcquired = FALSE;
    PSIFS_MCB Mcb = NULL;
    LIST_ENTRY Head;

    __try {

	 InitializeListHead(&(Head));
	 
        ExAcquireResourceExclusiveLite(
            &Vcb->McbLock,
            TRUE );
        LockAcquired = TRUE;

	while(0 == SifsFirstUnusedMcb(Vcb, TRUE, Vcb->NumOfMcb, &Head)) {

		while(!IsListEmpty(&Head)) {

			PLIST_ENTRY entry = RemoveHeadList(&Head);

			Mcb = CONTAINING_RECORD(entry, SIFS_MCB, Next);

			SifsFreeMcb(Mcb);
		}
	}
        
    } __finally {

        if (LockAcquired) {
            ExReleaseResourceLite(&Vcb->McbLock);
        }
    }
}

