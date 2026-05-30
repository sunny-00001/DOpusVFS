#include "stdafx.h"
#include "LeoHelpers.h"
#include "Win32IOWrapper.h"
#include "audiotags_config.h"
#include "audiotags_oggvorbis.h"

// Minimal, forward-read-only Ogg Vorbis/Ogg FLAC metadata extractor. (C) Leo Davidson 2009.

bool AudioTags_OggVorbis::readNextDataChunkFromOggStream(Win32IOWrapper *pfw,
														 OggPageHeader *pOggHeader,
														 BYTE &currentSegment,
														 std::vector< BYTE > *pChunkData,
														 bool bCheckForVorbisComments,
														 bool bCheckForFlacComments,
														 bool bRequireGranuleZero)
{
	pChunkData->clear();

	bool bFirst = true;

	while(true)
	{
		if (currentSegment >= pOggHeader->chSegmentCount)
		{
			assert(currentSegment == pOggHeader->chSegmentCount);

			currentSegment = 0;

			// read next page header in and sanity check the first/continuation flags.
			if (!readOggPageHeader(pfw, pOggHeader)
			||	( bFirst && (pOggHeader->chFlags&(OGG_PAGE_FLAG_FIRST)) != 0)
			||	(!bFirst && (pOggHeader->chFlags&(OGG_PAGE_FLAG_FIRST|OGG_PAGE_FLAG_CONTINUATION)) != OGG_PAGE_FLAG_CONTINUATION))
			{
				break;
			}
		}

		bFirst = false;

		if (bRequireGranuleZero && pOggHeader->ui64GranulePosition != 0)
		{
			break;
		}

		size_t amountToRead = 0;
		bool bGotEndSegment = false;

		while(currentSegment < pOggHeader->chSegmentCount)
		{
			amountToRead += pOggHeader->SegmentSizes[currentSegment]; // The size may be zero. It'll happen if the data size is a multiple of 255.

			if (pOggHeader->SegmentSizes[currentSegment++] < 255)
			{
				// The data ends at the first segment which is < 255 bytes.
				// Note that the pages do not have to be full for us to keep reading; only the segments do.
				// Some encoders will split the metadata into 4KB pages instead of using the full 64KB possible per page.
				// (That also seems to be normal for the audio data.)
				bGotEndSegment = true;
				break;
			}
		}

		if (amountToRead == 0)
		{
			break; // This is valid if it's a continuation page. It'll happen if the data size happens to be a multiple of 255*255.
		}

		if (pChunkData->empty() && amountToRead == (255*255))
		{
			// We're probably going to read some or all of the second page. Might as well allocate two full pages' worth in advance.
			// After this we leave it up to STL to grow the buffer in a reasonable way.
			pChunkData->reserve(2*255*255);
		}

		size_t oldSize = pChunkData->size();

		pChunkData->resize(pChunkData->size() + amountToRead);

		BYTE *readBuffer = &((*pChunkData)[oldSize]);

		size_t amountActuallyRead = pfw->read(readBuffer, amountToRead);

		if (amountActuallyRead != amountToRead)
		{
			pChunkData->resize(oldSize + (amountActuallyRead < amountToRead ? amountActuallyRead : 0));
			break;
		}

		if (oldSize == 0)
		{
			if (bCheckForVorbisComments)
			{
				assert(!bCheckForFlacComments && !bRequireGranuleZero);

				// Check that the first block of data is the start of a vorbis comments structure.
				const VorbisCommentsHeaderStart *pVC = reinterpret_cast< VorbisCommentsHeaderStart *>( &((*pChunkData)[0]) );

				if (pChunkData->size() < VORBISCOMMENTS_MINSIZE
				||	pVC->chID != 3
				||  0 != memcmp(pVC->Signature, "vorbis", 6))
				{
					pChunkData->clear();
					break;
				}
			}
			else if (bCheckForFlacComments)
			{
				assert(!bCheckForVorbisComments);

				if (pChunkData->size() < FLAC_VORBISCOMMENTS_MINSIZE
				||	(((*pChunkData)[0])&0x7F) != 4)
				{
					pChunkData->clear();
					break;
				}
			}
		}

		// If we reached a segment size < 255 then that was the last segment of the data chunk.
		// If the page was flagged as last then that's the end as well. (And probably indicates a bad file, but we'll try to process what we got.)
		if (bGotEndSegment
		||	((pOggHeader->chFlags&OGG_PAGE_FLAG_LAST) == OGG_PAGE_FLAG_LAST))
		{
			break;
		}
	}

	return !pChunkData->empty();
}

void AudioTags_OggVorbis::ParseVorbisCommentMemory(LPDVPFILEINFOMUSICW pMusicInfo, const BYTE *pCommentData, size_t commentBytesLeft, std::vector< VorbisCommentSearchParams > &searchVec, bool &bGotAnything)
{
	bool bWantEncoder = (pMusicInfo != NULL && pMusicInfo->lpszEncoder != NULL && pMusicInfo->cchEncoderMax > 0);
	std::string strUtf8;
	DWORD dwNumCommentStrings = 0;

	if (getVorbisCommentString(bWantEncoder ? (&strUtf8) : NULL, &pCommentData, &commentBytesLeft)
	&&	getVorbisCommentDWORD(&dwNumCommentStrings, &pCommentData, &commentBytesLeft))
	{
		if (bWantEncoder)
		{
			LeoHelpers::MBtoWCInPlaceTruncate(strUtf8.c_str(), -1, pMusicInfo->lpszEncoder, pMusicInfo->cchEncoderMax, CP_UTF8, true, NULL);
		}

		size_t cchMaxKeyLen = 0;

		for(std::vector< VorbisCommentSearchParams >::const_iterator viter = searchVec.begin(); viter != searchVec.end(); ++viter)
		{
			if (cchMaxKeyLen < viter->GetKeyLen())
			{
				cchMaxKeyLen = viter->GetKeyLen();
			}
		}

		for(DWORD dwStrNum = 0; dwStrNum < dwNumCommentStrings; ++dwStrNum)
		{
			if (!processVorbisCommentString(searchVec, cchMaxKeyLen, &pCommentData, &commentBytesLeft, &bGotAnything))
			{
				break;
			}
		}
	}
}

