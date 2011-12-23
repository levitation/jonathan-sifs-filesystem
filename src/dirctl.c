#include "fileflt.h"

VOID
SifsNotifyReportChange (
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PVOLUME_CONTEXT VolumeContext,
    __in PSIFS_MCB         Mcb,
    __in ULONG             Filter,
    __in ULONG             Action   
    )
{
    USHORT          Offset;

    Offset = (USHORT) ( Mcb->FullName.Length -
                        Mcb->ShortName.Length);

    FsRtlNotifyFullReportChange( VolumeContext->NotifySync,
                                 &(VolumeContext->NotifyList),
                                 (PSTRING) (&Mcb->FullName),
                                 (USHORT) Offset,
                                 (PSTRING)NULL,
                                 (PSTRING) NULL,
                                 (ULONG) Filter,
                                 (ULONG) Action,
                                 (PVOID) NULL );

    // ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
}

