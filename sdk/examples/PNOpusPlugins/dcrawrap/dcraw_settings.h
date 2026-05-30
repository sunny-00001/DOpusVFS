#pragma once

struct DCR_RawSettings
{
	enum Purpose
	{
		DP_UNKNOWN = 0,
		DP_THUMBS,
		DP_VIEWERS,
		DP_CONVERTER
	};

	// Default copy-const/assignment-op must work or be overridden.
	// Any changes need to be reflected in save/loadXmlRawSettings.

	bool	bTryPreview;
	bool	bTryFull;
	int		iRotation;
	bool	bCorrectGeometry;
	int		iInterpQuality;
	bool	bDocModeNoCol;
	bool	bDocModeRaw;
	bool	bHalfSizeColor;
	bool	bRGGB;
	bool	bAutoWhite;
	bool	bCamWhite;
	bool	bSetWhite;
	int		iUserMul1;
	int		iUserMul2;
	int		iUserMul3;
	int		iUserMul4;
	int		iBrightness;
	bool	bFixedWhite;
	int		iHighlightMode;
	int		iGammaPower;
	int		iGammaToeSlope;
	int		iChromaRed;
	int		iChromaBlue;
	int		iOutIccType;
	std::wstring strOutIcc;
	int		iCamIccType;
	std::wstring strCamIcc;
	bool	bBadPixels;
	std::wstring strBadPixelsPath;
	bool	bNoiseFilter;
	int		iNoiseThreshold;
	bool	bMedianFilter;
	int		iMedianPasses;

	enum
	{
		GAMMA_BT709_POWER    = 2222,
		GAMMA_BT709_TOESLOPE = 4500,
		GAMMA_SRGB_POWER     = 2400,
		GAMMA_SRGB_TOESLOPE  = 12920
	};

	// The object created by the constructor must be valid without further changes.
	// It will be used as the default settings should a config be loaded that has no defaults.
	// It's also what things are compared against when they are saved and what things are built
	// upon when they are loaded (since our config only stores changes from the defaults in order
	// to allow "better" defaults to be inherited by all users when new versions of the DLL go out).
	DCR_RawSettings(Purpose purp)
	: bTryPreview((purp == DP_CONVERTER) ? false : true)
	, bTryFull((purp == DP_THUMBS) ? false : true)
	, iRotation(-1)
	, bCorrectGeometry(true)
	, iInterpQuality(-1)
	, bDocModeNoCol(false)
	, bDocModeRaw(false)
	, bHalfSizeColor((purp == DP_CONVERTER) ? false : true)
	, bRGGB(false)
	, bAutoWhite(false)
	, bCamWhite(false)
	, bSetWhite(false)
	, iUserMul1(1000)
	, iUserMul2(500)
	, iUserMul3(1000)
	, iUserMul4(500)
	, iBrightness(1000)
	, bFixedWhite(false)
	, iHighlightMode(0)
	, iGammaPower(GAMMA_BT709_POWER)
	, iGammaToeSlope(GAMMA_BT709_TOESLOPE)
	, iChromaRed(1000)
	, iChromaBlue(1000)
	, iOutIccType(1)
	, iCamIccType(0)
	, bBadPixels(false)
	, bNoiseFilter(false)
	, iNoiseThreshold(500)
	, bMedianFilter(false)
	, iMedianPasses(1)
	{
	}
};
