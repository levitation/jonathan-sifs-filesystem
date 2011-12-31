#include "fileflt.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, SifsQueueRequest)
#pragma alloc_text(PAGE, SifsDeQueueRequest)
#endif

NTSTATUS
SifsInstanceSetup(
   __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType,
    __inout PVOLUME_CONTEXT VolumeContext
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    ULONG retLen = 0;
    PUNICODE_STRING workingName = NULL;
    USHORT size = 0;
    UCHAR volPropBuffer[sizeof(FLT_VOLUME_PROPERTIES)+512];
    PFLT_VOLUME_PROPERTIES volProp = (PFLT_VOLUME_PROPERTIES)volPropBuffer;
    PDEVICE_OBJECT volumeDevice = NULL;
    BOOLEAN bVolumeWritable = FALSE;

    PAGED_CODE();

    UNREFERENCED_PARAMETER( Flags );
    UNREFERENCED_PARAMETER( VolumeDeviceType );
    UNREFERENCED_PARAMETER( VolumeFilesystemType );

    try {
        
        //
        //  Always get the volume properties, so I can get a sector size
        //

        status = FltGetVolumeProperties( FltObjects->Volume,
                                         volProp,
                                         sizeof(volPropBuffer),
                                         &retLen );

        if (!NT_SUCCESS(status)) {

            leave;
        }

        //
        //  Save the sector size in the context for later use.  Note that
        //  we will pick a minimum sector size if a sector size is not
        //  specified.
        //

        ASSERT((volProp->SectorSize == 0) || (volProp->SectorSize >= MIN_SECTOR_SIZE));

        VolumeContext->SectorSize = max(volProp->SectorSize,MIN_SECTOR_SIZE);
        
	 VolumeContext->FileSystemType = VolumeFilesystemType;
	 VolumeContext->DeviceType = VolumeDeviceType;
	 ExInitializeNPagedLookasideList( &VolumeContext->Pre2PostContextList,
                                     NULL,
                                     NULL,
                                     0,
                                     sizeof(PRE_2_POST_CONTEXT),
                                     PRE_2_POST_TAG,
                                     0 );
	 ExInitializeResourceLite(&VolumeContext->MainResource);
	 ExInitializeResourceLite(&VolumeContext->McbLock);
	 InitializeListHead(&VolumeContext->McbList);
	 InitializeListHead(&VolumeContext->NotifyList);
	 KeInitializeEvent(&VolumeContext->Reaper.Engine,
                      SynchronizationEvent, FALSE);

	 /* start resource reaper thread */
    	status= SifsStartReaperThread(VolumeContext);
    	if (NT_SUCCESS(status)) {

		LARGE_INTEGER Timeout;
		
        	/* make sure Reaperthread is started */
	      Timeout.QuadPart = (LONGLONG)-10*1000*1000*10; /* 10 seconds */
	      status = KeWaitForSingleObject(
	                 &(VolumeContext->Reaper.Engine),
	                 Executive,
	                 KernelMode,
	                 FALSE,
	                 &Timeout
	             );	   
       }       

	 RtlInitEmptyUnicodeString(&(VolumeContext->VolumeName), VolumeContext->VolumeNameBuffer, sizeof(VolumeContext->VolumeNameBuffer));
	 
	 if(volProp->RealDeviceName.Length > 0){
	 	
	 	workingName = &volProp->RealDeviceName;
	 }else{

	 	workingName = &volProp->FileSystemDeviceName;
	 }

	 RtlCopyUnicodeString(&(VolumeContext->VolumeName), workingName);

	 if(NT_SUCCESS(FltIsVolumeWritable(FltObjects->Volume, &bVolumeWritable))) {

            if(bVolumeWritable == TRUE) {

                if (IsFlagOn(volProp->DeviceCharacteristics, FILE_READ_ONLY_DEVICE)) {
                    SetLongFlag(VolumeContext->Flags, VCB_WRITE_PROTECTED);
                }
            }else{

		  SetLongFlag(VolumeContext->Flags, VCB_WRITE_PROTECTED);
            }
        }else{

            if (IsFlagOn(volProp->DeviceCharacteristics, FILE_READ_ONLY_DEVICE)) {
                SetLongFlag(VolumeContext->Flags, VCB_WRITE_PROTECTED);
            }
        }
	 
        //
        //  Log debug info
        //

     	LOG_PRINT( LOGFL_VOLCTX,
                   ("FileFlt!InstanceSetup:                  Real SectSize=0x%04x, Used SectSize=0x%04x, Name=\"%wZ\", DeviceType = 0x%x, FileSystemType = 0x%x\n",
                    volProp->SectorSize,
                    VolumeContext->SectorSize,
                    &(VolumeContext->VolumeName),
                    VolumeContext->DeviceType,
                    VolumeContext->FileSystemType)
                    );
        

    } finally {

        //
        //  Remove the reference added to the device object by
        //  FltGetDiskDeviceObject.
        //

        if (volumeDevice) {

            ObDereferenceObject( volumeDevice );
        }
    }

    return status;

}

