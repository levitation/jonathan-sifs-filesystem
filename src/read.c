#include "fileflt.h"

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