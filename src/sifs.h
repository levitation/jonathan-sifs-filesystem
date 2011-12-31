#ifndef __FILEFLT_SIFS_H__
#define __FILEFLT_SIFS_H__

//--------------------------------------------------------------

#if DBG
# define SIFS_DEBUG   1
#else
# define SIFS_DEBUG   0
#endif

#if SIFS_DEBUG
#if _X86_
#define DbgBreak()      __asm int 3
#else
#define DbgBreak()      KdBreakPoint()
#endif
#else
#define DbgBreak()
#endif

#define IsFlagOn(a,b) ((BOOLEAN)(FlagOn(a,b) == b))
#ifndef FlagOn
#define FlagOn(_F,_SF)        ((_F) & (_SF))
#endif

#ifndef BooleanFlagOn
#define BooleanFlagOn(F,SF)   ((BOOLEAN)(((F) & (SF)) != 0))
#endif

#ifndef SetFlag
#define SetFlag(_F,_SF)       ((_F) |= (_SF))
#endif

#ifndef ClearFlag
#define ClearFlag(_F,_SF)     ((_F) &= ~(_SF))
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#define SetLongFlag(_F,_SF)       InterlockedOr(&(_F), (ULONG)(_SF))
#define ClearLongFlag(_F,_SF)     InterlockedAnd(&(_F), ~((ULONG)(_SF)))

#define CEILING_ALIGNED(T, A, B) (((A) + (B) - 1) & (~((T)(B) - 1)))
#define COCKLOFT_ALIGNED(T, A, B) (((A) + (B)) & (~((T)(B) - 1)))

#define READ_AHEAD_GRANULARITY          (0x10000)

//
// Define IsEndofFile for read and write operations
//

#define FILE_WRITE_TO_END_OF_FILE       0xffffffff

#define IsEndOfFile(Pos) (((Pos).LowPart == FILE_WRITE_TO_END_OF_FILE) && \
                          ((Pos).HighPart == -1 ))
                          
//
// Bug Check Codes Definitions
//

#define SIFS_FILE_SYSTEM   (FILE_SYSTEM)

#define SIFS_BUGCHK_BLOCK               (0x00010000)
#define SIFS_BUGCHK_CLEANUP             (0x00020000)
#define SIFS_BUGCHK_CLOSE               (0x00030000)
#define SIFS_BUGCHK_CMCB                (0x00040000)
#define SIFS_BUGCHK_CREATE              (0x00050000)
#define SIFS_BUGCHK_DEBUG               (0x00060000)
#define SIFS_BUGCHK_DEVCTL              (0x00070000)
#define SIFS_BUGCHK_DIRCTL              (0x00080000)
#define SIFS_BUGCHK_DISPATCH            (0x00090000)
#define SIFS_BUGCHK_EXCEPT              (0x000A0000)
#define SIFS_BUGCHK_EXT2                (0x000B0000)
#define SIFS_BUGCHK_FASTIO              (0x000C0000)
#define SIFS_BUGCHK_FILEINFO            (0x000D0000)
#define SIFS_BUGCHK_FLUSH               (0x000E0000)
#define SIFS_BUGCHK_FSCTL               (0x000F0000)
#define SIFS_BUGCHK_INIT                (0x00100000)
#define SIFS_BUGCHK_LOCK                (0x0011000)
#define SIFS_BUGCHK_MEMORY              (0x0012000)
#define SIFS_BUGCHK_MISC                (0x0013000)
#define SIFS_BUGCHK_READ                (0x00140000)
#define SIFS_BUGCHK_SHUTDOWN            (0x00150000)
#define SIFS_BUGCHK_VOLINFO             (0x00160000)
#define SIFS_BUGCHK_WRITE               (0x00170000)

#define SIFS_BUGCHK_LAST                (0x00170000)

#define SifsBugCheck(A,B,C,D) { KeBugCheckEx(SIFS_FILE_SYSTEM, A | __LINE__, B, C, D ); }

