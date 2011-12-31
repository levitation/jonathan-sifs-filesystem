#include "fileflt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsAcquireForLazyWrite)
#pragma alloc_text(PAGE, SifsReleaseFromLazyWrite)
#pragma alloc_text(PAGE, SifsAcquireForReadAhead)
#pragma alloc_text(PAGE, SifsReleaseFromReadAhead)
#pragma alloc_text(PAGE, SifsNoOpAcquire)
#pragma alloc_text(PAGE, SifsNoOpRelease)
#pragma alloc_text(PAGE, SifsAcquireForCreateSection)
#pragma alloc_text(PAGE, SifsReleaseForCreateSection)
#pragma alloc_text(PAGE, SifsAcquireFileForModWrite)
#pragma alloc_text(PAGE, SifsReleaseFileForModWrite)
#pragma alloc_text(PAGE, SifsAcquireFileForCcFlush)
#pragma alloc_text(PAGE, SifsReleaseFileForCcFlush)
#endif

#define CMCB_DEBUG_LEVEL DL_NVR

BOOLEAN
SifsAcquireForLazyWrite (
    __in PVOID    Context,
    __in BOOLEAN  Wait
    )
{
    //
    // On a readonly filesystem this function still has to exist but it
    // doesn't need to do anything.

    PSIFS_FCB    Fcb;

    Fcb = (PSIFS_FCB) Context;
    ASSERT(Fcb != NULL);
    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));
	
    if (!ExAcquireResourceSharedLite(
                &Fcb->PagingIoResource, Wait)) {
        return FALSE;
    }

    ASSERT(Fcb->LazyWriterThread == NULL);
    Fcb->LazyWriterThread = PsGetCurrentThread();

    ASSERT(IoGetTopLevelIrp() == NULL);

    IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

    return TRUE;
}

VOID
SifsReleaseFromLazyWrite (
	__in PVOID Context
	)
{
    //
    // On a readonly filesystem this function still has to exist but it
    // doesn't need to do anything.
    PSIFS_FCB Fcb;

    Fcb = (PSIFS_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

    ASSERT(Fcb->LazyWriterThread == PsGetCurrentThread());
    Fcb->LazyWriterThread = NULL;

    ExReleaseResourceLite(&Fcb->PagingIoResource);

    ASSERT(IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    IoSetTopLevelIrp( NULL );
}

BOOLEAN
SifsAcquireForReadAhead (
	__in PVOID    Context,
       __in BOOLEAN  Wait
       )
{
    PSIFS_FCB    Fcb;

    Fcb = (PSIFS_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

    if (!ExAcquireResourceSharedLite(
                &Fcb->MainResource, Wait  ))
        return FALSE;

    ASSERT(IoGetTopLevelIrp() == NULL);

    IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);

    return TRUE;
}

VOID
SifsReleaseFromReadAhead (
	__in PVOID Context
	)
{
    PSIFS_FCB Fcb;

    Fcb = (PSIFS_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

    IoSetTopLevelIrp( NULL );

    ExReleaseResourceLite(&Fcb->MainResource);
}


BOOLEAN
SifsNoOpAcquire (
    __in PVOID Fcb,
    __in BOOLEAN Wait
	)
{
    ASSERT(IoGetTopLevelIrp() == NULL);
    IoSetTopLevelIrp((PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    return TRUE;
}

VOID
SifsNoOpRelease (
    __in PVOID Fcb
	)
{
    ASSERT(IoGetTopLevelIrp() == (PIRP)FSRTL_CACHE_TOP_LEVEL_IRP);
    IoSetTopLevelIrp( NULL );

    return;
}

VOID
SifsAcquireForCreateSection (
    __in PFILE_OBJECT FileObject
	)
{
    PSIFS_FCB Fcb = FileObject->FsContext;

    if (Fcb->Header.PagingIoResource != NULL) {
        ExAcquireResourceExclusiveLite(Fcb->Header.PagingIoResource, TRUE);
    }
}

VOID
SifsReleaseForCreateSection (
    __in PFILE_OBJECT FileObject	
    )
{
    PSIFS_FCB Fcb = FileObject->FsContext;

    if (Fcb->Header.PagingIoResource != NULL) {
        ExReleaseResourceLite(Fcb->Header.PagingIoResource);
    }
}


NTSTATUS
SifsAcquireFileForModWrite (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER EndingOffset,
    __out PERESOURCE *ResourceToRelease,
    __in PDEVICE_OBJECT DeviceObject
	)
{
    BOOLEAN ResourceAcquired = FALSE;

    PSIFS_FCB Fcb = FileObject->FsContext;

    if (Fcb->Header.PagingIoResource != NULL) {
        *ResourceToRelease = Fcb->Header.PagingIoResource;
    } else {
        *ResourceToRelease = Fcb->Header.Resource;
    }

    ResourceAcquired = ExAcquireResourceSharedLite(*ResourceToRelease, FALSE);
    if (!ResourceAcquired) {
        *ResourceToRelease = NULL;
    }

    return (ResourceAcquired ? STATUS_SUCCESS : STATUS_CANT_WAIT);
}

NTSTATUS
SifsReleaseFileForModWrite (
    __in PFILE_OBJECT FileObject,
    __in PERESOURCE ResourceToRelease,
    __in PDEVICE_OBJECT DeviceObject
)
{
    PSIFS_FCB Fcb = FileObject->FsContext;

    if (ResourceToRelease != NULL) {
        ASSERT(ResourceToRelease == Fcb->Header.PagingIoResource ||
               ResourceToRelease == Fcb->Header.Resource);
        ExReleaseResourceLite(ResourceToRelease);
    } else {
        DbgBreak();
    }

    return STATUS_SUCCESS;
}

NTSTATUS
SifsAcquireFileForCcFlush (
    __in PFILE_OBJECT FileObject,
    __in PDEVICE_OBJECT DeviceObject
)
{
    PSIFS_FCB Fcb = FileObject->FsContext;

    if (Fcb->Header.PagingIoResource != NULL) {
        ExAcquireResourceSharedLite(Fcb->Header.PagingIoResource, TRUE);
    }

    return STATUS_SUCCESS;
}

NTSTATUS
SifsReleaseFileForCcFlush (
    __in PFILE_OBJECT FileObject,
    __in PDEVICE_OBJECT DeviceObject
)
{
    PSIFS_FCB Fcb = FileObject->FsContext;

    if (Fcb->Header.PagingIoResource != NULL) {
        ExReleaseResourceLite(Fcb->Header.PagingIoResource);
    }

    return STATUS_SUCCESS;
}


