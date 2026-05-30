#include "stdafx.h"
#include "LeoHelpers.h"
#include "Win32IOWrapper.h"
#include "audiotags_config.h"
#include "audiotags_oggvorbis.h"
#include "audiotags_flac.h"

// Minimal, forward-read-only FLAC metadata extractor. (C) Leo Davidson 2009.

BOOL AudioTags_FLAC::GetFileInfo(AudioTagsConfig &config,
								 LeoHelpers::FileAndStream &fas,
								 bool bFromFile,
								 LPVIEWERPLUGINFILEINFOW lpVPFileInfo,
								 LPDVPFILEINFOMUSICW pMusicInfo,
								 HBITMAP *phBitmap)
{
	assert(lpVPFileInfo != NULL && (pMusicInfo != NULL || phBitmap != NULL));

	bool bGotAnything = false;

	AudioTags_OggVorbis::CoverArtHolder coverArtHolder;

	std::vector< AudioTags_OggVorbis::VorbisCommentSearchParams > searchVec;

	if (pMusicInfo != NULL)
	{
		if (pMusicInfo->lpszAlbum   != NULL && pMusicInfo->cchAlbumMax   > 0) { searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("ALBUM",   pMusicInfo->lpszAlbum,    pMusicInfo->cchAlbumMax,   false, NULL, NULL, false)); }
		if (pMusicInfo->lpszArtist  != NULL && pMusicInfo->cchArtistMax  > 0) { searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("ARTIST",  pMusicInfo->lpszArtist,   pMusicInfo->cchArtistMax,  false, NULL, NULL, false)); }
		if (pMusicInfo->lpszTitle   != NULL && pMusicInfo->cchTitleMax   > 0) { searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("TITLE",   pMusicInfo->lpszTitle,    pMusicInfo->cchTitleMax,   false, NULL, NULL, false)); }
		if (pMusicInfo->lpszGenre   != NULL && pMusicInfo->cchGenreMax   > 0) { searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("GENRE",   pMusicInfo->lpszGenre,    pMusicInfo->cchGenreMax,   false, NULL, NULL, false)); }
		if (pMusicInfo->lpszComment != NULL && pMusicInfo->cchCommentMax > 0) { searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("COMMENT", pMusicInfo->lpszComment,  pMusicInfo->cchCommentMax, false, NULL, NULL, false)); }
		if (pMusicInfo->lpszEncoder != NULL && pMusicInfo->cchEncoderMax > 0) { searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("ENCODER", pMusicInfo->lpszEncoder,  pMusicInfo->cchEncoderMax, true,  NULL, NULL, false)); }
		
		searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("TRACKNUMBER", NULL, NULL, true, &pMusicInfo->iTrackNum, NULL, false)); // Prefer TRACKNUMBER over TRACK if we have both.
		searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("TRACK",       NULL, NULL, true, &pMusicInfo->iTrackNum, NULL, false));
		searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("YEAR",        NULL, NULL, true, &pMusicInfo->iYear,     NULL, false)); // Prefer YEAR over DATE if we have both.
		searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("DATE",        NULL, NULL, true, &pMusicInfo->iYear,     NULL, false));
		searchVec.push_back(AudioTags_OggVorbis::VorbisCommentSearchParams("BPM",         NULL, NULL, true, reinterpret_cast< int * >(&pMusicInfo->dwBPM), NULL, false));
	}

	if (phBitmap == NULL && searchVec.empty())
	{
		return FALSE; // No information for us to extract.
	}

	Win32IOWrapper *pfw = Win32IOWrapper::CreateFromFAS(&fas, false); // We do not need seeking.

	if (pfw != NULL)
	{
		DWORD dwTemp = 0;
		DWORD dwCurrentHeader = 0;

		BYTE streamInfo[34];

		LeoHelpers::AutoBuffer< BYTE > blockBuffer;

		if (pfw->readValue(&dwTemp)
		&&	dwTemp == MAKE_DWORD_NAME('f','L','a','C')
		&&	pfw->readValueSwap(&dwCurrentHeader) // DWORD containing METADATA_BLOCK_HEADER for the METADATA_BLOCK_STEAMINFO
		&&	(dwCurrentHeader&0x7FFFFFFF)==34   // Checks that block is a METADATA_BLOCK_STREAMINFO: type 0 and length 34
		&&	sizeof(streamInfo) == pfw->read(streamInfo, sizeof(streamInfo))) // Read STREAMINFO block.
		{
			if (pMusicInfo != NULL)
			{
				AudioTags_OggVorbis::ProcessFlacStreamInfo(streamInfo, pMusicInfo, bGotAnything);
			}

			const bool bWantBitrate = (pMusicInfo != NULL && pMusicInfo->dwDuration != 0);
			bool bWantTags = (pMusicInfo != NULL);
			bool bWantPics = (phBitmap   != NULL && lpVPFileInfo != NULL);

			bool bError = false;

			// If we're filling in pMusicInfo then we need to read to the end of the metadata blocks even after
			// we've parsed the blocks we're interested in. That is so that we can estimate the bitrate based
			// on the remaining size of the file.
			// The top bit in dwCurrentHeader will be set after we've processed the last block (which could be the one
			// we processed before entering the loop, which we handle).
			while((bWantBitrate || bWantTags || bWantPics) && (dwCurrentHeader&0x80000000)==0)
			{
				if (!pfw->readValueSwap(&dwCurrentHeader)) // DWORD containing METADATA_BLOCK_HEADER for the next header.
				{
					bError = true;
					break;
				}

				const DWORD dwBlockType = (dwCurrentHeader&0x7F000000); // Shift the constants up 24 instead of shifting this down.
				const DWORD dwBlockSize = (dwCurrentHeader&0x00FFFFFF);

				if (bWantTags && dwBlockType == (4<<24))
				{
					bWantTags = false;

					// METADATA_BLOCK_VORBIS_COMMENT
					if (!blockBuffer.AllocateMinimumBytes(dwBlockSize)
					||	dwBlockSize != pfw->read(blockBuffer.GetBuffer(), dwBlockSize))
					{
						bError = true;
						break;
					}

					AudioTags_OggVorbis::ParseVorbisCommentMemory(pMusicInfo, blockBuffer.GetBuffer(), dwBlockSize, searchVec, bGotAnything);
				}
				else if (bWantPics && dwBlockType == (6<<24))
				{
					// METADATA_BLOCK_PICTURE
					if (!blockBuffer.AllocateMinimumBytes(dwBlockSize)
					||	dwBlockSize != pfw->read(blockBuffer.GetBuffer(), dwBlockSize))
					{
						bError = true;
						break;
					}

					const BYTE *pMB = blockBuffer.GetBuffer();
					size_t mbLen = dwBlockSize;

					const DWORD dwFrontCoverType = 3;

					DWORD dwPictureType = 0xFFFFFFFF;
					DWORD dwPictureSizeBytes = 0;

					if (!AudioTags_OggVorbis::ProcessMetaDataBlockPictureHeader(pMB, mbLen, &dwPictureType, &dwPictureSizeBytes))
					{
						bError = true;
						break;
					}

					// If we don't already have an image, or we have a non-cover image and the new one is a cover, take the new image.
					if (!coverArtHolder.bitmapMemory.HasHandle() || (coverArtHolder.dwBitmapType != dwFrontCoverType && dwPictureType == dwFrontCoverType))
					{
						coverArtHolder.bitmapMemoryBytes = 0;
						coverArtHolder.dwBitmapType = 0xFFFFFFFF;
						if (!coverArtHolder.bitmapMemory.AllocateFixed(dwPictureSizeBytes)) // Frees the old buffer, if any.
						{
							bError = true;
							break;
						}
						
						memcpy(reinterpret_cast< BYTE * >( coverArtHolder.bitmapMemory.Get() ), pMB, dwPictureSizeBytes);
						coverArtHolder.bitmapMemoryBytes = dwPictureSizeBytes;
						coverArtHolder.dwBitmapType = dwPictureType;

						bGotAnything = true;
					}

					if (coverArtHolder.dwBitmapType == dwFrontCoverType)
					{
						// If we got a front cover then we should skip/ignore all further images.
						bWantPics = false;
					}
				}
				else
				{
					// Some other block that we'll just skip.
					if (0 != pfw->seekOrReadForward(dwBlockSize)) // dwBlockSize cannot be large enough that it goes negative when turned into a long.
					{
						bError = true;
						break;
					}
				}
			}

			if (!bError && pMusicInfo != NULL && pMusicInfo->dwDuration != 0)
			{
				// Estimate the bitrate based on the duration and the length, in bytes, of the rest of the file.
				// This estimate seems to tie up pretty well with what Foobar2000 reports, though I'm not sure how
				// accurate that is as sometimes it reports higher numbers, which should be impossible (i.e. it
				// is presumably including some of the metadata in its calculation?)

				unsigned __int64 ui64Position = 0;
				unsigned __int64 ui64Size = 0;

				if (pfw->tell64(&ui64Position) && pfw->size64(&ui64Size) && ui64Position < ui64Size)
				{
					ui64Size -= ui64Position;
					// Multiply by 8 as its bits per second.
					pMusicInfo->dwBitRate = static_cast< DWORD >( (ui64Size * 8) / pMusicInfo->dwDuration );
				}
			}
		}

		delete pfw;
		pfw = NULL;
	}

	// pMusicInfo->lpszCodec exists but (at the time of writing) is never asked for or used by Opus.
	// It is the pMusicInfo->lpszFormat field which populates the "Audio Codec" field in Opus.
	if (bGotAnything && pMusicInfo != NULL && pMusicInfo->lpszFormat != NULL && pMusicInfo->cchFormatMax > 0 && pMusicInfo->lpszFormat[0]==L'\0')
	{
		LeoHelpers::StringCopy(pMusicInfo->lpszFormat, L"FLAC", pMusicInfo->cchFormatMax);
	}

	if (bGotAnything && phBitmap != NULL && lpVPFileInfo != NULL)
	{
		AudioTags_OggVorbis::GiveImageToOpus(coverArtHolder, phBitmap, lpVPFileInfo);
	}

	return (bGotAnything ? TRUE : FALSE);
}