typedef enum _SIFS_IDENTIFIER_TYPE {
#ifdef _MSC_VER
    SIFSFGD  = ':DGF',
    SIFSVCB  = ':BCV',
    SIFSFCB  = ':BCF',
    SIFSCCB  = ':BCC',
    SIFSICX  = ':XCI',
    SIFSFSD  = ':DSF',
    SIFSMCB  = ':BCM'
#else
    SIFSFGD  = 0xE2FD0001,
    SIFSVCB  = 0xE2FD0002,
    SIFSFCB  = 0xE2FD0003,
    SIFSCCB  = 0xE2FD0004,
    SIFSICX  = 0xE2FD0005,
    SIFSFSD  = 0xE2FD0006,
    SIFSMCB  = 0xE2FD0007
#endif
} SIFS_IDENTIFIER_TYPE;

//
// SIFS_IDENTIFIER
//
// Header used to mark the structures
//
typedef struct _SIFS_IDENTIFIER {
    SIFS_IDENTIFIER_TYPE     Type;
    ULONG                    Size;
} SIFS_IDENTIFIER, *PSIFS_IDENTIFIER;

#define NodeType(Ptr) (*((SIFS_IDENTIFIER_TYPE *)(Ptr)))

typedef struct _SIFS_MCB  SIFS_MCB, *PSIFS_MCB;


typedef union _SIFS_PARAMETERS
{

	struct {

		PFLT_FILE_NAME_INFORMATION NameInfo;

		BOOLEAN FileExist;
	}Create;
	
}SIFS_PARAMETERS, *PSIFS_PARAMETERS;

typedef struct _SIFS_FCBVCB {

    // Command header for Vcb and Fcb
    FSRTL_ADVANCED_FCB_HEADER   Header;

    FAST_MUTEX                  Mutex;

    // Ext2Fsd identifier
    SIFS_IDENTIFIER             Identifier;


    // Locking resources
    ERESOURCE                   MainResource;
    ERESOURCE                   PagingIoResource;

} SIFS_FCBVCB, *PSIFS_FCBVCB;

typedef struct _SIFS_FILE_CONTEXT{
		
	    // Time stamps
	    LARGE_INTEGER                   CreationTime;
	    LARGE_INTEGER                   LastWriteTime;
	    LARGE_INTEGER                   ChangeTime;
	    LARGE_INTEGER                   LastAccessTime;
	    ULONG 				     FileAttributes;

	    HANDLE				     FileHandle;
	    PFILE_OBJECT			     FileObject;

	    LARGE_INTEGER		     AllocationSize;
	    LARGE_INTEGER		     ValidDataLength;
	    LARGE_INTEGER		     FileSize;
	    
	    BOOLEAN 				     Directory;
}SIFS_FILE_CONTEXT, *PSIFS_FILE_CONTEXT;

//
// SIFS_FCB File Control Block
//
// Data that represents an open file
// There is a single instance of the FCB for every open file
//
typedef struct _SIFS_FCB {

    /* Common header */
    SIFS_FCBVCB;

    // List of FCBs for this volume
    LIST_ENTRY                      Next;

    SECTION_OBJECT_POINTERS         SectionObject;

    // Share Access for the file object
    SHARE_ACCESS                    ShareAccess;

    // List of byte-range locks for this file
    FILE_LOCK                       FileLockAnchor;

    // oplock information management structure
    OPLOCK                          Oplock;

    // Lazy writer thread context
    PETHREAD                        LazyWriterThread;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
    ULONG                           OpenHandleCount;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
    ULONG                           ReferenceCount;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
    // But only for Files with FO_NO_INTERMEDIATE_BUFFERING flag
    ULONG                           NonCachedOpenCount;

    // Flags for the FCB
    ULONG                           Flags;

    // Pointer to the inode
    // Mcb Node ...
    PSIFS_MCB                       Mcb;

    PSIFS_FILE_CONTEXT	  Lower;

} SIFS_FCB, *PSIFS_FCB;

//
// Flags for SIFS_FCB
//