VOID
SifsCleanupContext(
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
	PVOLUME_CONTEXT volumeContext = NULL;
	PSTREAM_CONTEXT streamContext = NULL;
	PSTREAMHANDLE_CONTEXT streamHandleContext = NULL;

	PAGED_CODE();

	UNREFERENCED_PARAMETER( ContextType );
	
	switch(ContextType){
		
	case FLT_VOLUME_CONTEXT:

		volumeContext = Context;

		SifsStopReaperThread(volumeContext);
		SifsCleanupAllMcbs(volumeContext);

		RemoveEntryList(&volumeContext->NotifyList);
		ExDeleteResourceLite(&(volumeContext->MainResource));
		ExDeleteResourceLite(&(volumeContext->McbLock));	
		
		ExDeleteNPagedLookasideList( &volumeContext->Pre2PostContextList );
		
		break;
	case FLT_STREAM_CONTEXT:
		
		streamContext = Context;

		if (streamContext->Resource != NULL) {

            		ExDeleteResourceLite( streamContext->Resource );
            		FsFreeResource( streamContext->Resource);
        	}
		if(streamContext->Lower.FileObject != NULL) {

			FsCloseFile(streamContext->Lower.FileHandle, streamContext->Lower.FileObject);
		}
		if(streamContext->Lower.Metadata != NULL) {

			ExFreePoolWithTag(streamContext->Lower.Metadata, SIFS_METADATA_TAG);
		}
		if(streamContext->NameInfo != NULL) {

			FltReleaseFileNameInformation( streamContext->NameInfo  );
		}
		
		break;
	case FLT_STREAMHANDLE_CONTEXT:

		streamHandleContext =  Context;

		 //
	        //  Delete the resource and memory the memory allocated for the resource
	        //

	        if (streamHandleContext->Resource != NULL) {

	            ExDeleteResourceLite( streamHandleContext->Resource );
	            FsFreeResource( streamHandleContext->Resource );
	        }
			
		break;
	}   
}


VOID
SifsOplockComplete (
    __in PFLT_CALLBACK_DATA CallbackData,
    __in PVOID Context
	)
{
    //
    //  Check on the return value in the Irp.
    //

    if (CallbackData->IoStatus.Status == STATUS_SUCCESS) {

        //
        //  queue the Irp context in the workqueue.
        //

        SifsQueueRequest((PSIFS_IRP_CONTEXT)Context);

    } else {

        //
        //  complete the request in case of failure
        //

        SifsCompleteIrpContext( (PSIFS_IRP_CONTEXT) Context,
                                CallbackData->IoStatus.Status );
    }
}

VOID
SifsLockIrp (
    __inout PFLT_CALLBACK_DATA Data,
    __in PVOID Context    
	)
{
    PSIFS_IRP_CONTEXT IrpContext;

    if (Data == NULL) {
        return;
    }

    IrpContext = (PSIFS_IRP_CONTEXT) Context;

    if ( IrpContext->MajorFunction == IRP_MJ_READ ||
            IrpContext->MajorFunction == IRP_MJ_WRITE ) {

        //
        //  lock the user's buffer to MDL, if the I/O is bufferred
        //

        if (!IsFlagOn(IrpContext->MinorFunction, IRP_MN_MDL)) {

            SifsLockUserBuffer( Data, (IrpContext->MajorFunction == IRP_MJ_READ) ? 
					Data->Iopb->Parameters.Read.Length : Data->Iopb->Parameters.Write.Length,
                                (IrpContext->MajorFunction == IRP_MJ_READ) ?
                                IoWriteAccess : IoReadAccess );
        }

    } else if (IrpContext->MajorFunction == IRP_MJ_DIRECTORY_CONTROL
               && IrpContext->MinorFunction == IRP_MN_QUERY_DIRECTORY) {

        ULONG Length = Data->Iopb->Parameters.DirectoryControl.QueryDirectory.Length;
        SifsLockUserBuffer(Data, Length, IoWriteAccess);

    } else if (IrpContext->MajorFunction == IRP_MJ_QUERY_EA) {

        ULONG Length = Data->Iopb->Parameters.QueryEa.Length;
        SifsLockUserBuffer(Data, Length, IoWriteAccess);

    } else if (IrpContext->MajorFunction == IRP_MJ_SET_EA) {
    
        ULONG Length = Data->Iopb->Parameters.SetEa.Length;
        SifsLockUserBuffer(Data, Length, IoReadAccess);

    } else if ( (IrpContext->MajorFunction == IRP_MJ_FILE_SYSTEM_CONTROL) &&
                (IrpContext->MinorFunction == IRP_MN_USER_FS_REQUEST) ) {
       
        if ( (Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_GET_VOLUME_BITMAP) ||
                (Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_GET_RETRIEVAL_POINTERS) ||
                (Data->Iopb->Parameters.FileSystemControl.Common.FsControlCode == FSCTL_GET_RETRIEVAL_POINTER_BASE) ) {
            ULONG Length = Data->Iopb->Parameters.FileSystemControl.Common.OutputBufferLength;
            SifsLockUserBuffer(Data, Length, IoWriteAccess);
        }
    }
}


