#include "fileflt.h"

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

