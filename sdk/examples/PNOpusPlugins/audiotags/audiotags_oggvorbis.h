#pragma once

// Minimal, forward-read-only Ogg Vorbis/Ogg FLAC metadata extractor. (C) Leo Davidson 2009.

class AudioTags_OggVorbis
{
public:
	static BOOL GetFileInfo(AudioTagsConfig &config,
							LeoHelpers::FileAndStream &fas,
							bool bFromFile,
							LPVIEWERPLUGINFILEINFOW lpVPFileInfo,
							LPDVPFILEINFOMUSICW pMusicInfo,
							HBITMAP *phBitmap);

	struct CoverArtHolder
	{
		DWORD dwBitmapType;
		size_t bitmapMemoryBytes;
		LeoHelpers::LocalFreeScoper bitmapMemory;

		CoverArtHolder() : dwBitmapType(0xFFFFFFFF), bitmapMemoryBytes(0)
		{
		}
	};

	class VorbisCommentSearchParams
	{
	private:
		// Default copy and assignment must work for this.
		const char *m_szUpperKey;    // e.g. "ARTIST"
		size_t m_cchKeyLen;          // length of m_szUpperKey
		wchar_t *m_szCaptureBuffer;  // Usually into the structure we are filling in.
		size_t m_cchBufferLeft;      // Amount of space left in the buffer.
		wchar_t *m_szBufferOriginal; // Start of the buffer.
		size_t m_cchBufferOriginal;  // Amount of space in the buffer.
		bool m_bFirstMatchOnly;      // Ignored for non-string captures. If true then the buffer will only be written to or updated if it is empty. Intended for the ENCODER field/tag because some things don't use th special field and use a generic tag instead, but if both field and tag are used we don't want to return both.
		bool m_bFull;                // For string captures, true iff the buffer has already been truncated due to the input being too long. For int capures means a value has already been captured.
		int *m_piCaptureInt;
		CoverArtHolder *m_pCoverArtHolder;
		bool m_bExpectFlacPictureBlock; // If true then a base64-encoded METADATA_PICTURE_BLOCK is expected. If false then a base64-encoded standalone image is expected.

		void addToString(const char *newStringUnterminated, size_t cchNewStringLength);

	public:
		VorbisCommentSearchParams(const char *szUpperKey, wchar_t *szCaptureBuffer, size_t cchBufferLength, bool bFirstMatchOnly, int *piCaptureInt, CoverArtHolder *pCoverArtHolder, bool bExpectFlacPictureBlock)
		: m_szUpperKey(szUpperKey)
		, m_cchKeyLen(strlen(szUpperKey))
		, m_szCaptureBuffer(szCaptureBuffer)
		, m_cchBufferLeft(cchBufferLength)
		, m_szBufferOriginal(szCaptureBuffer)
		, m_cchBufferOriginal(cchBufferLength)
		, m_bFirstMatchOnly(bFirstMatchOnly)
		, m_bFull(false)
		, m_piCaptureInt(piCaptureInt)
		, m_pCoverArtHolder(pCoverArtHolder)
		, m_bExpectFlacPictureBlock(bExpectFlacPictureBlock)
		{
		}

		const char *GetUpperKey() const { return m_szUpperKey; }
		size_t GetKeyLen() const        { return m_cchKeyLen;  }

		bool Capture(const char *newStringUnterminated, size_t cchNewStringLength);
	};

	static void ParseVorbisCommentMemory(LPDVPFILEINFOMUSICW pMusicInfo,
										 const BYTE *pCommentData,
										 size_t commentBytesLeft,
										 std::vector< VorbisCommentSearchParams > &searchVec,
										 bool &bGotAnything);

	static void ProcessFlacStreamInfo(const BYTE *streamInfo, LPDVPFILEINFOMUSICW pMusicInfo, bool &bGotAnything);

	static bool ProcessMetaDataBlockPictureHeader(const BYTE *&pMB, size_t &mbLen, DWORD *pdwPictureType, DWORD *pdwPictureSizeBytes);