// Only called once per file. Not worth inlining.
bool AudioTags_OggVorbis::getVorbisCommentString(std::string *pstrUtf8Out, const BYTE **ppCommentData, size_t *pCommentBytesLeft)
{
	if (*pCommentBytesLeft < 4)
	{
		return false;
	}

	DWORD dwStringLen = *reinterpret_cast< const DWORD * >(*ppCommentData);
	(*ppCommentData) += sizeof(DWORD);
	(*pCommentBytesLeft) -= sizeof(DWORD);

	if (dwStringLen > *pCommentBytesLeft)
	{
		return false;
	}

	if (pstrUtf8Out != NULL)
	{
		pstrUtf8Out->assign(reinterpret_cast< const char * >( *ppCommentData ), dwStringLen);
	}

	(*ppCommentData) += dwStringLen;
	(*pCommentBytesLeft) -= dwStringLen;

	return true;
}

// Only called once per file. Not worth inlining.
bool AudioTags_OggVorbis::getVorbisCommentDWORD(DWORD *pdwOut, const BYTE **ppCommentData, size_t *pCommentBytesLeft)
{
	if (*pCommentBytesLeft < 4)
	{
		return false;
	}

	if (pdwOut != NULL)
	{
		*pdwOut = *reinterpret_cast< const DWORD * >(*ppCommentData);
	}
	(*ppCommentData) += sizeof(DWORD);
	(*pCommentBytesLeft) -= sizeof(DWORD);

	return true;
}

bool AudioTags_OggVorbis::processVorbisCommentString(std::vector< VorbisCommentSearchParams > &searchVec, size_t cchMaxKeyLen,
													 const BYTE **ppCommentData, size_t *pCommentBytesLeft, bool *pbGotAnything)
{
	// Get the length of the string and move to the string.

	if (*pCommentBytesLeft < 4)
	{
		return false;
	}

	DWORD dwStringLen = *reinterpret_cast< const DWORD * >(*ppCommentData);
	(*ppCommentData) += sizeof(DWORD);
	(*pCommentBytesLeft) -= sizeof(DWORD);

	if (dwStringLen > *pCommentBytesLeft)
	{
		return false;
	}

	// Turn the start of the string into a key.
	// If we don't find an equals sign before cchMaxKeyLen then we know we can simply skip the string.

	bool bJustSkip = true;

	std::string strKey;
	const size_t maxSearch = min(cchMaxKeyLen+1, dwStringLen);

	for(size_t i = 0; i < maxSearch; ++i)
	{
		const char c = reinterpret_cast< const char * >(*ppCommentData)[i];
		if (c == '=')
		{
			if (i != 0 && (i+1) < dwStringLen)
			{
				bJustSkip = false;
			}
			break;
		}
		strKey.push_back(LeoHelpers::CharToUpper(c));
	}

	if (!bJustSkip)
	{
		const size_t cchOurKeyLen = strKey.length();

		for (std::vector< VorbisCommentSearchParams >::iterator searchIter = searchVec.begin(); searchIter != searchVec.end(); ++searchIter)
		{
			if (cchOurKeyLen == searchIter->GetKeyLen()
			&&	0 == memcmp(strKey.c_str(), searchIter->GetUpperKey(), cchOurKeyLen * sizeof(searchIter->GetUpperKey()[0])))
			{
				if (searchIter->Capture(reinterpret_cast< const char * >(*ppCommentData) + cchOurKeyLen + 1, dwStringLen - (cchOurKeyLen + 1)))
				{
					*pbGotAnything = true;
				}

				break;
			}
		}
	}

	(*ppCommentData) += dwStringLen;
	(*pCommentBytesLeft) -= dwStringLen;

	return true;
}

bool AudioTags_OggVorbis::VorbisCommentSearchParams::Capture(const char *newStringUnterminated, size_t cchNewStringLength)
{
	if (m_piCaptureInt != NULL && *m_piCaptureInt == 0 && !m_bFull)
	{
		// Get the digits into a null-terminated buffer.
		char szDigits[32];
		size_t maxDigits = min(_countof(szDigits)-1,cchNewStringLength);
		size_t i = 0;
		while (i < maxDigits && LeoHelpers::IsDigit(newStringUnterminated[i]))
		{
			szDigits[i] = newStringUnterminated[i];
			++i;
		}
		szDigits[i] = '\0';
		
		if (i != 0)
		{
			// We can treat UTF8 as ASCII when it comes to digits.
			*m_piCaptureInt = atoi(szDigits);
			m_bFull = true;
		}
		return true;
	}

	if (m_szCaptureBuffer != NULL)
	{
		addToString(newStringUnterminated, cchNewStringLength);
		return true;
	}

	return false;
}

