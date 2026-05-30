#pragma once
/*
   TARGA.DLL - Directory Opus Sample Viewer Plugin

   (c) Copyright 2009 GP Software
   All Rights Reserved
*/

#define TGAFILESIGNATURE		"TRUEVISION-XFILE."

#define TGAIMAGE_NONE				0
#define TGAIMAGE_COLORMAPPEDRAW		1
#define TGAIMAGE_TRUECOLORRAW		2
#define TGAIMAGE_BWRAW				3
#define TGAIMAGE_COLORMAPPEDRLE		9
#define TGAIMAGE_TRUECOLORRLE		10
#define TGAIMAGE_BWRLE				11

#define TGAORDER_BOTTOMLEFT			0
#define TGAORDER_BOTTOMRIGHT		1
#define TGAORDER_TOPLEFT			2
#define TGAORDER_TOPRIGHT			3

#define TGAATRTYPE_NONE             0
#define TGAATRTYPE_IGNORE           1
#define TGAATRTYPE_RETAIN           2
#define TGAATRTYPE_USEFUL           3
#define TGAATRTYPE_PREMULT          4

#define TGABUFSIZE	(64*1024)

class TGAFile
{
private:

#pragma pack(push,1)

	struct TGAFileHeader
	{
		BYTE	bIDLength;
		BYTE	bColorMapType;
		BYTE	bImageType;
		WORD	wColorMapFirstEntryIndex;
		WORD	wColorMapLength;
		BYTE	bColorMapEntrySize;
		WORD	wOriginX;
		WORD	wOriginY;
		WORD	wImageWidth;
		WORD	wImageHeight;
		BYTE	bPixelDepth;	
		BYTE	bImageDescriptor;

		int		GetZeroBits()   { return ((bImageDescriptor>>6)&3); }
		int		GetAlphaBits()  { return (bImageDescriptor&0xf);    }
		int		GetImageOrder() { return ((bImageDescriptor>>4)&3); }
	};

	struct TGAFileFooter
	{
		// (default) assignment operator must work on this struct.
		DWORD	dwExtensionAreaOffset;
		DWORD	dwDeveloperDirectoryOffset;
		char	chSignature[18];
	};

	struct TGAFileExtensionArea
	{
		// (default) assignment operator must work on this struct.
		WORD		wExtensionSize;
		char		chAuthorName[41];
		char		chAuthorComments[324];
		WORD		wMonth;
		WORD		wDay;
		WORD		wYear;
		WORD		wHour;
		WORD		wMinute;
		WORD		wSecond;
		char		chJobNameID[41];
		WORD		wJobTimeHours;
		WORD		wJobTimeMinutes;
		WORD		wJobTimeSeconds;
		char		chSoftwareID[41];
		WORD		wSoftwareVersionNumber;
		BYTE		bSoftwareVersionLetter;
		COLORREF	dwKeyColor;
		WORD		wPixelRatioNum;
		WORD		wPixelRatioDen;
		WORD		wGammaNum;
		WORD		wGammaDen;
		DWORD		dwColorCorrectionOffset;
		DWORD		dwPostageStampOffset;
		DWORD		dwScanLineOffset;
		BYTE		bAttributesType;
	};

#pragma pack(pop)


private:
	LeoHelpers::FileAndStream *m_pFas;
	Win32IOWrapper *m_pIO;

	TGAFileHeader        m_header;
	TGAFileFooter        m_footer;
	TGAFileExtensionArea m_extensionArea;

	bool m_fHaveFooter;
	bool m_fFileHasNoFooter; // Not all TGA files have a footer. If we look and it isn't there we set this to avoid looking again.
	bool m_fHaveExtensionArea;

	size_t m_pixelSize;
	size_t m_blockCount;
	size_t m_dupPixelCount;

	bool m_fWantColorMap;
	LeoHelpers::AutoBuffer< COLORREF > m_colorMap;

	// Buffered IO to hopefully speed things up
	LeoHelpers::AutoBuffer< BYTE > m_buffer;
	BYTE *m_pBufferPos;
	BYTE *m_pBufferEnd;

