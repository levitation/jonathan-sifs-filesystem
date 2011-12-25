#include "fileflt.h"

NTSTATUS
SifsFlushFile (
    __in PSIFS_IRP_CONTEXT    IrpContext,
    __in PSIFS_FCB            Fcb,
    __in PSIFS_CCB            Ccb
	)
{
    IO_STATUS_BLOCK    IoStatus;

    ASSERT(Fcb != NULL);
    ASSERT((Fcb->Identifier.Type == SIFSFCB) &&
           (Fcb->Identifier.Size == sizeof(SIFS_FCB)));

    /* update timestamp and achieve attribute */
    if (Ccb != NULL) {

        if (!IsFlagOn(Ccb->Flags, CCB_LAST_WRITE_UPDATED)) {

            LARGE_INTEGER   SysTime;
            KeQuerySystemTime(&SysTime);

            Fcb->Lower->ChangeTime.QuadPart = FsLinuxTime(SysTime);
            Fcb->Lower->LastWriteTime = FsNtTime(Fcb->Lower->ChangeTime.QuadPart);            
        }
    }

    CcFlushCache(&(Fcb->SectionObject), NULL, 0, &IoStatus);
    ClearFlag(Fcb->Flags, FCB_FILE_MODIFIED);

    return IoStatus.Status;
}