void AudioTags_OggVorbis::VorbisCommentSearchParams::addToString(const char *newStringUnterminated, size_t cchNewStringLength)
{
	if (m_bFull || m_szCaptureBuffer == NULL || m_cchBufferOriginal==0 || (m_bFirstMatchOnly && m_szBufferOriginal[0]))
	{
		return;
	}

	assert(m_cchBufferLeft > 0); // Must always be 1 left for the null.
	assert(m_cchBufferOriginal > 0);
	assert(m_szBufferOriginal != NULL);

	if (m_szCaptureBuffer != m_szBufferOriginal)
	{
		LeoHelpers::StringCopy(m_szCaptureBuffer, L"; ", m_cchBufferLeft);

		if (m_cchBufferLeft <= 2)
		{
			// The "; " won't fit so truncate the string.
			size_t newNullIdx = LeoHelpers::TruncateString(m_szBufferOriginal, m_cchBufferOriginal - 1, 0, true);

			m_szCaptureBuffer = m_szBufferOriginal + newNullIdx;
			m_cchBufferLeft   = m_cchBufferOriginal - newNullIdx;
			m_bFull           = true;
		}
		else
		{
			m_szCaptureBuffer += 2;
			m_cchBufferLeft -= 2;
		}
	}

	assert(m_cchBufferLeft > 0); // Must always be 1 left for the null.

	if (!m_bFull)
	{
		bool bConversionTruncated = false;

		// Do not ask MBtoWCInPlaceTruncate to put "..." at the end of the string if it is truncated, as it won't be able to use the full buffer when
		// we're only giving it the current part onwards. We'll do the "..." ourselves if we need to.
		if (!LeoHelpers::MBtoWCInPlaceTruncate(newStringUnterminated, static_cast< int >( cchNewStringLength ), m_szCaptureBuffer, static_cast< int >( m_cchBufferLeft ), CP_UTF8, false, &bConversionTruncated)
		||	bConversionTruncated)
		{
			size_t oldNullIdx = wcslen(m_szBufferOriginal);

			size_t newNullIdx = LeoHelpers::TruncateString(m_szBufferOriginal, oldNullIdx, m_cchBufferOriginal - (oldNullIdx + 1), true);

			m_szCaptureBuffer = m_szBufferOriginal + newNullIdx;
			m_cchBufferLeft   = m_cchBufferOriginal - newNullIdx;
			m_bFull           = true;
		}
		else
		{
			size_t newNullIdx = wcslen(m_szBufferOriginal);

			m_szCaptureBuffer = m_szBufferOriginal + newNullIdx;
			m_cchBufferLeft   = m_cchBufferOriginal - newNullIdx;
		}
	}

	assert(m_cchBufferLeft > 0); // Must always be 1 left for the null.
}

void AudioTags_OggVorbis::ProcessFlacStreamInfo(const BYTE *streamInfo, LPDVPFILEINFOMUSICW pMusicInfo, bool &bGotAnything)
{
	// We ignore the first 10 bytes (80 bits).

	// Next 20 bits is the sample rate.
	pMusicInfo->dwSampleRate = ((static_cast<DWORD>(streamInfo[10])<<12))
					         + ((static_cast<DWORD>(streamInfo[11])<<4))
					         + ((static_cast<DWORD>(streamInfo[12])>>4)&0xF);

	// Next 3 bits is the number of channels, minus one.
	pMusicInfo->iNumChannels = ((static_cast<DWORD>(streamInfo[12])>>1)&0x7) + 1;

	// Next 5 bits are bits-per-sample, minus one. Opus doesn't report that so we ignore it.

	// Next 36 bits are the total number of samples (per channel), which we use to get the duration.
	unsigned __int64 ui64TotalSamples = (static_cast<unsigned __int64>(streamInfo[13]&0xF)<<32)
									  + (static_cast<unsigned __int64>(streamInfo[14])<<24)
									  + (static_cast<unsigned __int64>(streamInfo[15])<<16)
									  + (static_cast<unsigned __int64>(streamInfo[16])<<8)
									  + (static_cast<unsigned __int64>(streamInfo[17]));

	// Then there's the MD5 hash which we also ignore.

	// Calculate the duration if we know enough to do so.
	if (pMusicInfo->dwSampleRate != 0 && ui64TotalSamples != 0)
	{
		pMusicInfo->dwDuration = static_cast<DWORD>(ui64TotalSamples / pMusicInfo->dwSampleRate);
	}

	pMusicInfo->dwMusicFlags |= DVPMusicFlag_VBR; // FLAC is inherently VBR. Calculating the bitrate requires scanning the file, though.

	bGotAnything = true;
}

bool AudioTags_OggVorbis::ProcessMetaDataBlockPictureHeader(const BYTE *&pMB, size_t &mbLen, DWORD *pdwPictureType, DWORD *pdwPictureSizeBytes)
{
	// The MIME string could be "-->" which means the data is a URL, not a picture, but in that case it'll fail our later PNG/JPEG checks.
	DWORD dwTemp = 0;
	DWORD dwTemp2 = 0;

	if (pdwPictureSizeBytes != NULL // pdwPictureSizeBytes is not optional.
	&&	GetMemoryDWORDSwap(pMB, mbLen, pdwPictureType) // picture type (pdwPictureType may be null, which is fine)
	&&	GetMemoryDWORDSwap(pMB, mbLen, &dwTemp) && SkipMemoryBytes(pMB, mbLen, dwTemp) // MIME string
	&&	GetMemoryDWORDSwap(pMB, mbLen, &dwTemp) && SkipMemoryBytes(pMB, mbLen, dwTemp) // description string
	&&	GetMemoryDWORDSwap(pMB, mbLen, NULL) // width
	&&	GetMemoryDWORDSwap(pMB, mbLen, NULL) // height
	&&	GetMemoryDWORDSwap(pMB, mbLen, NULL) // depth bpp
	&&	GetMemoryDWORDSwap(pMB, mbLen, NULL) // num indexed colors
	&&	GetMemoryDWORDSwap(pMB, mbLen, pdwPictureSizeBytes) // Image data length.
	&&	*pdwPictureSizeBytes <= mbLen
	&&	*pdwPictureSizeBytes > 10)
	{
		return true;
	}

	if (pdwPictureType != NULL)
	{
		*pdwPictureType = 0xFFFFFFFF;
	}

	if (pdwPictureSizeBytes != NULL)
	{
		*pdwPictureSizeBytes = 0;
	}

	return false;
}

