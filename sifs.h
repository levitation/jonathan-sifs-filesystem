#ifndef __FILEFLT_SIFS_H__
#define __FILEFLT_SIFS_H__

#define SIFS_VERSION_MAJOR 0x00
#define SIFS_VERSION_MINOR 0x04
#define SIFS_SUPPORTED_FILE_VERSION 0x03

#define SIFS_ROUND_DATA_LEN(x,y) (((x) + ((y) -1)) & (~((y) - 1)))
#define SIFS_PADDING_DATA_LEN(x,y,z)  ((x) - (y) - (z))

#define SIFS_SALT_SIZE 8
#define SIFS_SALT_SIZE_HEX (SIFS_SALT_SIZE*2)
#define SIFS_MAX_KEY_BYTES 64
#define SIFS_MAX_ENCRYPTED_KEY_BYTES 512
#define SIFS_DEFAULT_IV_BYTES 16

#define SIFS_MAX_IV_BYTES 16	/* 128 bits */
#define SIFS_SALT_BYTES 2

#define SIFS_MINIMUM_HEADER_EXTENT_SIZE  	8192
#define SIFS_DEFAULT_EXTERN_SIZE       			4096

#define MAGIC_SIFS_MARKER 0x3c81b7f5
#define MAGIC_SIFS_MARKER_SIZE_BYTES 8	/* 4*2 */

#define SIFS_FILE_SIZE_BYTES (sizeof(LONGLONG))

#define SIFS_CHECK_FILE_VALID_MINIMUN_SIZE (SIFS_FILE_SIZE_BYTES + MAGIC_SIFS_MARKER_SIZE_BYTES)

#define SIFS_DONT_VALIDATE_HEADER_SIZE 0
#define SIFS_VALIDATE_HEADER_SIZE 1
//--------------------------------------------------------------

struct _CRYPT_CONTEXT{
#define SIFS_STRUCT_INITIALIZED   	0x00000001
#define SIFS_POLICY_APPLIED       		0x00000002
#define SIFS_ENCRYPTED            		0x00000004
#define SIFS_SECURITY_WARNING     	0x00000008
#define SIFS_ENABLE_HMAC          		0x00000010
#define SIFS_ENCRYPT_IV_PAGES     	0x00000020
#define SIFS_KEY_VALID            		0x00000040
#define SIFS_METADATA_IN_XATTR    	0x00000080
#define SIFS_VIEW_AS_ENCRYPTED    	0x00000100
#define SIFS_KEY_SET              		0x00000200
#define SIFS_ENCRYPT_FILENAMES    	0x00000400
#define SIFS_ENCFN_USE_MOUNT_FNEK 0x00000800
#define SIFS_ENCFN_USE_FEK        	0x00001000
#define SIFS_UNLINK_SIGS          		0x00002000
#define SIFS_I_SIZE_INITIALIZED   	0x00004000
	unsigned int Flags;
	unsigned int FileVersion;
	unsigned int IvBytes;
	unsigned int KeySize;
	unsigned int ExtentShift;
	unsigned int ExtentMask;

	ULONG  ExternSize;
	ULONG  MetadataSize;

	LONGLONG ValidDataSize;

	unsigned char Key[SIFS_MAX_KEY_BYTES];
	unsigned char Root_iv[SIFS_MAX_IV_BYTES];
	
};

//--------------------------------------------------------------
//sifs

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
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreClose(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreRead(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreWrite(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreQueryInformation (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreSetInformation(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreNetworkQueryOpen(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

FLT_PREOP_CALLBACK_STATUS
SifsPreDirCtrlBuffers(
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PVOID *CompletionContext
    );

int
SifsCheckFileValid(
	__in PFLT_INSTANCE Instance,
	__in PFILE_OBJECT FileObject
	);

//--------------------------------------------------------------
//crypto

PVOID
SifsAllocateCryptContext(
	VOID
	);

VOID
SifsFreeCryptContext(
	__in PVOID CryptContext
	);

VOID
SifsInitializeCryptContext(
	__inout PCRYPT_CONTEXT CryptContext
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
	__in PFLT_FILE_NAME_INFORMATION NameInfo,
	__inout PCRYPT_CONTEXT CryptContext,
	__inout PBOOLEAN IsEmptyFile
	);

ULONG
SifsFileValidateLength(
	__in PSTREAM_CONTEXT StreamContext
	);

#endif /* __FILEFLT_SIFS_H__ */