NTSTATUS
SifsQueueRequest (
	__in  PSIFS_IRP_CONTEXT IrpContext
	)
{
    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    /* set the flags of "can wait" and "queued" */
    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT);
    SetFlag(IrpContext->Flags, IRP_CONTEXT_FLAG_REQUEUED);

    /* make sure the buffer is kept valid in system context */
    SifsLockIrp(IrpContext->Data, IrpContext);

    /* initialize workite*/
    ExInitializeWorkItem(
        &IrpContext->WorkQueueItem,
        SifsDeQueueRequest,
        IrpContext );

    /* dispatch it */
    ExQueueWorkItem(&IrpContext->WorkQueueItem, CriticalWorkQueue);

    return STATUS_PENDING;
}


VOID
SifsDeQueueRequest (
	__in PVOID Context
	)
{
    PSIFS_IRP_CONTEXT IrpContext;

    IrpContext = (PSIFS_IRP_CONTEXT) Context;

    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == SIFSICX) &&
           (IrpContext->Identifier.Size == sizeof(SIFS_IRP_CONTEXT)));

    __try {

        __try {

            FsRtlEnterFileSystem();

            if (!IrpContext->IsTopLevel) {
                IoSetTopLevelIrp((PIRP) FSRTL_FSP_TOP_LEVEL_IRP);
            }

            IrpContext->SifsDispatchRequest(IrpContext);

        } __except (SifsExceptionFilter(IrpContext, GetExceptionInformation())) {

            SifsExceptionHandler(IrpContext);
        }

    } __finally {

        IoSetTopLevelIrp(NULL);

        FsRtlExitFileSystem();
    }
}

static FLT_PREOP_CALLBACK_STATUS
SifsBuildRequest (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext,
    __in PSIFS_PARAMETERS SifsParameters,
    __in NTSTATUS (*SifsDispatchRequest) (__in PSIFS_IRP_CONTEXT IrpContext)
    )
{
    FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_COMPLETE;
    BOOLEAN             atIrqlPassiveLevel = FALSE;
    BOOLEAN             isTopLevelIrp = FALSE;
    PSIFS_IRP_CONTEXT   irpContext = NULL;
    NTSTATUS            status = STATUS_UNSUCCESSFUL;
         
    __try {

        __try {

            atIrqlPassiveLevel = (KeGetCurrentIrql() == PASSIVE_LEVEL);

            if (atIrqlPassiveLevel) {
				
                FsRtlEnterFileSystem();
            }

            if (!IoGetTopLevelIrp()) {
				
                isTopLevelIrp = TRUE;
                IoSetTopLevelIrp(Data);
            }

            irpContext = SifsAllocateIrpContext(Data, FltObjects, SifsParameters);

            if (!irpContext) {

                status = STATUS_INSUFFICIENT_RESOURCES;
				
		  Data->IoStatus.Status = status;
		  Data->IoStatus.Information = 0;		  

            } else {

                if ((irpContext->MajorFunction == IRP_MJ_CREATE) &&
                        !atIrqlPassiveLevel) {

                    DbgBreak();
                }

		  FltReferenceContext(VolumeContext);
		  irpContext->VolumeContext = VolumeContext;
		  irpContext->SifsDispatchRequest = SifsDispatchRequest;	 
		  
                retValue = SifsDispatchRequest(irpContext);
            }
        } __except (SifsExceptionFilter(irpContext, GetExceptionInformation())) {

            status = SifsExceptionHandler(irpContext);

	     if(status == STATUS_PENDING) {

	         retValue = FLT_PREOP_PENDING;
	     }else{

		  Data->IoStatus.Status = status;
		  Data->IoStatus.Information = 0;
	     }
        }

    } __finally  {

        if (isTopLevelIrp) {
            IoSetTopLevelIrp(NULL);
        }

        if (atIrqlPassiveLevel) {
            FsRtlExitFileSystem();
        }

	 if(retValue != FLT_PREOP_SUCCESS_NO_CALLBACK) {

		FltSetCallbackDataDirty(Data);
    	}
    }

    return retValue;
}