void AudioTags_OggVorbis::GiveImageToOpus(CoverArtHolder &coverArtHolder, HBITMAP *phBitmap, LPVIEWERPLUGINFILEINFOW lpVPFileInfo)
{
	if (coverArtHolder.bitmapMemory.HasHandle() && coverArtHolder.bitmapMemoryBytes > 10
	&&	phBitmap != NULL && *phBitmap == NULL
	&&	lpVPFileInfo != NULL && ((lpVPFileInfo->dwFlags)&(DVPFIF_JPEGStream|DVPFIF_PNGStream))==0)
	{
		BYTE * pBuf = reinterpret_cast< BYTE * >( coverArtHolder.bitmapMemory.Get() );

		if ((*reinterpret_cast< DWORD * >( pBuf + 0 ) == MAKE_DWORD_NAME(0xff,0xd8,0xff,0xe0)
		&&	 *reinterpret_cast< DWORD * >( pBuf + 6 ) == MAKE_DWORD_NAME('J','F','I','F'))
		||	(*reinterpret_cast< DWORD * >( pBuf + 0 ) == MAKE_DWORD_NAME(0xff,0xd8,0xff,0xe1)
		&&	 *reinterpret_cast< DWORD * >( pBuf + 6 ) == MAKE_DWORD_NAME('E','x','i','f')))
		{
			(*phBitmap) = reinterpret_cast< HBITMAP >( pBuf ); // Return the LocalAlloc(FIXED) JPEG image buffer to Opus for decoding.
			lpVPFileInfo->dwFlags |= DVPFIF_JPEGStream; // Tell Opus the HBITMAP is really a JPEG buffer.
			coverArtHolder.bitmapMemory.Forget(); // Opus owns the memory now so we should not free it.
		}
		else
		if (*reinterpret_cast< DWORD * >( pBuf + 0 ) == MAKE_DWORD_NAME(137,'P','N','G')
		&&	*reinterpret_cast< DWORD * >( pBuf + 4 ) == MAKE_DWORD_NAME(13,10,26,10))
		{
			(*phBitmap) = reinterpret_cast< HBITMAP >( pBuf ); // Return the LocalAlloc(FIXED) PNG image buffer to Opus for decoding.
			lpVPFileInfo->dwFlags |= DVPFIF_PNGStream; // Tell Opus the HBITMAP is really a PNG buffer.
			coverArtHolder.bitmapMemory.Forget(); // Opus owns the memory now so we should not free it.
		}
	}
	// Whatever happened, make sure nothing else tries the buffer or size, if it still belongs to the object.
	coverArtHolder.bitmapMemory.Free();
	coverArtHolder.bitmapMemoryBytes = 0;
}