	bool (TGAFile::*m_readProc)(BYTE *pRow, const size_t decodeHeight, const size_t decodeWidth, const size_t rowSpare);
	bool (TGAFile::*m_thumProc)(BYTE *pRow, const size_t decodeHeight, const size_t decodeWidth, const size_t rowSpare);

	static const BYTE c5to8bits[32];

	void close();
	bool init();
	bool processHeader();
	bool attemptSeekReadFooterAndLosePosition();
	bool attemptSeekReadExtensionAreaAndLosePosition();
	bool scanToEndOfFile();
	bool loadColorMap();
	bool attemptSeekToAppropriateImage(SIZE *pSizeDesired, size_t *pDecodeWidth, size_t *pDecodeHeight);

private:
	// The byte/pixel/RLE/row functions are here, and use template meta-programming (http://en.wikipedia.org/wiki/Template_metaprogramming)
	// so that they can all be inlined and avoid run-time looping (in release builds anyway).
	// For example, readPixel_Raw<3> when compiled (for release builds) will not have a loop at all; just three calls to readByte.

	// These routines were adapted from code by Thomas G. Lane (this template adaption by Leo Davidson).

	// Read a single byte from the file
	bool readByte(BYTE *pOut)
	{
		// Got byte in buffer?
		if (m_pBufferPos < m_pBufferEnd)
		{
			*pOut = *m_pBufferPos++;
			return true;
		}

		m_pBufferPos = m_buffer.GetBuffer();

		size_t amountRead = m_pIO->read(m_pBufferPos, m_buffer.GetSizeBytes());

		if (amountRead < 1)
		{
			*pOut = 0;
			m_pBufferEnd = m_pBufferPos;
			return false;
		}

		m_pBufferEnd = m_pBufferPos + amountRead;

		*pOut = *m_pBufferPos++;
		return true;
	}

	// Read a non-RLE pixel
	template< int pixelSize > bool readPixel_Raw(BYTE pixel[ pixelSize ])
	{
		assert(pixelSize > 0 && pixelSize < 5);

		if (pixelSize > 0) { if (!readByte(pixel + 0)) { return false; } }
		if (pixelSize > 1) { if (!readByte(pixel + 1)) { return false; } }
		if (pixelSize > 2) { if (!readByte(pixel + 2)) { return false; } }
		if (pixelSize > 3) { if (!readByte(pixel + 3)) { return false; } }
		return true;
	}

	// Read an RLE-pixel
	template< int pixelSize > bool readPixel_RLE(BYTE pixel[ pixelSize ])
	{
		assert(pixelSize > 0 && pixelSize < 5);

		BYTE b;

		// Still got a 'run' from last time?
		if (m_dupPixelCount > 0)
		{
			--m_dupPixelCount;
			return true;
		}

		// Don't have a 'raw' block?
		if (m_blockCount > 0)
		{
			--m_blockCount;
		}
		else
		{
			// Read next block header
			if (!readByte(&b))
			{
				return false;
			}

			// Start of a 'run'?
			if (b&0x80)
			{
				// Get run count (number of pixels to output is this +1; only one pixel is read and repeated)
				m_dupPixelCount = (b&0x7f);
			}
			// Start of a 'raw'
			else
			{
				// Get raw count (number of pixels to read and output is this +1)
				m_blockCount = (b&0x7f);
			}
		}

		// Read the next pixel
		return readPixel_Raw< pixelSize >(pixel);
	}

	template< bool IsRLE, int pixelSize > bool readPixel_Gen(BYTE pixel[ pixelSize ])
	{
		if ( IsRLE) { return readPixel_RLE< pixelSize >(pixel); }
		if (!IsRLE) { return readPixel_Raw< pixelSize >(pixel); }
	}