#define FCB_FROM_POOL               0x00000001
#define FCB_PAGE_FILE               0x00000002
#define FCB_FILE_MODIFIED           0x00000020
#define FCB_STATE_BUSY              0x00000040
#define FCB_ALLOC_IN_CREATE         0x00000080
#define FCB_ALLOC_IN_WRITE          0x00000100
#define FCB_DELETE_PENDING          0x00000200

//
// Mcb Node
//

struct _SIFS_MCB {

    // Identifier for this structure
    SIFS_IDENTIFIER                 Identifier;

    // Flags
    ULONG                           Flags;

    // Link List Info
    LIST_ENTRY			Next;


    // Mcb Node Info

    // -> Fcb
    PSIFS_FCB                       Fcb;

    // Short name
    UNICODE_STRING                  ShortName;

    // Full name with path
    UNICODE_STRING                  FullName;

    // File attribute
    ULONG                           FileAttr;

    // reference count
    ULONG                           Refercount;

    SIFS_FILE_CONTEXT       Lower;

    struct{

	LARGE_INTEGER                   CreationTime;
	LARGE_INTEGER                   LastWriteTime;
	LARGE_INTEGER                   ChangeTime;
	LARGE_INTEGER                   LastAccessTime;
	ULONG 				     	 FileAttributes;

	LARGE_INTEGER		     AllocationSize;
	LARGE_INTEGER		     ValidDataLength;
	LARGE_INTEGER		     FileSize;	

	ULONG 				     NumberOfLinks;
    };
    
};

//
// Flags for MCB
//
#define MCB_FROM_POOL               0x00000001
#define MCB_VCB_LINK                0x00000002
#define MCB_ENTRY_TREE              0x00000004
#define MCB_FILE_DELETED            0x00000008

#define MCB_ZONE_INIT               0x20000000
#define MCB_TYPE_SPECIAL            0x40000000  /* unresolved symlink + device node */
#define MCB_TYPE_SYMLINK            0x80000000

#define IsMcbUsed(Mcb)          ((Mcb)->Refercount > 0)
#define IsMcbSymLink(Mcb)       IsFlagOn((Mcb)->Flags, MCB_TYPE_SYMLINK)
#define IsZoneInited(Mcb)       IsFlagOn((Mcb)->Flags, MCB_ZONE_INIT)
#define IsMcbSpecialFile(Mcb)   IsFlagOn((Mcb)->Flags, MCB_TYPE_SPECIAL)
#define IsMcbRoot(Mcb)          ((Mcb)->Inode.i_ino == EXT2_ROOT_INO)
#define IsMcbReadonly(Mcb)      IsFlagOn((Mcb)->FileAttr, FILE_ATTRIBUTE_READONLY)
#define IsMcbDirectory(Mcb)     IsFlagOn((Mcb)->FileAttr, FILE_ATTRIBUTE_DIRECTORY)
#define IsFileDeleted(Mcb)      IsFlagOn((Mcb)->Flags, MCB_FILE_DELETED)

#define IsLinkInvalid(Mcb)      (IsMcbSymLink(Mcb) && IsFileDeleted(Mcb->Target))

/*
 * routines for reference count management
 */

#define SifsReferXcb(_C)  InterlockedIncrement(_C)
#define SifsDerefXcb(_C)  DEC_OBJ_CNT(_C)

__inline ULONG DEC_OBJ_CNT(PULONG _C) {
    if (*_C > 0) {
        return InterlockedDecrement(_C);
    } else {
        DbgBreak();
    }
    return 0;
}

#define SifsReferMcb(Mcb) SifsReferXcb(&Mcb->Refercount)
#define SifsDerefMcb(Mcb)  SifsDerefXcb(&Mcb->Refercount)

//
// SIFS_CCB Context Control Block
//
// Data that represents one instance of an open file
// There is one instance of the CCB for every instance of an open file
//
typedef struct _SIFS_CCB {

    // Identifier for this structure
    SIFS_IDENTIFIER     Identifier;

    // Flags
    ULONG               Flags;

    // State that may need to be maintained
    UNICODE_STRING      DirectorySearchPattern;

} SIFS_CCB, *PSIFS_CCB;

//
// Flags for CCB
//

