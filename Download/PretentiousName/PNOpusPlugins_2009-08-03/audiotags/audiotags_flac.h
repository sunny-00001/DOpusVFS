#pragma once

// Minimal, forward-read-only FLAC metadata extractor. (C) Leo Davidson 2009.

class AudioTags_FLAC
{
public:

	static BOOL GetFileInfo(AudioTagsConfig &config,
							LeoHelpers::FileAndStream &fas,
							bool bFromFile,
							LPVIEWERPLUGINFILEINFOW lpVPFileInfo,
							LPDVPFILEINFOMUSICW pMusicInfo,
							HBITMAP *phBitmap);
};
