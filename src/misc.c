#include "fileflt.h"

ULONG
SifsValidateFileSize(
	__in PSTREAM_CONTEXT StreamContext
	)
{
	ULONG rc = 0;

	if(StreamContext->CryptedFile == TRUE) {
		
		rc = StreamContext->FileSize.QuadPart - StreamContext->CryptContext.MetadataSize;
	}else{

		rc = StreamContext->FileSize.QuadPart;
	}

	return rc;
}

FLT_TASK_STATE
SifsGetTaskStateInPreCreate(
	__inout PFLT_CALLBACK_DATA Data
	)
{
	FLT_TASK_STATE 		taskState = FLT_TASK_STATE_UNKNOWN;

	TaskGetState(PsGetCurrentProcessId(), &taskState);

	return taskState;
}

int
SifsCheckPreCreatePassthru_1(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects
    )
{
	int rc = 0;

	if(FltObjects->FileObject->FileName.Length == 0) {
		
		goto SifsCheckPreCreatePassthru_1Cleanup;
	}

	if (FlagOn( Data->Iopb->OperationFlags, SL_OPEN_PAGING_FILE )) {
		
		goto SifsCheckPreCreatePassthru_1Cleanup;
	}
	
	if(FlagOn( Data->Iopb->OperationFlags, SL_OPEN_TARGET_DIRECTORY)){
		
		goto SifsCheckPreCreatePassthru_1Cleanup;
	}
	
	if (FlagOn( Data->Iopb->TargetFileObject->Flags, FO_VOLUME_OPEN )) { 
	
		goto SifsCheckPreCreatePassthru_1Cleanup;
	}   

	if(FlagOn( Data->Iopb->Parameters.Create.Options, FILE_DIRECTORY_FILE)) {
		
		goto SifsCheckPreCreatePassthru_1Cleanup;
       }

	rc = -1;
	
SifsCheckPreCreatePassthru_1Cleanup:
	

	return rc;
}

int
SifsCheckPreCreatePassthru_2(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOLUME_CONTEXT VolumeContext,
    __in PFLT_FILE_NAME_INFORMATION NameInfo
    )
{
	int rc = 0;
	UNICODE_STRING pattern;

	RtlInitUnicodeString(&pattern, L"txt");

	  // 测试条件: 仅对\\Device\\HarddiskVolume3 加密 LanmanRedirector
       if((NameInfo->Volume.Length == 0)
		|| ((RtlCompareMemory(NameInfo->Volume.Buffer, L"\\Device\\HarddiskVolume3", 46) != 46)
		&& (RtlCompareMemory(NameInfo->Volume.Buffer, L"\\Device\\LanmanRedirector", 48) != 48))){

		goto SifsCheckPreCreatePassthru_2_Cleanup;

	}
	   
	if((NameInfo->Extension.Length == 6)
		&& (RtlCompareUnicodeString (&NameInfo->Extension,  &pattern, TRUE) == 0)) {
		
		rc = -1;
	}

SifsCheckPreCreatePassthru_2_Cleanup:
	
	return rc;
}

BOOLEAN
SifsIsNameValid(
	__in PUNICODE_STRING FileName
	)
{
    USHORT  i = 0;
    PUSHORT pName = (PUSHORT) FileName->Buffer;

    if (FileName == NULL) {
        return FALSE;
    }

    while (i < (FileName->Length / sizeof(WCHAR))) {

        if (pName[i] == 0) {
            break;
        }

        if (pName[i] == L'|'  || pName[i] == L':'  ||
                pName[i] == L'/'  || pName[i] == L'*'  ||
                pName[i] == L'?'  || pName[i] == L'\"' ||
                pName[i] == L'<'  || pName[i] == L'>'   ) {

            return FALSE;
        }

        i++;
    }

    return TRUE;
}