	static void GiveImageToOpus(CoverArtHolder &coverArtHolder, HBITMAP *phBitmap, LPVIEWERPLUGINFILEINFOW lpVPFileInfo);


	static inline bool GetMemoryDWORD(const BYTE *&pMB, size_t &mbLen, DWORD *pdwOut)
	{
		if (mbLen < sizeof(DWORD))
			return false;

		if (pdwOut)
		{
			*pdwOut = *reinterpret_cast< const DWORD * >(pMB);
		}

		pMB += sizeof(DWORD);
		mbLen -= sizeof(DWORD);

		return true;
	}


	static inline bool GetMemoryDWORDSwap(const BYTE *&pMB, size_t &mbLen, DWORD *pdwOut)
	{
		if (mbLen < sizeof(DWORD))
			return false;

		if (pdwOut)
		{
			*pdwOut = LeoHelpers::SwapDWORD( *reinterpret_cast< const DWORD * >(pMB) );
		}

		pMB += sizeof(DWORD);
		mbLen -= sizeof(DWORD);

		return true;
	}
	
	static inline bool SkipMemoryBytes(const BYTE *&pMB, size_t &mbLen, DWORD dwSkip)
	{
		if (mbLen < dwSkip)
			return false;

		pMB += dwSkip;
		mbLen -= dwSkip;

		return true;
	}

	static inline bool FindMemoryNullByte(const BYTE *pMB, size_t mbLen, const BYTE **ppNull)
	{
		const BYTE *pEnd = pMB + mbLen;

		while(pMB < pEnd)
		{
			if (*pMB == '\0')
			{
				if (ppNull != NULL)
				{
					*ppNull = pMB;
				}
				return true;
			}
			++pMB;
		}

		return false;
	}

private:

#pragma pack(push,1)

	struct OggPageHeader
	{
		DWORD            dwSignature;
		BYTE             chVersion;
		BYTE             chFlags;
		unsigned __int64 ui64GranulePosition;
		DWORD            dwBitstreamSerial;
		DWORD            dwPageSequence;
		DWORD            dwChecksum;
		BYTE             chSegmentCount;
		BYTE             SegmentSizes[1];
	};

#define OGGPAGEHEADER_MAXSIZE (sizeof(OggPageHeader) + 254)
#define OGGPAGEHEADER_MINSIZE (sizeof(OggPageHeader))
#define OGG_VERSION (0)
#define OGG_SIGNATURE (MAKE_DWORD_NAME('O','g','g','S'))
#define OGG_PAGE_FLAG_CONTINUATION (1<<0)
#define OGG_PAGE_FLAG_FIRST        (1<<1)
#define OGG_PAGE_FLAG_LAST         (1<<2)

	struct VorbisHeader // 30 bytes
	{
		BYTE  chID;
		BYTE  Signature[6];
		DWORD chVersion;
		BYTE  chChannels;
		DWORD dwSampleRate;
		DWORD dwBitRateMaximum;
		DWORD dwBitRateNominal;
		DWORD dwBitRateMinimum;
		BYTE  chBlocksizes; // 4 bits each
		BYTE  chFramingFlag;
	};

	struct VorbisCommentsHeaderStart // This only applies to actual Ogg Vorbis. FLAC and Speex have different data before the dwEncoderLength field.
	{
		BYTE  chID;
		BYTE  Signature[6];
		DWORD dwEncoderLength;
	};

#define VORBIS_VERSION (0)
#define VORBISCOMMENTS_MINSIZE (sizeof(VorbisCommentsHeaderStart) + sizeof(DWORD)) // extra DWORD is for string count. Count and encoder length could both be zero so this is the minimum.
#define FLAC_VORBISCOMMENTS_MINSIZE (2 * sizeof(DWORD) + 1) // FLAC vorbis comments have a 1 byte ID, then the dwEncoderLength part.