static const DWORD S_OggCrcLookup[256] =
{
	0x00000000,0x04c11db7,0x09823b6e,0x0d4326d9,0x130476dc,0x17c56b6b,0x1a864db2,0x1e475005,
	0x2608edb8,0x22c9f00f,0x2f8ad6d6,0x2b4bcb61,0x350c9b64,0x31cd86d3,0x3c8ea00a,0x384fbdbd,
	0x4c11db70,0x48d0c6c7,0x4593e01e,0x4152fda9,0x5f15adac,0x5bd4b01b,0x569796c2,0x52568b75,
	0x6a1936c8,0x6ed82b7f,0x639b0da6,0x675a1011,0x791d4014,0x7ddc5da3,0x709f7b7a,0x745e66cd,
	0x9823b6e0,0x9ce2ab57,0x91a18d8e,0x95609039,0x8b27c03c,0x8fe6dd8b,0x82a5fb52,0x8664e6e5,
	0xbe2b5b58,0xbaea46ef,0xb7a96036,0xb3687d81,0xad2f2d84,0xa9ee3033,0xa4ad16ea,0xa06c0b5d,
	0xd4326d90,0xd0f37027,0xddb056fe,0xd9714b49,0xc7361b4c,0xc3f706fb,0xceb42022,0xca753d95,
	0xf23a8028,0xf6fb9d9f,0xfbb8bb46,0xff79a6f1,0xe13ef6f4,0xe5ffeb43,0xe8bccd9a,0xec7dd02d,
	0x34867077,0x30476dc0,0x3d044b19,0x39c556ae,0x278206ab,0x23431b1c,0x2e003dc5,0x2ac12072,
	0x128e9dcf,0x164f8078,0x1b0ca6a1,0x1fcdbb16,0x018aeb13,0x054bf6a4,0x0808d07d,0x0cc9cdca,
	0x7897ab07,0x7c56b6b0,0x71159069,0x75d48dde,0x6b93dddb,0x6f52c06c,0x6211e6b5,0x66d0fb02,
	0x5e9f46bf,0x5a5e5b08,0x571d7dd1,0x53dc6066,0x4d9b3063,0x495a2dd4,0x44190b0d,0x40d816ba,
	0xaca5c697,0xa864db20,0xa527fdf9,0xa1e6e04e,0xbfa1b04b,0xbb60adfc,0xb6238b25,0xb2e29692,
	0x8aad2b2f,0x8e6c3698,0x832f1041,0x87ee0df6,0x99a95df3,0x9d684044,0x902b669d,0x94ea7b2a,
	0xe0b41de7,0xe4750050,0xe9362689,0xedf73b3e,0xf3b06b3b,0xf771768c,0xfa325055,0xfef34de2,
	0xc6bcf05f,0xc27dede8,0xcf3ecb31,0xcbffd686,0xd5b88683,0xd1799b34,0xdc3abded,0xd8fba05a,
	0x690ce0ee,0x6dcdfd59,0x608edb80,0x644fc637,0x7a089632,0x7ec98b85,0x738aad5c,0x774bb0eb,
	0x4f040d56,0x4bc510e1,0x46863638,0x42472b8f,0x5c007b8a,0x58c1663d,0x558240e4,0x51435d53,
	0x251d3b9e,0x21dc2629,0x2c9f00f0,0x285e1d47,0x36194d42,0x32d850f5,0x3f9b762c,0x3b5a6b9b,
	0x0315d626,0x07d4cb91,0x0a97ed48,0x0e56f0ff,0x1011a0fa,0x14d0bd4d,0x19939b94,0x1d528623,
	0xf12f560e,0xf5ee4bb9,0xf8ad6d60,0xfc6c70d7,0xe22b20d2,0xe6ea3d65,0xeba91bbc,0xef68060b,
	0xd727bbb6,0xd3e6a601,0xdea580d8,0xda649d6f,0xc423cd6a,0xc0e2d0dd,0xcda1f604,0xc960ebb3,
	0xbd3e8d7e,0xb9ff90c9,0xb4bcb610,0xb07daba7,0xae3afba2,0xaafbe615,0xa7b8c0cc,0xa379dd7b,
	0x9b3660c6,0x9ff77d71,0x92b45ba8,0x9675461f,0x8832161a,0x8cf30bad,0x81b02d74,0x857130c3,
	0x5d8a9099,0x594b8d2e,0x5408abf7,0x50c9b640,0x4e8ee645,0x4a4ffbf2,0x470cdd2b,0x43cdc09c,
	0x7b827d21,0x7f436096,0x7200464f,0x76c15bf8,0x68860bfd,0x6c47164a,0x61043093,0x65c52d24,
	0x119b4be9,0x155a565e,0x18197087,0x1cd86d30,0x029f3d35,0x065e2082,0x0b1d065b,0x0fdc1bec,
	0x3793a651,0x3352bbe6,0x3e119d3f,0x3ad08088,0x2497d08d,0x2056cd3a,0x2d15ebe3,0x29d4f654,
	0xc5a92679,0xc1683bce,0xcc2b1d17,0xc8ea00a0,0xd6ad50a5,0xd26c4d12,0xdf2f6bcb,0xdbee767c,
	0xe3a1cbc1,0xe760d676,0xea23f0af,0xeee2ed18,0xf0a5bd1d,0xf464a0aa,0xf9278673,0xfde69bc4,
	0x89b8fd09,0x8d79e0be,0x803ac667,0x84fbdbd0,0x9abc8bd5,0x9e7d9662,0x933eb0bb,0x97ffad0c,
	0xafb010b1,0xab710d06,0xa6322bdf,0xa2f33668,0xbcb4666d,0xb8757bda,0xb5365d03,0xb1f740b4
};

DWORD AudioTags_OggVorbis::oggCrc32(DWORD result, void *ptr, size_t len)
{
	size_t i;
    BYTE * data = reinterpret_cast< BYTE * >( ptr );

	for(i = 0; i < len; ++i)
	{
		result = (result<<8) ^ S_OggCrcLookup[ ((result >> 24)&0xff) ^ data[i] ];
	}

	return result;
}