NTSTATUS
SifsSearchMcb_i(
	__in PLIST_ENTRY Head,
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName,
	__out PSIFS_MCB *Mcb
	)
{
	PLIST_ENTRY entry = NULL;
	NTSTATUS status = STATUS_OBJECT_NAME_NOT_FOUND;

	for( entry = Head->Flink; entry != Head; entry = entry->Flink) {

		PSIFS_MCB context = CONTAINING_RECORD(entry, SIFS_MCB, Next);

		if((context->ShortName.Length == LastFileName->Length)
			&& (context->FullName.Length == FileName->Length)
			&& (!RtlCompareUnicodeString(&context->ShortName, LastFileName, TRUE))
			&& (!RtlCompareUnicodeString(&context->FullName, FileName, TRUE))){

			if(Mcb) {

				SifsReferMcb(context);				
				*Mcb = context;
			}

			status = STATUS_SUCCESS;
			break;
		}
	}	

	return status;
}


NTSTATUS
SifsLookupFile(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PVOLUME_CONTEXT VolumeContext, 
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName, 
	__out PSIFS_MCB *Mcb
	)
{
	NTSTATUS status = STATUS_OBJECT_NAME_NOT_FOUND;
	BOOLEAN   LockAcquired = FALSE;

	__try{

		ExAcquireResourceSharedLite(&VolumeContext->McbLock, TRUE);
        	LockAcquired = TRUE;
        	status = SifsSearchMcb_i(&(VolumeContext->McbList), FileName, LastFileName, Mcb);
			
	}__finally{

		if (LockAcquired) {
            		ExReleaseResourceLite(&VolumeContext->McbLock);
        	}
	}

	return status;
}

static NTSTATUS
SifsGetLowerFileAttributes(
	__in PFLT_INSTANCE Instance,
	__out PSIFS_MCB Mcb
	)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	FILE_BASIC_INFORMATION  fileBasicInformation;
	FILE_STANDARD_INFORMATION fileStandardInformation;
	FILE_VALID_DATA_LENGTH_INFORMATION fileValidDataLengthInformation;

	status = FltQueryInformationFile(Instance,
									 Mcb->Lower.FileObject,
									 &fileBasicInformation,
									 sizeof(fileBasicInformation),
									 FileBasicInformation,
									 NULL
									 ) ;

	if(!NT_SUCCESS(status)) {

		goto SifsGetLowerFileAttributesCleanup;
	}
	
	status = FltQueryInformationFile(Instance,
									 Mcb->Lower.FileObject,
									 &fileStandardInformation,
									 sizeof(fileStandardInformation),
									 FileStandardInformation,
									 NULL
									 ) ;

	if(!NT_SUCCESS(status)) {

		goto SifsGetLowerFileAttributesCleanup;
	}

	status = FltQueryInformationFile(Instance,
									 Mcb->Lower.FileObject,
									 &fileValidDataLengthInformation,
									 sizeof(fileValidDataLengthInformation),
									 FileValidDataLengthInformation,
									 NULL
									 ) ;

	if(!NT_SUCCESS(status)) {

		goto SifsGetLowerFileAttributesCleanup;
	}

	Mcb->Lower.CreationTime = fileBasicInformation.CreationTime;
	Mcb->Lower.LastWriteTime = fileBasicInformation.LastWriteTime;
	Mcb->Lower.LastAccessTime = fileBasicInformation.LastAccessTime;
	Mcb->Lower.ChangeTime = fileBasicInformation.ChangeTime;
	Mcb->Lower.FileAttributes = fileBasicInformation.FileAttributes;
	Mcb->Lower.AllocationSize = fileStandardInformation.AllocationSize;
	Mcb->Lower.FileSize = fileStandardInformation.EndOfFile;
	Mcb->Lower.Directory = fileStandardInformation.Directory;
	Mcb->Lower.ValidDataLength = fileValidDataLengthInformation.ValidDataLength;

SifsGetLowerFileAttributesCleanup:
	
	return status;
}