	struct FlacOggHeader
	{
		BYTE  chPacketType;          // should be 0x7F
		DWORD dwSignature1;          // Should be "FLAC" or "CALF" to avoid the DWORD swap.
		BYTE  chMappingVersionMajor; // should be 1
		BYTE  chMappingVersionMinor; // probably 0 but we should ignore.
		WORD  wNumberOfHeaders;      // Big-endian. Number of non-audio headers after this one and before the audio data. Maybe zero to signify "unknown".
		DWORD dwSignature2;          // Should be "fLaC" or "CaLf" to avoid the DWORD swap.
		DWORD dwStreamInfoHeader;    // METADATA_BLOCK_HEADER -- In the Ogg-FLAC header this should be 0x22, or 0x22000000 to avoid the DWORD swap.
		BYTE  StreamInfo[34];        // METADATA_BLOCK_STREAMINFO
	};

#pragma pack(pop)

	static DWORD oggCrc32(DWORD result, void *ptr, size_t len);

	static inline bool readOggPageHeader(Win32IOWrapper *pfw, OggPageHeader *pHeader)
	{
		if (sizeof(OggPageHeader) != pfw->read(pHeader, sizeof(OggPageHeader))
		||	pHeader->dwSignature != OGG_SIGNATURE
		||	pHeader->chVersion != OGG_VERSION
		||	pHeader->chSegmentCount == 0)
		{
			return false;
		}

		size_t extraRead = pHeader->chSegmentCount - 1;

		if (extraRead > 0
		&&	extraRead != pfw->read(reinterpret_cast< BYTE * >(pHeader) + sizeof(OggPageHeader), extraRead))
		{
			return false;

		}

		return true;
	}

	static inline bool isValidOggPage(OggPageHeader *pCandidateHeader, size_t candidateSize, DWORD dwBitstreamSerialNumber)
	{
		size_t candCalcSize = OGGPAGEHEADER_MINSIZE;

		if (candCalcSize <= candidateSize
		&&	pCandidateHeader->dwSignature == OGG_SIGNATURE
		&&	pCandidateHeader->chVersion   == OGG_VERSION
		&&	pCandidateHeader->dwBitstreamSerial == dwBitstreamSerialNumber
		&&	pCandidateHeader->chSegmentCount > 0)
		{
			candCalcSize += (pCandidateHeader->chSegmentCount - 1);

			if (candCalcSize <= candidateSize)
			{
				for(size_t segIdx = 0; segIdx < pCandidateHeader->chSegmentCount; ++segIdx)
				{
					candCalcSize += pCandidateHeader->SegmentSizes[segIdx];
				}

				if (candCalcSize <= candidateSize)
				{
					DWORD dwExpectedCRC = pCandidateHeader->dwChecksum;
					pCandidateHeader->dwChecksum = 0; // Checksum must be set to zero when calculating the checksum. We'll put it back afterwards.

					DWORD dwActualCRC = oggCrc32(0, pCandidateHeader, candCalcSize);

					pCandidateHeader->dwChecksum = dwExpectedCRC; // Put the original CRC back in case this isn't a header after all.

					if (dwActualCRC == dwExpectedCRC)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	static bool readNextDataChunkFromOggStream(Win32IOWrapper *pfw,
											   OggPageHeader *pOggHeader,
											   BYTE &currentSegment,
											   std::vector< BYTE > *pChunkData,
											   bool bCheckForVorbisComments,
											   bool bCheckForFlacComments,
											   bool bRequireGranuleZero);

	static bool getVorbisCommentString(std::string *pstrUtf8Out, const BYTE **ppCommentData, size_t *pCommentBytesLeft);

	static bool getVorbisCommentDWORD(DWORD *pdwOut, const BYTE **ppCommentData, size_t *pCommentBytesLeft);

	static bool processVorbisCommentString(std::vector< VorbisCommentSearchParams > &searchVec, size_t cchMaxKeyLen,
											const BYTE **ppCommentData, size_t *pCommentBytesLeft, bool *pbGotAnything);
};