	// Read 8 bit color-mapped row
	template< bool IsRLE > bool readRowCM8(BYTE *pRow, const size_t decodeHeight, const size_t decodeWidth, const size_t rowSpare)
	{
		const COLORREF *pColors = m_colorMap.GetBuffer();

		if (pColors == NULL || m_colorMap.GetSizeElements() < 256)
		{
			return false;
		}

		const BYTE *pColorRefBytes = 0;

		BYTE pixel[1] = {0};

		for (size_t y = 0; y < decodeHeight; ++y)
		{
			for (size_t x = 0; x < decodeWidth; ++x)
			{
				if (!readPixel_Gen< IsRLE, 1 >(pixel))
				{
					return false;
				}

				pColorRefBytes = reinterpret_cast< const BYTE * >( pColors + pixel[0] );

				pRow[0] = pColorRefBytes[0];
				pRow[1] = pColorRefBytes[1];
				pRow[2] = pColorRefBytes[2];
				pRow += 3;
			}
			pRow += rowSpare;
		}
		return true;
	}

	// Read 8 bit greyscale
	template< bool IsRLE > bool readRowBW8(BYTE *pRow, const size_t decodeHeight, const size_t decodeWidth, const size_t rowSpare)
	{
		BYTE pixel[1] = {0};

		for (size_t y = 0; y < decodeHeight; ++y)
		{
			for (size_t x = 0; x < decodeWidth; ++x)
			{
				if (!readPixel_Gen< IsRLE, 1 >(pixel))
				{
					return false;
				}

				pRow[0] = pixel[0];
				pRow[1] = pixel[0];
				pRow[2] = pixel[0];
				pRow += 3;
			}
			pRow += rowSpare;
		}
		return true;
	}

	// Read 16 bit row
	template< bool IsRLE > bool readRow16(BYTE *pRow, const size_t decodeHeight, const size_t decodeWidth, const size_t rowSpare)
	{
		BYTE pixel[2] = {0};

		for (size_t y = 0; y < decodeHeight; ++y)
		{
			for (size_t x = 0; x < decodeWidth; ++x)
			{
				if (!readPixel_Gen< IsRLE, 2 >(pixel))
				{
					return false;
				}

				// Get pixel value
				const WORD wPixel = pixel[0] + (pixel[1]<<8);

				// Expand 5 bit data to 8 bit sample width
				// The 1 spare bit can, in theory, be an alpha mask but I'm not aware of anything that will produce such an image to test against.
				pRow[0] = c5to8bits[ wPixel     &0x1f];
				pRow[1] = c5to8bits[(wPixel>>5 )&0x1f];
				pRow[2] = c5to8bits[(wPixel>>10)&0x1f];
				pRow += 3;
			}
			pRow += rowSpare;
		}
		return true;
	}

	// Read 24 bit or 32 bit row
	template< bool IsRLE, int pixelSize > bool readRowDirect(BYTE *pRow, const size_t decodeHeight, const size_t decodeWidth, const size_t rowSpare)
	{
		assert(pixelSize == 3 || pixelSize == 4);

		BYTE pixel[pixelSize] = {0};

		for (size_t y = 0; y < decodeHeight; ++y)
		{
			for (size_t x = 0; x < decodeWidth; ++x)
			{
				if (!readPixel_Gen< IsRLE, pixelSize >(pixel))
				{
					return false;
				}

				if (pixelSize == 4)
				{
					*reinterpret_cast< DWORD * >( pRow ) = *reinterpret_cast< DWORD * >( pixel );
				}
				else
				{
					if (pixelSize > 0) { pRow[0] = pixel[0]; }
					if (pixelSize > 1) { pRow[1] = pixel[1]; }
					if (pixelSize > 2) { pRow[2] = pixel[2]; }
				}
				pRow += pixelSize;
			}
			pRow += rowSpare;
		}
		return true;
	}

private:
	TGAFile(const TGAFile &rhs); // disallow
	TGAFile &operator=(const TGAFile &rhs); // disallow
public:
	TGAFile(LeoHelpers::FileAndStream *pFas);
	virtual ~TGAFile();

	bool Identify();

	int GetWidth()  { return m_pIO ? m_header.wImageWidth  : 0; }
	int GetHeight() { return m_pIO ? m_header.wImageHeight : 0; }
	int GetDepth()  { return m_pIO ? m_header.bPixelDepth  : 0; }

	HBITMAP LoadBitmap(HWND hWnd, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, SIZE *pSizeDesired);

};