NTSTATUS
SifsCreateUnderlyingFile(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PVOLUME_CONTEXT VolumeContext, 
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName, 
	__out PSIFS_MCB *Mcb
	)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PSIFS_MCB mcb = NULL;
	
	mcb = SifsAllocateMcb(FileName, LastFileName, 0);

	if(mcb == NULL) {

		status = STATUS_INSUFFICIENT_RESOURCES;
		goto SifsCreateUnderlyingFileCleanup;
	}	

       status = FsCreateFile(IrpContext->FltObjects->Instance, GENERIC_READ|GENERIC_WRITE
	   		, FILE_CREATE, FILE_NON_DIRECTORY_FILE, FILE_SHARE_READ|FILE_SHARE_WRITE
	   		, FILE_ATTRIBUTE_NORMAL, FileName, &(mcb->Lower.FileHandle), &(mcb->Lower.FileObject));

	if(!NT_SUCCESS(status)){

		goto SifsCreateUnderlyingFileCleanup;
	}

	status = SifsGetLowerFileAttributes(IrpContext->FltObjects->Instance, mcb);

	if(!NT_SUCCESS(status)) {

		goto SifsCreateUnderlyingFileCleanup;
	}

	if(Mcb) {

		SifsReferMcb(mcb);
		
		*Mcb = mcb;
	}
	   
SifsCreateUnderlyingFileCleanup:

	if(!NT_SUCCESS(status)) {

		if(mcb->Lower.FileObject) {

			FsCloseFile(mcb->Lower.FileHandle, mcb->Lower.FileObject);
		}
		
		if(mcb) {

			SifsDerefMcb(mcb);
		}
	}
	
	return status;
}

NTSTATUS
SifsOpenUnderlyingFile(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PVOLUME_CONTEXT VolumeContext, 
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName, 
	__out PSIFS_MCB *Mcb
	)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	PSIFS_MCB mcb = NULL;
	
	mcb = SifsAllocateMcb(FileName, LastFileName, 0);

	if(mcb == NULL) {

		status = STATUS_INSUFFICIENT_RESOURCES;
		goto SifsOpenUnderlyingFileCleanup;
	}	

       status = FsCreateFile(IrpContext->FltObjects->Instance, GENERIC_READ|GENERIC_WRITE
	   		, FILE_OPEN, FILE_NON_DIRECTORY_FILE, FILE_SHARE_READ|FILE_SHARE_WRITE
	   		, FILE_ATTRIBUTE_NORMAL, FileName, &(mcb->Lower.FileHandle), &(mcb->Lower.FileObject));

	if(!NT_SUCCESS(status)){

		goto SifsOpenUnderlyingFileCleanup;
	}

	status = SifsGetLowerFileAttributes(IrpContext->FltObjects->Instance, mcb);

	if(!NT_SUCCESS(status)) {

		goto SifsOpenUnderlyingFileCleanup;
	}

	if(Mcb) {

		SifsReferMcb(mcb);
		
		*Mcb = mcb;
	}
	   
SifsOpenUnderlyingFileCleanup:

	if(!NT_SUCCESS(status)) {

		if(mcb->Lower.FileObject) {

			FsCloseFile(mcb->Lower.FileHandle, mcb->Lower.FileObject);
		}
		
		if(mcb) {

			SifsDerefMcb(mcb);
		}
	}
	

	return status;
}

BOOLEAN 
SifsCheckFcbTypeIsSifs(
	__in PFILE_OBJECT FileObject
	)
{
	BOOLEAN rc = FALSE;
	
	if(FileObject){
		
		if(FileObject->FsContext){
			
			PFSRTL_COMMON_FCB_HEADER fcbHead = (PFSRTL_COMMON_FCB_HEADER)FileObject->FsContext;
			
			if(fcbHead->NodeTypeCode == SIFSFCB){
				
				rc  = TRUE;
			}
		}
	}
	
	return rc;
}

