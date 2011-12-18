#include "fileflt.h"

VOID
SifsInitializeCryptContext(
	__inout PCRYPT_CONTEXT CryptContext
	)
{
	CryptContext->ExternSize = SIFS_DEFAULT_EXTERN_SIZE;
	CryptContext->MetadataSize = SIFS_MINIMUM_HEADER_EXTENT_SIZE;

	RtlCopyMemory(CryptContext->Key, "0123456", 7);
}
