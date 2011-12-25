#include "fileflt.h"

FAST_IO_POSSIBLE
SifsIsFastIoPossible(
    __in PSIFS_FCB Fcb
	)
{
    FAST_IO_POSSIBLE IsPossible = FastIoIsNotPossible;

    if (!Fcb || !FltOplockIsFastIoPossible(&Fcb->Oplock))
        return IsPossible;

    IsPossible = FastIoIsQuestionable;

    if (!FsRtlAreThereCurrentFileLocks(&Fcb->FileLockAnchor)) {
        if (/* !IsFlagOn(VolumeContext->Flags, VCB_READ_ONLY) */1) {
            IsPossible = FastIoIsPossible;
        }
    }

    return IsPossible;
}