BOOL AudioTags_OggVorbis::GetFileInfo(AudioTagsConfig &config,
									  LeoHelpers::FileAndStream &fas,
									  bool bFromFile,
									  LPVIEWERPLUGINFILEINFOW lpVPFileInfo,
									  LPDVPFILEINFOMUSICW pMusicInfo,
									  HBITMAP *phBitmap)
{
	assert(lpVPFileInfo != NULL && (pMusicInfo != NULL || phBitmap != NULL));

	bool bGotAnything = false;

	CoverArtHolder coverArtHolder;

	std::vector< VorbisCommentSearchParams > searchVec;

	if (pMusicInfo != NULL)
	{
		if (pMusicInfo->lpszAlbum   != NULL && pMusicInfo->cchAlbumMax   > 0) { searchVec.push_back(VorbisCommentSearchParams("ALBUM",   pMusicInfo->lpszAlbum,    pMusicInfo->cchAlbumMax,   false, NULL, NULL, false)); }
		if (pMusicInfo->lpszArtist  != NULL && pMusicInfo->cchArtistMax  > 0) { searchVec.push_back(VorbisCommentSearchParams("ARTIST",  pMusicInfo->lpszArtist,   pMusicInfo->cchArtistMax,  false, NULL, NULL, false)); }
		if (pMusicInfo->lpszTitle   != NULL && pMusicInfo->cchTitleMax   > 0) { searchVec.push_back(VorbisCommentSearchParams("TITLE",   pMusicInfo->lpszTitle,    pMusicInfo->cchTitleMax,   false, NULL, NULL, false)); }
		if (pMusicInfo->lpszGenre   != NULL && pMusicInfo->cchGenreMax   > 0) { searchVec.push_back(VorbisCommentSearchParams("GENRE",   pMusicInfo->lpszGenre,    pMusicInfo->cchGenreMax,   false, NULL, NULL, false)); }
		if (pMusicInfo->lpszComment != NULL && pMusicInfo->cchCommentMax > 0) { searchVec.push_back(VorbisCommentSearchParams("COMMENT", pMusicInfo->lpszComment,  pMusicInfo->cchCommentMax, false, NULL, NULL, false)); }
		if (pMusicInfo->lpszEncoder != NULL && pMusicInfo->cchEncoderMax > 0) { searchVec.push_back(VorbisCommentSearchParams("ENCODER", pMusicInfo->lpszEncoder,  pMusicInfo->cchEncoderMax, true,  NULL, NULL, false)); }
		
		searchVec.push_back(VorbisCommentSearchParams("TRACKNUMBER", NULL, NULL, true, &pMusicInfo->iTrackNum, NULL, false)); // Prefer TRACKNUMBER over TRACK if we have both.
		searchVec.push_back(VorbisCommentSearchParams("TRACK",       NULL, NULL, true, &pMusicInfo->iTrackNum, NULL, false));
		searchVec.push_back(VorbisCommentSearchParams("YEAR",        NULL, NULL, true, &pMusicInfo->iYear,     NULL, false)); // Prefer YEAR over DATE if we have both.
		searchVec.push_back(VorbisCommentSearchParams("DATE",        NULL, NULL, true, &pMusicInfo->iYear,     NULL, false));
		searchVec.push_back(VorbisCommentSearchParams("BPM",         NULL, NULL, true, reinterpret_cast< int * >(&pMusicInfo->dwBPM), NULL, false));
	}

	if (searchVec.empty())
	{
		return FALSE; // No information for us to extract.
	}

	bool bIsVorbis = false; // Vorbis, if in a file at all, is almost always in an Ogg container and can't have tags without Ogg. (Can still have sample rate, etc. outside of Ogg, of course, but that is so uncommon we don't bother with it.)
	bool bIsFlac = false; // FLAC is usually in its own container format but sometimes also appears within an Ogg container (e.g. to make it more suitable for streaming).

	// FLAC-in-Ogg is described here: http://flac.sourceforge.net/ogg_mapping.html

	Win32IOWrapper *pfw = Win32IOWrapper::CreateFromFAS(&fas, false); // We do not need seeking.

	if (pfw != NULL)
	{
		// We will read (at most) the last 64k of the file when scanning for the final page.
		// We need to know that amount up-front so that the buffer we allocate is big enough, and can be used for other stuff before them.
		// Unless there's something unexpected at the end of the file, 65536 bytes should be enough to contin the final page (max size 65307 bytes)
		// especially when the test files I've seen have lots of small ~4k pages at the end.
		long lEndReadSize = 64*1024;
		LeoHelpers::AutoBuffer< BYTE > pageBuffer;
		pageBuffer.AllocateBytes(max(OGGPAGEHEADER_MAXSIZE,lEndReadSize));

		OggPageHeader *pOggHeader = reinterpret_cast< OggPageHeader * >( pageBuffer.GetBuffer() );

		union
		{
			VorbisHeader  vorbisHeader;
			FlacOggHeader flacHeader;
		} codecHeader;

		// We work out which header we should check for based on its size so it's important that the sizes actually differ.
		// If we need to support two formats with equal-sized headers then we'll have to change the logic to load some and
		// inspect it to decide which format is it.
		assert(sizeof(codecHeader.vorbisHeader) != sizeof(codecHeader.flacHeader));

		std::vector< BYTE > vorbisComments;

		// Data in Ogg containers is split into "pages" which contain "segements".
		// There must be at least two pages in a valid Ogg Vorbis file (so the first cannot be the last).
		// The first page must have one segment, which must be the vorbis header.
		// The second page's first segment must be the start of the mandatory vorbis comments structure.
		// The vorbis comments structure may be large enough to span multiple segments or even multiple pages.

		if (readOggPageHeader(pfw, pOggHeader)
		&&	((pOggHeader->chFlags&(OGG_PAGE_FLAG_FIRST|OGG_PAGE_FLAG_CONTINUATION|OGG_PAGE_FLAG_LAST)) == OGG_PAGE_FLAG_FIRST)
		&&	pOggHeader->chSegmentCount == 1)
		{
			if (pOggHeader->SegmentSizes[0] == sizeof(codecHeader.vorbisHeader))
			{
				VorbisHeader &vorbisHeader = codecHeader.vorbisHeader; // To simplify this code block.

				if (sizeof(vorbisHeader) == pfw->read(&vorbisHeader, sizeof(vorbisHeader))
				&&	vorbisHeader.chID == 1
				&&	0 == memcmp(vorbisHeader.Signature,"vorbis",6)
				&&	vorbisHeader.chVersion == VORBIS_VERSION
				&&	vorbisHeader.chFramingFlag == 1)
				{
					bIsVorbis = true;

					if (pMusicInfo != NULL)
					{
						if (pMusicInfo->lpszFormat != NULL && pMusicInfo->cchFormatMax > 0)
						{
							LeoHelpers::StringCopy(pMusicInfo->lpszFormat, L"Ogg Vorbis", pMusicInfo->cchFormatMax);
						}

						pMusicInfo->iNumChannels = vorbisHeader.chChannels;
						pMusicInfo->dwSampleRate = vorbisHeader.dwSampleRate;

						pMusicInfo->dwMusicFlags |= DVPMusicFlag_VBR; // Vorbis is inherently VBR.

						if (vorbisHeader.dwBitRateNominal != 0)
						{
							pMusicInfo->dwBitRate = vorbisHeader.dwBitRateNominal;
						}
						else if (vorbisHeader.dwBitRateMaximum != 0)
						{
							if (vorbisHeader.dwBitRateMinimum != 0)
							{
								pMusicInfo->dwBitRate = (vorbisHeader.dwBitRateMaximum + vorbisHeader.dwBitRateMinimum) / 2;
							}
							else
							{
								pMusicInfo->dwBitRate = vorbisHeader.dwBitRateMaximum;
							}
						}
						else if (vorbisHeader.dwBitRateMinimum != 0)
						{
							pMusicInfo->dwBitRate = vorbisHeader.dwBitRateMinimum;
						}

						// FWIW, if the input could seek then the old plugin would calculate the real average bitrate by scanning the whole
						// file. Just reporting the nominal bitrate for now, though...

						bGotAnything = true;
					}
				}
			}
			else if (pOggHeader->SegmentSizes[0] == sizeof(codecHeader.flacHeader))
			{
				FlacOggHeader &flacHeader = codecHeader.flacHeader; // To simplify this code block.

				if (sizeof(flacHeader) == pfw->read(&flacHeader, sizeof(flacHeader))
				&&	flacHeader.chPacketType == 0x7f
				&&	flacHeader.dwSignature1 == MAKE_DWORD_NAME('F','L','A','C')
				&&	flacHeader.chMappingVersionMajor == 1
				&&	flacHeader.dwSignature2 == MAKE_DWORD_NAME('f','L','a','C')
				&&	flacHeader.dwStreamInfoHeader == 0x22000000)
				{
					bIsFlac = true;

					if (pMusicInfo != NULL)
					{
						ProcessFlacStreamInfo(flacHeader.StreamInfo, pMusicInfo, bGotAnything);

						if (pMusicInfo->lpszFormat != NULL && pMusicInfo->cchFormatMax > 0)
						{
							LeoHelpers::StringCopy(pMusicInfo->lpszFormat, L"Ogg FLAC", pMusicInfo->cchFormatMax);
						}
					}
				}
			}
		}

		BYTE currentSegment = 0;

		if ((bIsVorbis || bIsFlac)
		&&	readOggPageHeader(pfw, pOggHeader) // read second page header in
		&&	((pOggHeader->chFlags&(OGG_PAGE_FLAG_FIRST|OGG_PAGE_FLAG_CONTINUATION)) == 0) // It could be the last (as far as we're concerned anyway)
		&&	pOggHeader->chSegmentCount > 0
		&&	pOggHeader->SegmentSizes[0] >= (bIsFlac ? FLAC_VORBISCOMMENTS_MINSIZE : VORBISCOMMENTS_MINSIZE)
		&&	readNextDataChunkFromOggStream(pfw, pOggHeader, currentSegment, &vorbisComments, bIsVorbis, bIsFlac, bIsFlac)) // Do not require granule zero for non-FLAC streams. Some non-FLAC encoders put -1 there.
		{
			const DWORD dwBitstreamSerialNumber = pOggHeader->dwBitstreamSerial;

			// Read segments (and further pages if needed) until we have all of (what should be) the vorbis comments structure loaded.

			if (vorbisComments.size() > 0)
			{
				// The header's ID and signature have already been checked, and it's at least sizeof(VorbisCommentsHeaderStart)
				const size_t skipBytesToEncoderLength = ( bIsFlac ? 4 : (sizeof(VorbisCommentsHeaderStart) - sizeof(DWORD)) );
				const BYTE *pCommentData = reinterpret_cast< const BYTE * >( &(vorbisComments[0]) ) + skipBytesToEncoderLength;
				size_t commentBytesLeft  = vorbisComments.size() - skipBytesToEncoderLength;

				ParseVorbisCommentMemory(pMusicInfo, pCommentData, commentBytesLeft, searchVec, bGotAnything);
			}

			bool bErrorReadingToEndOfMetaData = false;

			if (bIsFlac && vorbisComments.size() > sizeof(DWORD)) // Should be impossible for the size test to fail here.
			{
				const bool bWantBitrate = (pMusicInfo != NULL && pMusicInfo->dwDuration != 0);
				bool bWantPics = (phBitmap != NULL && lpVPFileInfo != NULL);

				// For Ogg FLAC files there may be further metadata chunks which we must read.
				// This is to get the cover art (since it's not in the VorbisComments like with Ogg Vorbis) and to
				// be able to more accurately estimate the bitrate (since we don't want to include metadata in
				// that calculation and thus need to find where it ends).

				std::vector< BYTE > dataChunk;

				DWORD dwCurrentHeader = LeoHelpers::SwapDWORD( *reinterpret_cast< const DWORD * >(&(vorbisComments[0])) );

				while ((bWantBitrate || bWantPics) && (dwCurrentHeader&0x80000000)==0
				&&     readNextDataChunkFromOggStream(pfw, pOggHeader, currentSegment, &dataChunk, false, false, true)) // Stops once the granule is non-zero.
				{
					if (dataChunk.size() < sizeof(DWORD))
					{
						bErrorReadingToEndOfMetaData = true;
						break;
					}

					dwCurrentHeader = LeoHelpers::SwapDWORD( *reinterpret_cast< const DWORD * >(&(dataChunk[0])) );

					const DWORD dwBlockType = (dwCurrentHeader&0x7F000000); // Shift the constants up 24 instead of shifting this down.
					const DWORD dwBlockSize = (dwCurrentHeader&0x00FFFFFF);

					if (dwBlockType == (0x00<<24)
					||	dwBlockType == (0x7F<<24)
					||	(dataChunk.size() - sizeof(DWORD)) != dwBlockSize)
					{
						bErrorReadingToEndOfMetaData = true;
						break;
					}
				}
			}

			vorbisComments.clear(); // Free this memory up early.

			unsigned __int64 ui64TotalSize = 0; 
			unsigned __int64 ui64PosAfterMetaData = 0; 

			// Get or estimate the duration. First sanity check the remaining file size.

			if (!bErrorReadingToEndOfMetaData
			&&	pMusicInfo != NULL
			&&	pfw->size64(&ui64TotalSize)
			&&	pfw->tell64(&ui64PosAfterMetaData)
			&&	ui64PosAfterMetaData < ui64TotalSize)
			{
				// If we're loading from a file then we can quickly get the accurate duration via the sample number of the final page.
				// We could read and parse the entire file until we get to the final page but it's a lot quicker to jump to the end of
				// the file and find the last page by searching for "OggS" and then verifying the CRC (to avoid matching a random occurace
				// of "OggS" in other data).
				if (pMusicInfo->dwDuration == 0 && pMusicInfo->dwSampleRate > 0 && bFromFile
				&&	OGGPAGEHEADER_MINSIZE <= (ui64TotalSize - ui64PosAfterMetaData))
				{
					assert(lEndReadSize >= OGGPAGEHEADER_MINSIZE);
					assert(pageBuffer.GetSizeBytes() >= static_cast<unsigned __int64>(lEndReadSize));

					if (lEndReadSize > (ui64TotalSize - ui64PosAfterMetaData))
					{
						// There's less than 64k left so just read the remaining data from the file.
						lEndReadSize = static_cast<long>(ui64TotalSize - ui64PosAfterMetaData); // We know it'll fit in a "long" as it's <64k
					}
					else if (0 != pfw->seek(-lEndReadSize, SEEK_END)) // There's more than 64k left so we need to seek to 64k from the end.
					{
						lEndReadSize = 0; // error
					}

					if (lEndReadSize > OGGPAGEHEADER_MINSIZE && pfw->read(pageBuffer.GetBuffer(), lEndReadSize))
					{
						assert(sizeof(pageBuffer.GetBuffer()[0]) == sizeof(BYTE));

						size_t endIdx = (lEndReadSize - OGGPAGEHEADER_MINSIZE);

						while(true)
						{
							OggPageHeader *pCandidateHeader = reinterpret_cast< OggPageHeader * >(pageBuffer.GetBuffer() + endIdx);

							if (isValidOggPage(pCandidateHeader, lEndReadSize - endIdx, dwBitstreamSerialNumber))
							{
								// Alrighty, this is a real ogg page header, or we're extremely unlucky. :-)
								unsigned __int64 ui64Duration = pCandidateHeader->ui64GranulePosition / pMusicInfo->dwSampleRate;
								if (ui64Duration > 0
								&&	ui64Duration <= 0xFFFFFFFF)
								{
									pMusicInfo->dwDuration = static_cast<DWORD>(ui64Duration);
									break;
								}
							}

							if (endIdx == 0) // Slightly unusual loop to avoid "for (endIdx = blah; endIdx >= 0; --endIdx)" which b0rks due to unsigned wrap-around.
							{
								break;
							}
							--endIdx;
						}
					}
				}

				// If we didn't or couldn't get the duration based on the last page, estimate it from the bitrate and the file size.
				if (pMusicInfo->dwBitRate > 0 && pMusicInfo->dwDuration == 0)
				{
					// There's little point trying to be clever here (e.g. accounting for the ogg page headers) because we'll be
					// off anyway. Doing it the naive way (which is the same as what the example Ogg Vorbis code does) produces
					// numbers that are both over- and under-estimates so it's about as good as we can get. (Actually, the example
					// code may use the total file size rather than the size-after-metadata. Doing this way seems slightly better.)
					pMusicInfo->dwDuration = static_cast<DWORD>( ((ui64TotalSize - ui64PosAfterMetaData) * 8) / pMusicInfo->dwBitRate );

					pMusicInfo->dwMusicFlags |= DVPMusicFlag_DurationInaccurate;
				}

				// If we a duration but not a bitrate, estimate things the other way around. (This seems common with Speex files.)
				if (pMusicInfo->dwDuration > 0 && pMusicInfo->dwBitRate == 0)
				{
					// Multiply by 8 as its bits per second.
					pMusicInfo->dwBitRate = static_cast< DWORD >( ((ui64TotalSize - ui64PosAfterMetaData) * 8) / pMusicInfo->dwDuration );
				}
			}
		}

		delete pfw;
		pfw = NULL;
	}

	if (bGotAnything && phBitmap != NULL && lpVPFileInfo != NULL)
	{
		// If we got a COVERART art directly, or converted the METADATA_BLOCK_PICTURE tag, then process the image.
		GiveImageToOpus(coverArtHolder, phBitmap, lpVPFileInfo);
	}

	return (bGotAnything ? TRUE : FALSE);
}