#define CCB_FROM_POOL               0x00000001
#define CCB_VOLUME_DASD_PURGE       0x00000002
#define CCB_LAST_WRITE_UPDATED      0x00000004

#define CCB_DELETE_ON_CLOSE         0x00000010

#define CCB_ALLOW_EXTENDED_DASD_IO  0x80000000


typedef struct _sifs_icb SIFS_IRP_CONTEXT;
typedef SIFS_IRP_CONTEXT *PSIFS_IRP_CONTEXT;

struct _sifs_icb {

    // Identifier for this structure
    SIFS_IDENTIFIER     Identifier;

    PFLT_CALLBACK_DATA Data;
		
    PCFLT_RELATED_OBJECTS FltObjects;
   
    // Flags
    ULONG               Flags;

    // The major and minor function code for the request
    UCHAR               MajorFunction;
    UCHAR               MinorFunction;

    // The real device object
    PDEVICE_OBJECT      RealDevice;

    // The file object
    PFILE_OBJECT        FileObject;

    PSIFS_FCB           Fcb;
    PSIFS_CCB           Ccb;

    // If the request is top level
    BOOLEAN             IsTopLevel;

    // Used if the request needs to be queued for later processing
    WORK_QUEUE_ITEM     WorkQueueItem;

    // If an exception is currently in progress
    BOOLEAN             ExceptionInProgress;

    // The exception code when an exception is in progress
    NTSTATUS            ExceptionCode;

    SIFS_PARAMETERS Parameters;

    PVOLUME_CONTEXT VolumeContext;

    NTSTATUS (*SifsDispatchRequest) (__in PSIFS_IRP_CONTEXT IrpContext);

} ;

#define IRP_CONTEXT_FLAG_FROM_POOL       (0x00000001)
#define IRP_CONTEXT_FLAG_WAIT            (0x00000002)
#define IRP_CONTEXT_FLAG_WRITE_THROUGH   (0x00000004)
#define IRP_CONTEXT_FLAG_FLOPPY          (0x00000008)
#define IRP_CONTEXT_FLAG_DISABLE_POPUPS  (0x00000020)
#define IRP_CONTEXT_FLAG_DEFERRED        (0x00000040)
#define IRP_CONTEXT_FLAG_VERIFY_READ     (0x00000080)
#define IRP_CONTEXT_STACK_IO_CONTEXT     (0x00000100)
#define IRP_CONTEXT_FLAG_REQUEUED        (0x00000200)
#define IRP_CONTEXT_FLAG_USER_IO         (0x00000400)
#define IRP_CONTEXT_FLAG_DELAY_CLOSE     (0x00000800)
#define IRP_CONTEXT_FLAG_FILE_BUSY       (0x00001000)


#define SifsCanIWait() (!IrpContext || IsFlagOn(IrpContext->Flags, IRP_CONTEXT_FLAG_WAIT))

// Include this so we don't need the latest WDK to build the driver.
#ifndef FSCTL_GET_RETRIEVAL_POINTER_BASE
#define FSCTL_GET_RETRIEVAL_POINTER_BASE    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 141, METHOD_BUFFERED, FILE_ANY_ACCESS) // RETRIEVAL_POINTER_BASE
#endif

#define SifsRaiseStatus(IRPCONTEXT,STATUS) {  \
    (IRPCONTEXT)->ExceptionCode = (STATUS); \
    ExRaiseStatus((STATUS));                \
}

#define SifsNormalizeAndRaiseStatus(IRPCONTEXT,STATUS) {                        \
    (IRPCONTEXT)->ExceptionCode = STATUS;                                       \
    if ((STATUS) == STATUS_VERIFY_REQUIRED) { ExRaiseStatus((STATUS)); }        \
    ExRaiseStatus(FsRtlNormalizeNtstatus((STATUS),STATUS_UNEXPECTED_IO_ERROR)); \
}


//--------------------------------------------------------------
//sifs

NTSTATUS
SifsInstanceSetup(
   __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType,
    __inout PVOLUME_CONTEXT VolumeContext
    );

