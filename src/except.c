#include "fileflt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsExceptionFilter)
#pragma alloc_text(PAGE, SifsExceptionHandler)
#endif


NTSTATUS
SifsExceptionFilter (
    __in PSIFS_IRP_CONTEXT    IrpContext,
    __in PEXCEPTION_POINTERS  ExceptionPointer
	)
{
    NTSTATUS Status = EXCEPTION_EXECUTE_HANDLER;
    NTSTATUS ExceptionCode;
    PEXCEPTION_RECORD ExceptRecord;

    ExceptRecord = ExceptionPointer->ExceptionRecord;
    ExceptionCode = ExceptRecord->ExceptionCode;

    DbgPrint("-------------------------------------------------------------\n");
    DbgPrint("Exception happends in SifsFsd (code %xh):\n", ExceptionCode);
    DbgPrint(".exr %p;.cxr %p;\n", ExceptionPointer->ExceptionRecord,
             ExceptionPointer->ContextRecord);
    DbgPrint("-------------------------------------------------------------\n");

    DbgBreak();

    //
    // Check IrpContext is valid or not
    //

    if (IrpContext) {
        if ((IrpContext->Identifier.Type != SIFSICX) ||
                (IrpContext->Identifier.Size != sizeof(SIFS_IRP_CONTEXT))) {
            DbgBreak();
            IrpContext = NULL;
        } 
    } else {
        if (FsRtlIsNtstatusExpected(ExceptionCode)) {
            return EXCEPTION_EXECUTE_HANDLER;
        } else {
            SifsBugCheck( SIFS_BUGCHK_EXCEPT, (ULONG_PTR)ExceptRecord,
                          (ULONG_PTR)ExceptionPointer->ContextRecord,
                          (ULONG_PTR)ExceptRecord->ExceptionAddress );
        }
    }

    if (IrpContext) {
        SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    }

    if ( Status == EXCEPTION_EXECUTE_HANDLER ||
            FsRtlIsNtstatusExpected(ExceptionCode)) {
        //
        // If the exception is expected execute our handler
        //

        LOG_PRINT(LOGFL_ERRORS, ( "Filter!SifsExceptionFilter: Catching exception %xh\n",
                        ExceptionCode));

        Status = EXCEPTION_EXECUTE_HANDLER;

        if (IrpContext) {
            IrpContext->ExceptionInProgress = TRUE;
            IrpContext->ExceptionCode = ExceptionCode;
        }

    } else  {

        //
        // Continue search for an higher level exception handler
        //

        LOG_PRINT(LOGFL_ERRORS, ( "Filter!SifsExceptionFilter: Passing on exception %#x\n",
                        ExceptionCode));

        Status = EXCEPTION_CONTINUE_SEARCH;

        if (IrpContext) {
			
            SifsFreeIrpContext(IrpContext);
        }
    }

    return Status;
}


NTSTATUS
SifsExceptionHandler (
	__in PSIFS_IRP_CONTEXT IrpContext
	)
{
    NTSTATUS Status;

    if (IrpContext) {

        if ( (IrpContext->Identifier.Type != SIFSICX) ||
                (IrpContext->Identifier.Size != sizeof(SIFS_IRP_CONTEXT))) {
            DbgBreak();
            return STATUS_UNSUCCESSFUL;
        }

        Status = IrpContext->ExceptionCode;

        if (IrpContext->Data) {

            //
            // Check if this error is a result of user actions
            //           

            /* queue it again if our request is at top level */
            if (IrpContext->IsTopLevel &&
                    ((Status == STATUS_CANT_WAIT) ||
                     ((Status == STATUS_VERIFY_REQUIRED) &&
                      (KeGetCurrentIrql() >= APC_LEVEL)))) {

                Status = SifsQueueRequest(IrpContext);
            }

            if (Status == STATUS_PENDING) {
                goto errorout;
            } 

	     SifsNormalizeAndRaiseStatus(IrpContext, Status);
        }

release_context:

        SifsFreeIrpContext(IrpContext);

    } else {

        Status = STATUS_INVALID_PARAMETER;
    }

errorout:

    return Status;
}