FLT_PREOP_CALLBACK_STATUS
SifsPreCreate(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	PFLT_FILE_NAME_INFORMATION 	nameInfo = NULL;
	NTSTATUS status = STATUS_SUCCESS;
	BOOLEAN fileExist = FALSE;
	BOOLEAN directory = FALSE;	
	FLT_TASK_STATE 	taskState = FLT_TASK_STATE_UNKNOWN;
	ULONG                  createDisposition = (Data->Iopb->Parameters.Create.Options >> 24) & 0x000000ff;
	SIFS_PARAMETERS parameters = {  0 };

	if(FileFltStatusValidate) {

		goto SifsPreCreateCleanup;
		
	}

	if(SifsCheckPreCreatePassthru_1(Data, FltObjects) == 0) {
		
		goto SifsPreCreateCleanup;
	}
	
	status = FltGetFileNameInformation( Data,
	                                        FLT_FILE_NAME_NORMALIZED |
	                                        FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP,
	                                        &nameInfo );
       if (!NT_SUCCESS( status )) {

            goto SifsPreCreateCleanup;
       }

       status = FltParseFileNameInformation( nameInfo );
	  
       if (!NT_SUCCESS( status )) {
        
	 	goto SifsPreCreateCleanup;
       }

	if(FsRtlDoesNameContainWildCards(&nameInfo->FinalComponent)) {

		goto SifsPreCreateCleanup;
	}

	if(SifsIsNameValid(&nameInfo->FinalComponent) == FALSE) {

		goto SifsPreCreateCleanup;
	}

	if((FsCheckFileExistAndDirectoryByFileName(Data, FltObjects, nameInfo, &fileExist, &directory) == 0) 
		&& (directory == TRUE)){

		goto SifsPreCreateCleanup;
	}

       if(SifsCheckPreCreatePassthru_2(Data, FltObjects, VolumeContext, nameInfo) == 0) {
			
		goto SifsPreCreateCleanup;
	}

	if((fileExist == FALSE)
	 	&& ((createDisposition == FILE_OPEN) || (createDisposition == FILE_OVERWRITE))) {
	 	
	 	goto SifsPreCreateCleanup;
	}
	
	if((fileExist == TRUE)
		&& ((createDisposition == FILE_CREATE))) {
		
		goto SifsPreCreateCleanup;
	}

	taskState = SifsGetTaskStateInPreCreate(Data);	

	RtlZeroMemory(&parameters, sizeof(parameters));
	
	parameters.Create.NameInfo = nameInfo;
	parameters.Create.FileExist = fileExist;

	nameInfo = NULL;
	
	retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonCreate);

SifsPreCreateCleanup:

	if(nameInfo != NULL) {

		FltReleaseFileNameInformation( nameInfo );
	}	
	
	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreCleanup(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonCleanup);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreClose(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonClose);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreRead(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonRead);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreWrite(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonWrite);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreQueryInformation (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonQueryFileInformation);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreSetInformation(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,    
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonSetFileInformation);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreNetworkQueryOpen(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		if(FLT_IS_FASTIO_OPERATION(Data)) {

			retValue = FLT_PREOP_DISALLOW_FASTIO;
		}
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreDirCtrlBuffers(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreLockControl(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonLockControl);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreFlushBuffers(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonFlushBuffers);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreFastIoCheckIfPossible(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsFastIoCheckIfPossible);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreQueryEa(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonFlushBuffers);
	}

	return retValue;
}

FLT_PREOP_CALLBACK_STATUS
SifsPreSetEa(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    )
{
	FLT_PREOP_CALLBACK_STATUS retValue = FLT_PREOP_SUCCESS_NO_CALLBACK;
	SIFS_PARAMETERS parameters = {  0 };

	if(SifsCheckFcbTypeIsSifs(FltObjects->FileObject) == TRUE) {
		
		retValue = SifsBuildRequest(Data, FltObjects, CompletionContext, VolumeContext, &parameters, SifsCommonSetEa);
	}

	return retValue;
}