VOID
SifsCleanupContext(
    __in PFLT_CONTEXT Context,
    __in FLT_CONTEXT_TYPE ContextType
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreCreate(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreCleanup(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreClose(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreRead(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreWrite(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreQueryInformation (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreSetInformation(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreNetworkQueryOpen(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreDirCtrlBuffers(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreLockControl(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreFlushBuffers(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreFastIoCheckIfPossible(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreQueryEa(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreSetEa(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext,
    __in PVOLUME_CONTEXT VolumeContext
    );

ULONG
SifsValidateFileSize(
	__in PSTREAM_CONTEXT StreamContext
	);


NTSTATUS
SifsQueueRequest (
	__in  PSIFS_IRP_CONTEXT IrpContext
	);

VOID
SifsDeQueueRequest (
	__in PVOID Context
	);

VOID
SifsOplockComplete (
    __in PFLT_CALLBACK_DATA CallbackData,
    __in PVOID Context
	);

VOID
SifsLockIrp (
    __inout PFLT_CALLBACK_DATA Data,
    __in PVOID Context    
	);

//--------------------------------------------------------------
//crypto

VOID
SifsInitializeCryptContext(
	__inout PCRYPT_CONTEXT CryptContext
	);

int
SifsWriteHeadersVirt(
	__inout PUCHAR PageVirt,
	__in LONG Max,
	__out PLONG Size,
	__in PCRYPT_CONTEXT CryptContext
	);

int
SifsReadHeadersVirt(
	__in PUCHAR PageVirt,
	__inout PCRYPT_CONTEXT CryptContext,
	__in  LONG ValidateHeaderSize
	);

int
SifsQuickCheckValidate_i(
	__in PUCHAR Buffer
	);
//--------------------------------------------------------------
//read_write


int
SifsWriteSifsMetadata(
       __inout PFLT_INSTANCE Instance,
       __in ULONG                     DesiredAccess,
        __in ULONG                     CreateDisposition,
        __in ULONG                     CreateOptions,
        __in ULONG                     ShareAccess,
        __in ULONG                     FileAttribute,
	__in PFLT_FILE_NAME_INFORMATION NameInfo,
	__inout PCRYPT_CONTEXT CryptContext
	);

int
SifsReadSifsMetadata(
       __in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject,	
	__inout PSTREAM_CONTEXT StreamContext
	);

int
SifsQuickCheckValidate(
       __in PFLT_INSTANCE Instance,
	__in PUNICODE_STRING FileName,
	__inout PCRYPT_CONTEXT CryptContext,
	__inout PBOOLEAN IsEmptyFile,
	__in LONG Aligned
	);

int
SifsQuickCheckValidateSifs(
	__in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject,
	__out PUCHAR  PageVirt,
	__in LONG PageVirtLen
	);

int
SifsWriteFileSize(
	__in PFLT_INSTANCE Instance,
	__in PUNICODE_STRING FileName,
	__inout PUCHAR Metadata,
	__in LONG MetadataLen,
	__in LONGLONG  FileSize
	);

//-----------------------------------------------------------------------
//keystore
int
SifsGenerateKeyPacketSet(
	__inout PUCHAR DestBase,
	__in PCRYPT_CONTEXT CryptContext,
	__in PLONG Len,
	__in LONG Max
	);

int 
SifsParsePacketSet(
	__inout PCRYPT_CONTEXT CryptContext,
	__in PUCHAR Src
	);

//-----------------------------------------------------------------------------
//misc
FLT_TASK_STATE
SifsGetTaskStateInPreCreate(
	__inout PFLT_CALLBACK_DATA Data
	);

int
SifsCheckPreCreatePassthru_1(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects
    );

int
SifsCheckPreCreatePassthru_2(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOLUME_CONTEXT VolumeContext,
    __in PFLT_FILE_NAME_INFORMATION NameInfo
    );

BOOLEAN
SifsIsNameValid(
	__in PUNICODE_STRING FileName
	);

NTSTATUS
SifsLookupFile(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PVOLUME_CONTEXT VolumeContext, 
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName, 
	__out PSIFS_MCB *Mcb
	);

NTSTATUS
SifsCreateUnderlyingFile(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PVOLUME_CONTEXT VolumeContext, 
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName, 
	__out PSIFS_MCB *Mcb
	);

NTSTATUS
SifsOpenUnderlyingFile(
	__in PSIFS_IRP_CONTEXT IrpContext, 
	__in PVOLUME_CONTEXT VolumeContext, 
	__in PUNICODE_STRING FileName, 
	__in PUNICODE_STRING LastFileName, 
	__out PSIFS_MCB *Mcb
	);

BOOLEAN 
SifsCheckFcbTypeIsSifs(
	__in PFILE_OBJECT FileObject
	);

//-----------------------------------------------------------------------------
//create

FLT_PREOP_CALLBACK_STATUS
SifsCommonCreate(
    __in PSIFS_IRP_CONTEXT IrpContext
    );

//-----------------------------------------------------------------------------
//cleanup

FLT_PREOP_CALLBACK_STATUS
SifsCommonCleanup (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//close

FLT_PREOP_CALLBACK_STATUS
SifsCommonClose (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//read

NTSTATUS
SifsCompleteIrpContext (
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in NTSTATUS Status 
    );

FLT_PREOP_CALLBACK_STATUS
SifsCommonRead (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//write

NTSTATUS
SifsCommonWrite (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//block

NTSTATUS
SifsLockUserBuffer (
	__in  PFLT_CALLBACK_DATA Data,
       __in  ULONG            Length,
       __in  LOCK_OPERATION   Operation
       );

PVOID
SifsGetUserBufferOnRead (
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	);

PVOID
SifsGetUserBufferOnWrite(
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	);

PVOID
SifsGetUserBufferOnSetEa(
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	);

PVOID
SifsGetUserBufferOnQueryEa(
	__in  PFLT_IO_PARAMETER_BLOCK Iopb
	);

//-----------------------------------------------------------------------------
//except

NTSTATUS
SifsExceptionFilter (
    __in PSIFS_IRP_CONTEXT    IrpContext,
    __in PEXCEPTION_POINTERS  ExceptionPointer
	);

NTSTATUS
SifsExceptionHandler (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//memory

PSIFS_IRP_CONTEXT
SifsAllocateIrpContext (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PSIFS_PARAMETERS SifsParameters
    );

VOID
SifsFreeIrpContext (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

PSIFS_FCB
SifsAllocateFcb (
    __in PSIFS_MCB   Mcb
	);

VOID
SifsFreeFcb (
	__in PSIFS_FCB Fcb
	);

PSIFS_CCB
SifsAllocateCcb (
	VOID
	);

VOID
SifsFreeCcb (
	__in PSIFS_CCB Ccb
	);

PSIFS_MCB
SifsAllocateMcb (
    __in PUNICODE_STRING  FileName,
    __in PUNICODE_STRING  ShortName,
    __in ULONG            FileAttr
	);

VOID
SifsFreeMcb (
	__in PSIFS_MCB Mcb
	);

VOID
SifsCleanupAllMcbs(
	__in PVOLUME_CONTEXT Vcb
	);

VOID
SifsInsertMcb (
    __in PVOLUME_CONTEXT  Vcb,
    __in PSIFS_MCB Child
	);

BOOLEAN
SifsRemoveMcb (
    __in PVOLUME_CONTEXT  Vcb,
    __in PSIFS_MCB Mcb
	);

NTSTATUS
SifsStartReaperThread(
	__in PVOID Context
	);

VOID
SifsStopReaperThread(
	__in PVOID Context
	);

//-----------------------------------------------------------------------------
//fileinfo

FLT_PREOP_CALLBACK_STATUS
SifsCommonQueryFileInformation (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

FLT_PREOP_CALLBACK_STATUS
SifsCommonSetFileInformation (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

NTSTATUS
SifsIsFileRemovable(
    __in PSIFS_FCB            Fcb
	);

NTSTATUS
SifsExpandAndTruncateFile(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_MCB         Mcb,
    __in PLARGE_INTEGER    Size
	);

NTSTATUS
SifsDeleteFile(
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PSIFS_FCB         Fcb,
    __in PSIFS_MCB         Mcb
	);
//-----------------------------------------------------------------------------
//dirctl

VOID
SifsNotifyReportChange (
    __in PSIFS_IRP_CONTEXT IrpContext,
    __in PVOLUME_CONTEXT VolumeContext,
    __in PSIFS_MCB         Mcb,
    __in ULONG             Filter,
    __in ULONG             Action   
    );

//-----------------------------------------------------------------------------
//flush

NTSTATUS
SifsFlushFile (
    __in PSIFS_IRP_CONTEXT    IrpContext,
    __in PSIFS_FCB            Fcb,
    __in PSIFS_CCB            Ccb
	);

FLT_PREOP_CALLBACK_STATUS
SifsCommonFlushBuffers (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//fastio

FAST_IO_POSSIBLE
SifsIsFastIoPossible(
    __in PSIFS_FCB Fcb
	);

FLT_PREOP_CALLBACK_STATUS
SifsFastIoCheckIfPossible (
    __in PSIFS_IRP_CONTEXT IrpContext
);

FLT_PREOP_CALLBACK_STATUS
SifsFastIoRead (
	__in PSIFS_IRP_CONTEXT IrpContext
       );

FLT_PREOP_CALLBACK_STATUS
SifsFastIoWrite (
    __in PSIFS_IRP_CONTEXT IrpContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsFastIoQueryBasicInfo (
    __in PSIFS_IRP_CONTEXT IrpContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsFastIoQueryStandardInfo (
    __in PSIFS_IRP_CONTEXT IrpContext
);

FLT_PREOP_CALLBACK_STATUS
SifsFastIoQueryNetworkOpenInformation (
    __in PSIFS_IRP_CONTEXT IrpContext
);

//-----------------------------------------------------------------------------
//lock

FLT_PREOP_CALLBACK_STATUS
SifsCommonLockControl (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

//-----------------------------------------------------------------------------
//cmcb
BOOLEAN
SifsAcquireForLazyWrite (
    __in PVOID    Context,
    __in BOOLEAN  Wait
    );

VOID
SifsReleaseFromLazyWrite (
	__in PVOID Context
	);

BOOLEAN
SifsAcquireForReadAhead (
	__in PVOID    Context,
       __in BOOLEAN  Wait
       );

VOID
SifsReleaseFromReadAhead (
	__in PVOID Context
	);

BOOLEAN
SifsNoOpAcquire (
    __in PVOID Fcb,
    __in BOOLEAN Wait
	);

VOID
SifsNoOpRelease (
    __in PVOID Fcb
	);

VOID
SifsAcquireForCreateSection (
    __in PFILE_OBJECT FileObject
	);

VOID
SifsReleaseForCreateSection (
    __in PFILE_OBJECT FileObject	
    );

NTSTATUS
SifsAcquireFileForModWrite (
    __in PFILE_OBJECT FileObject,
    __in PLARGE_INTEGER EndingOffset,
    __out PERESOURCE *ResourceToRelease,
    __in PDEVICE_OBJECT DeviceObject
	);

NTSTATUS
SifsReleaseFileForModWrite (
    __in PFILE_OBJECT FileObject,
    __in PERESOURCE ResourceToRelease,
    __in PDEVICE_OBJECT DeviceObject
);

NTSTATUS
SifsAcquireFileForCcFlush (
    __in PFILE_OBJECT FileObject,
    __in PDEVICE_OBJECT DeviceObject
);

NTSTATUS
SifsReleaseFileForCcFlush (
    __in PFILE_OBJECT FileObject,
    __in PDEVICE_OBJECT DeviceObject
);

//-----------------------------------------------------------------------------
// ea

FLT_PREOP_CALLBACK_STATUS
SifsCommonQueryEa (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

FLT_PREOP_CALLBACK_STATUS
SifsCommonSetEa (
	__in PSIFS_IRP_CONTEXT IrpContext
	);

#endif /* __FILEFLT_SIFS_H__ */
