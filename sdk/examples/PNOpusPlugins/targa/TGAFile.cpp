/*
   TARGA.DLL - Directory Opus Sample Viewer Plugin

   (c) Copyright 2009 GP Software
   All Rights Reserved
*/

#include "StdAfx.h"
#include "../common/LeoHelpers.h"
#include "../common/Win32IOWrapper.h"
#include "TgaFile.h"

//static
const BYTE TGAFile::c5to8bits[32] =
{
	0,8,16,25,33,41,49,58,66,74,82,90,99,107,115,123,132,140,
	148,156,165,173,181,189,197,206,214,222,230,239,247,255
};

TGAFile::TGAFile(LeoHelpers::FileAndStream *pFas)
: m_pFas(pFas)
, m_pIO(Win32IOWrapper::CreateFromFAS(pFas, false)) // 2nd arg false because we want seeking but don't *need* it. Avoids expensive conversion of non-seekable streams.
, m_fHaveFooter(false)
, m_fFileHasNoFooter(false)
, m_fHaveExtensionArea(false)
, m_pBufferPos(0)
, m_pBufferEnd(0)
, m_pixelSize(0)
, m_blockCount(0)
, m_dupPixelCount(0)
, m_fWantColorMap(false)
, m_readProc(0)
, m_thumProc(0)
{
	if (!init()) // Don't call init() in the initialisers, for reasons that should be obvious.
	{
		close();
	}
}

// virtual
TGAFile::~TGAFile()
{
	close();
}

void TGAFile::close()
{
	// m_pFas does not belong to us so don't delete it.

	// m_pIO does belong to us. Delete it to close the file or release our reference on the stream (whichever it is).
	delete m_pIO;
	m_pIO = 0;
}

bool TGAFile::init()
{
	if (!m_pIO)
	{
		return false;
	}

	// Assumption: The file/steam position is at the start.

	assert(!m_pIO->GetThrowExceptions()); // Double-check that we don't throw exceptions in code called from constructors.

	std::wstring strFileExt;

	if (!m_pFas->GetFileExtension(&strFileExt, false)
	||	0 != _wcsicmp(strFileExt.c_str(), L"tga"))
	{
		// The file extension is not "tga" so we'll only consider this a TGA file if we can read the footer and it
		// has the TGA signature. If we can't seek quickly, or it's a TGA file that has no footer (which is possible
		// and fairly common) then we have no good way of verifying what it is and will not handle it.

		if (!attemptSeekReadFooterAndLosePosition()
		||	!m_fHaveFooter
		||	m_fFileHasNoFooter
		||	0 != m_pIO->seek(0, SEEK_SET))
		{
			return false;
		}
	}

	// The file/steam position is at the start.
	// Extension is TGA or we have read the footer and it is valid.

	if (sizeof(m_header) != m_pIO->read(&m_header, sizeof(m_header))
	||	!processHeader())
	{
		assert(false); // In debug builds I'd like to be alerted to any such images to have a look at them.
		return false;
	}

	// The file/steam position now just after the header.

	return true;
}

bool TGAFile::processHeader()
{
	// 16384 arbitrary max width/height
	if (m_header.wImageWidth  == 0 || m_header.wImageWidth  > 16384
	||	m_header.wImageHeight == 0 || m_header.wImageHeight > 16384)
	{
		return false;
	}

	switch(m_header.bPixelDepth)
	{
	default:
		return false;
	case 1:
	case 2:
	case 4:
	case 8:
	case 15:
	case 16:
	case 24:
	case 32:
		break;
	}

	m_pixelSize = ((m_header.bPixelDepth)>>3);
	m_blockCount = 0;
	m_dupPixelCount = 0;

	if (m_pixelSize == 0 || m_pixelSize > 4)
	{
		return false;
	}

	m_fWantColorMap = false;

	// Get the row reading function
	switch(m_header.bImageType)
	{
	default:
	case TGAIMAGE_NONE:
		return false;

	case TGAIMAGE_TRUECOLORRAW:
		switch(m_pixelSize)
		{
		default:
			return false;
		case 2:
			m_readProc = &TGAFile::readRow16< false >;
			m_thumProc = m_readProc;
			break;
		case 3:
			m_readProc = &TGAFile::readRowDirect< false, 3 >;
			m_thumProc = m_readProc;
			break;
		case 4:
			m_readProc = &TGAFile::readRowDirect< false, 4 >;
			m_thumProc = m_readProc;
			break;
		}
		break;

	case TGAIMAGE_TRUECOLORRLE:
		switch(m_pixelSize)
		{
		default:
			return false;
		case 2:
			m_readProc = &TGAFile::readRow16< true  >;
	        m_thumProc = &TGAFile::readRow16< false >;
			break;
		case 3:
			m_readProc = &TGAFile::readRowDirect< true,  3 >;
	        m_thumProc = &TGAFile::readRowDirect< false, 3 >;
			break;
		case 4:
			m_readProc = &TGAFile::readRowDirect< true,  4 >;
	        m_thumProc = &TGAFile::readRowDirect< false, 4 >;
			break;
		}
		break;

	case TGAIMAGE_BWRAW:
		if (m_pixelSize != 1) { return false; }
		m_readProc = &TGAFile::readRowBW8< false >;
		m_thumProc = m_readProc;
		break;

	case TGAIMAGE_BWRLE:
		if (m_pixelSize != 1) { return false; }
		m_readProc = &TGAFile::readRowBW8< true  >;
		m_thumProc = &TGAFile::readRowBW8< false >;
		break;


	case TGAIMAGE_COLORMAPPEDRAW:
		if (m_pixelSize != 1) { return false; }
		m_fWantColorMap = true;
		m_readProc = &TGAFile::readRowCM8< false >;
		m_thumProc = m_readProc;
		break;

	case TGAIMAGE_COLORMAPPEDRLE:
		if (m_pixelSize != 1) { return false; }
		m_fWantColorMap = true;
		m_readProc = &TGAFile::readRowCM8< true  >;
		m_thumProc = &TGAFile::readRowCM8< false >;
		break;
	}

	switch(m_header.bColorMapType)
	{
	default:
		return false;
	case 0:
		if (m_header.wColorMapFirstEntryIndex !=0
		||	m_header.wColorMapLength !=0
		||	m_header.bColorMapEntrySize != 0)
		{
			return false;
		}
		break;
	case 1:
		if (m_header.bColorMapEntrySize != 15
		&&	m_header.bColorMapEntrySize != 16
		&&	m_header.bColorMapEntrySize != 24
		&&	m_header.bColorMapEntrySize != 32)
		{
			return false;
		}
		if (m_header.wColorMapLength == 0)
		{
			return false;
		}
		break;
	}

	if (m_header.GetZeroBits() != 0)
	{
		return false;
	}

	switch(m_header.GetImageOrder())
	{
	default:
	case TGAORDER_TOPRIGHT:
	case TGAORDER_BOTTOMRIGHT: // Does anything actually make these images? I'd support them if I could find a test image.
		return false;
	case TGAORDER_BOTTOMLEFT:
	case TGAORDER_TOPLEFT:
		return true;
	}

	/*
	// Don't test the alpha bits value. It is not written properly by some programs and seems basically useless.
	switch(m_pixelSize)
	{
	default: return false;
	case 1: if (m_header.GetAlphaBits() != 0) { return false; } break;
	case 2: if (m_header.GetAlphaBits() != 0) { return false; } break; // in theory 1 could be valid here.
	case 3: if (m_header.GetAlphaBits() != 0) { return false; } break;
	case 4: if (m_header.GetAlphaBits() != 0
			&&	m_header.GetAlphaBits() != 8) { return false; } break;
	}
	*/

	return true;
}

bool TGAFile::attemptSeekReadFooterAndLosePosition()
{
	if (m_fHaveFooter)
	{
		return true;
	}

	if (m_fFileHasNoFooter || !m_pIO->CanFastRandomSeek())
	{
		return false;
	}

	// Read where the footer should be if it's there.

	assert(sizeof(m_footer) < LONG_MAX);
	assert(sizeof(m_footer.chSignature) == (strlen(TGAFILESIGNATURE) + 1));

	if (0 != m_pIO->seek(-static_cast< long >( sizeof(m_footer) ), SEEK_END)
	||	sizeof(m_footer) != m_pIO->read(&m_footer, sizeof(m_footer))
	||	0 != memcmp(m_footer.chSignature, TGAFILESIGNATURE, sizeof(m_footer.chSignature)))
	{
		m_fFileHasNoFooter = true;
		return false;
	}

	m_fHaveFooter = true;
	return true;
}

bool TGAFile::attemptSeekReadExtensionAreaAndLosePosition()
{
	if (m_fHaveExtensionArea)
	{
		return true;
	}

	// We need the footer to locate the extension area.
	if (!attemptSeekReadFooterAndLosePosition()
	||	m_footer.dwExtensionAreaOffset == 0
	||	m_footer.dwExtensionAreaOffset > LONG_MAX
	||	0 != m_pIO->seek(m_footer.dwExtensionAreaOffset, SEEK_SET))
	{
		return false;
	}

	ZeroMemory(&m_extensionArea,sizeof(m_extensionArea));

	WORD wExtensionSize = 0;

	// Read the size of the extension area structure, including the size WORD itself.
	if (sizeof(WORD) != m_pIO->read(&wExtensionSize, sizeof(WORD))
	||	wExtensionSize < sizeof(WORD))
	{
		return false;
	}

	// Kludge: Some silly TGA writers set 494 as the size when it should be (and really is) at least 495. Adjust for them.
	// This wouldn't matter if it wasn't the last byte of the extension area that we're particularly interested in. Typical.
	if (wExtensionSize == 494)
	{
		wExtensionSize = 495;
	}

	size_t amountToRead = sizeof(m_extensionArea);

	if (amountToRead > wExtensionSize)
	{
		amountToRead = wExtensionSize;
	}

	m_extensionArea.wExtensionSize = wExtensionSize;

	amountToRead -= sizeof(WORD);

	if (amountToRead != m_pIO->read(m_extensionArea.chAuthorName, amountToRead))
	{
		return false;
	}

	m_fHaveExtensionArea = true;

	return true;
}

bool TGAFile::scanToEndOfFile()
{
	assert(!m_pIO->CanFastRandomSeek()); // The other, faster ways of getting this data should be used if the input has fast seeking.

	// Assumption: We're just after the main image data.
	// Some data that came after the image data may be left over in the read buffer.

	unsigned __int64 ui64Size = 0;
	unsigned __int64 ui64Position = 0;
	unsigned __int64 ui64Extra = 0;

	if (!m_pIO->size64(&ui64Size)
	||	!m_pIO->tell64(&ui64Position)
	||	ui64Size < ui64Position)
	{
		return false;
	}

	if (m_pBufferPos != NULL && m_pBufferEnd != NULL && m_pBufferPos < m_pBufferEnd)
	{
		ui64Extra = m_pBufferEnd - m_pBufferPos;
	}

	if (ui64Extra > ui64Position)
	{
		return false;
	}

	unsigned __int64 ui64AmountToRead = ui64Size - ui64Position;
	unsigned __int64 ui64AmountTotal = ui64AmountToRead + ui64Extra;

	if (ui64AmountTotal < sizeof(m_footer)
	||	ui64AmountTotal > SIZE_MAX)
	{
		return false;
	}

	LeoHelpers::AutoBuffer< BYTE > restOfFile;

	if (!restOfFile.AllocateBytes(static_cast< size_t >(ui64AmountTotal) ))
	{
		return false;
	}

	BYTE *pRest = restOfFile.GetBuffer();
	BYTE *pEnd  = pRest + restOfFile.GetSizeBytes();

	if (ui64Extra != 0)
	{
		while(m_pBufferPos < m_pBufferEnd)
		{
			*pRest++ = *m_pBufferPos++;
		}
	}

	if (ui64AmountToRead != 0
	&&	ui64AmountToRead != m_pIO->read(pRest, static_cast< size_t >(ui64AmountToRead) ))
	{
		return false;
	}

	ui64Position -= ui64Extra; // ui64Position is now the file position of the start of restOfFile.

	// Look for the footer.

	assert(sizeof(m_footer.chSignature) == (strlen(TGAFILESIGNATURE) + 1));
	assert(sizeof(m_footer) == sizeof(TGAFileFooter)); // in case the type changes or whatever.

	pRest = restOfFile.GetBuffer(); // Point to start of the buffer again.

	const TGAFileFooter *pCandidateFooter =
		reinterpret_cast< const TGAFileFooter * >( pRest + (restOfFile.GetSizeBytes() - sizeof(TGAFileFooter)) );

	if (0 != memcmp(pCandidateFooter->chSignature, TGAFILESIGNATURE, sizeof(m_footer.chSignature)))
	{
		m_fFileHasNoFooter = true;
		return false;
	}

	m_footer = *pCandidateFooter;
	m_fHaveFooter = true;


	if (m_footer.dwExtensionAreaOffset == 0)
	{
		return true;
	}

	if (m_footer.dwExtensionAreaOffset < ui64Position)
	{
		return false;
	}

	const BYTE *pCandidateExtensionAreaBytes = pRest + (ui64Position - m_footer.dwExtensionAreaOffset);

	if ( (pCandidateExtensionAreaBytes + sizeof(WORD)) > pEnd )
	{
		return false;
	}

	WORD wExtensionSize = *reinterpret_cast< const WORD * >( pCandidateExtensionAreaBytes );

	// Kludge: Some silly TGA writers set 494 as the size when it should be (and really is) at least 495. Adjust for them.
	// This wouldn't matter if it wasn't the last byte of the extension area that we're particularly interested in. Typical.
	if (wExtensionSize == 494)
	{
		wExtensionSize = 495;
	}

	if (wExtensionSize < sizeof(WORD)
	||	pCandidateExtensionAreaBytes + wExtensionSize > pEnd)
	{
		return false;
	}

	size_t amountToCopy = sizeof(m_extensionArea);

	if (amountToCopy > wExtensionSize)
	{
		amountToCopy = wExtensionSize;
	}

	ZeroMemory(&m_extensionArea,sizeof(m_extensionArea));

	memcpy(&m_extensionArea, pCandidateExtensionAreaBytes, amountToCopy);

	m_extensionArea.wExtensionSize = wExtensionSize;

	m_fHaveExtensionArea = true;

	return true;
}

bool TGAFile::Identify()
{
	if (!m_pIO)
	{
		return false;
	}

	// If the file is still open after construction then we're good.
	return true;
}

HBITMAP TGAFile::LoadBitmap(HWND hWnd, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, SIZE *pSizeDesired)
{
	if (!m_pIO
	||	!m_buffer.AllocateBytes(TGABUFSIZE))
	{
		return NULL;
	}

	m_pBufferPos = m_buffer.GetBuffer();
	m_pBufferEnd = m_pBufferPos; // Buffer starts out empty.

	if ((m_header.bIDLength     != 0 && 0 != m_pIO->seekOrReadForward(m_header.bIDLength))
	||	(m_header.bColorMapType != 0 && !loadColorMap()))
	{
		return NULL;
	}

	// File pointer should now be at the start of the main image data.

	size_t tempDecodeWidth = m_header.wImageWidth;
	size_t tempDecodeHeight = m_header.wImageHeight;

	// Switch to decoding the embedded thumbnail if it makes sense (which is probably never!).
	if (!attemptSeekToAppropriateImage(pSizeDesired, &tempDecodeWidth, &tempDecodeHeight))
	{
		return NULL;
	}

	// Assign to constants to help out the optimizer.
	const size_t decodeWidth  = tempDecodeWidth;
	const size_t decodeHeight = tempDecodeHeight;

	// There is unused code to make us find and extract the embedded thumbnail instead of the main image,
	// if the thumbnail is available and suitable. However, the thumbnail is rarely available and almost
	// never suitable. :-) The code would be called here, if it was worth calling.

	if (decodeWidth == 0
	||	decodeHeight == 0
	||	decodeWidth > LONG_MAX
	||	decodeHeight > LONG_MAX)
	{
		return NULL;
	}

	// Note that we don't know what the alpha channel means until the end when we have the extension area.
	bool fAlphaChannel = (m_pixelSize == 4);
	bool fAlphaActuallyIsAlpha = fAlphaChannel;
	bool fAlphaIsPreMultiplied = false;
	bool fAlphaNeedsTesting = fAlphaChannel;

	size_t rowWidth = 0;
	size_t rowSpare = 0;

	// Calculate row width in bytes (we always convert to 24 bit, or 32 bit if there is an alpha channel)
	if (fAlphaChannel)
	{
		rowWidth = decodeWidth * 4; // Inherently a multiple of 4.
		rowSpare = 0;
	}
	else
	{
		rowWidth = (((decodeWidth*3)+3)&~3); // (x+3)&~3 rounds x up to a multiple of 4 as required for GDI bitmaps.
		rowSpare = rowWidth - (decodeWidth*3);
	}

	LeoHelpers::AutoBuffer< BYTE > bitmapData;
	if (!bitmapData.AllocateBytes(rowWidth * decodeHeight))
	{
		return NULL;
	}

	// Read rows
	if (!(this->*m_readProc)( bitmapData.GetBuffer(), decodeHeight, decodeWidth, rowSpare ))
	{
		return NULL;
	}

	if (fAlphaChannel)
	{
		if ( ( (  m_pIO->CanFastRandomSeek() && attemptSeekReadExtensionAreaAndLosePosition() )
		||	   ( !m_pIO->CanFastRandomSeek() && scanToEndOfFile()                             ) )
		&&	m_fHaveExtensionArea
		&&	m_extensionArea.wExtensionSize >= RTL_SIZEOF_THROUGH_FIELD(TGAFileExtensionArea,bAttributesType))
		{
			fAlphaNeedsTesting = false; // The file is explicit about its alpha channel so we don't need to test it.

			if (m_extensionArea.bAttributesType == TGAATRTYPE_PREMULT)
			{
				fAlphaIsPreMultiplied = true; // The pixels are pre-multiplied and we need to "un-multiply" them.
			}
			else if (m_extensionArea.bAttributesType != TGAATRTYPE_USEFUL)
			{
				fAlphaActuallyIsAlpha = false; // We should ignore the alpha channel as it's being used for something else.
			}
		}

		if (fAlphaNeedsTesting)
		{
			// Verify that at least one pixel has a non-zero alpha value. Some 32-bit images don't really use the alpha channel
			// and a completely transparent image seems totally useless, so assume any such image is a mistake and ignore its alpha.
			fAlphaActuallyIsAlpha = false;

			BYTE *pRow = bitmapData.GetBuffer();

			for (size_t y = 0; y < decodeHeight && !fAlphaActuallyIsAlpha; ++y)
			{
				BYTE *pCol = pRow + 3;
				for (size_t x = 0; x < decodeWidth; ++x)
				{
					if (*pCol != 0)
					{
						fAlphaActuallyIsAlpha = true;
						break;
					}
					pCol += 4;
				}
				pRow += rowWidth;
			}
		}
		else if (fAlphaIsPreMultiplied)
		{
			// "Un-multiply" the pre-multiplied alpha. That is, divide each color value by the alpha value.
			BYTE *pRow = bitmapData.GetBuffer();
			int t = 0;

			for (size_t y = 0; y < decodeHeight; ++y)
			{
				BYTE *pCol = pRow;
				for (size_t x = 0; x < decodeWidth; ++x)
				{
					t = pCol[3];
					if (t != 0)
					{
						pCol[0] = static_cast< BYTE >( (pCol[0] * 255) / t );
						pCol[1] = static_cast< BYTE >( (pCol[1] * 255) / t );
						pCol[2] = static_cast< BYTE >( (pCol[2] * 255) / t );
					}
					pCol += 4;
				}
				pRow += rowWidth;
			}
		}
	}

	// Initialise bitmap info
	BITMAPINFO bmInfo = {0};
	bmInfo.bmiHeader.biSize        = sizeof(bmInfo.bmiHeader);
	bmInfo.bmiHeader.biWidth       = static_cast< LONG >( decodeWidth );
	bmInfo.bmiHeader.biPlanes      = 1;
	bmInfo.bmiHeader.biBitCount    = fAlphaChannel ? 32 : 24;
	bmInfo.bmiHeader.biCompression = BI_RGB;

	switch(m_header.GetImageOrder())
	{
	default:
		return NULL;
	case TGAORDER_TOPRIGHT:
	case TGAORDER_TOPLEFT:
		bmInfo.bmiHeader.biHeight = -static_cast< LONG >( decodeHeight );
		break;
	case TGAORDER_BOTTOMRIGHT:
	case TGAORDER_BOTTOMLEFT:
		bmInfo.bmiHeader.biHeight = static_cast< LONG >( decodeHeight );
		break;
	}

	HBITMAP hBitmap = NULL;

	HDC hDC = ::GetDC(hWnd);

	if (hDC != NULL)
	{
		hBitmap = ::CreateDIBitmap(hDC, &bmInfo.bmiHeader, CBM_INIT, bitmapData.GetBuffer(), &bmInfo, DIB_RGB_COLORS);
		
		ReleaseDC(hWnd, hDC);
		hDC = NULL;
	}

	// "&& m_header.GetAlphaBits() == 8" -- don't test that as it does not seem reliable. Some images with alpha set it to 8 and some without set it to 0, so it seems useless.
	if (lpVPFileInfo && hBitmap && fAlphaChannel && fAlphaActuallyIsAlpha)
	{
		lpVPFileInfo->dwFlags|=DVPFIF_HasAlphaChannel; // Tell Opus the returned bitmap is 32 bit RGBA.
	}

	return hBitmap;
}

bool TGAFile::loadColorMap()
{
	if (m_header.bColorMapType != 1)
	{
		return false;
	}

	// Calculate color map size

	size_t colorEntryBytes = 0;

	switch(m_header.bColorMapEntrySize)
	{
	default: return false;
	case 15: // fall-through
	case 16: colorEntryBytes = 2; break;
	case 24: colorEntryBytes = 3; break;
	case 32: colorEntryBytes = 4; break;
	}

	size_t colorMapBytes = colorEntryBytes * m_header.wColorMapLength;

	if (colorMapBytes == 0)
	{
		return false;
	}

	// If we're going to use the color map, read it in.
	if (m_fWantColorMap)
	{
		if ((m_header.wColorMapFirstEntryIndex + m_header.wColorMapLength) > 256)
		{
			return false;
		}

		LeoHelpers::AutoBuffer< BYTE > tempColorMap;

		if (!tempColorMap.AllocateBytes(colorMapBytes)
		||	colorMapBytes != m_pIO->read(tempColorMap.GetBuffer(), colorMapBytes))
		{
			return false;
		}

		// Convert the color map into 32-bit, and make it the full length.
		if (!m_colorMap.AllocateElements( 256 ))
		{
			return false;
		}

		COLORREF *pColors = m_colorMap.GetBuffer();

		for(WORD i = 0; i < m_header.wColorMapFirstEntryIndex; ++i)
		{
			*pColors++ = RGB(0,0,0);
		}

		const BYTE *pTempMap = tempColorMap.GetBuffer();

		switch(colorEntryBytes)
		{
		default:
			return false;
		case 2:
			for(WORD i = 0; i < m_header.wColorMapLength; ++i)
			{
				WORD wPixel = pTempMap[0] + (pTempMap[1]<<8);

				// Expand 5 bit data to 8 bit sample width
				// The 1 spare bit can, in theory, be an alpha mask but I'm not aware of anything that will produce such an image to test against.
				reinterpret_cast< BYTE * >( pColors )[0] = c5to8bits[wPixel&0x1f]; wPixel>>=5;
				reinterpret_cast< BYTE * >( pColors )[1] = c5to8bits[wPixel&0x1f]; wPixel>>=5;
				reinterpret_cast< BYTE * >( pColors )[2] = c5to8bits[wPixel&0x1f];
				reinterpret_cast< BYTE * >( pColors )[3] = 0;
				++pColors;
				pTempMap += 2;
			}
			break;
		case 3:
			for(WORD i = 0; i < m_header.wColorMapLength; ++i)
			{
				reinterpret_cast< BYTE * >( pColors )[0] = *pTempMap++;
				reinterpret_cast< BYTE * >( pColors )[1] = *pTempMap++;
				reinterpret_cast< BYTE * >( pColors )[2] = *pTempMap++;
				reinterpret_cast< BYTE * >( pColors )[3] = 0;
				++pColors;
			}
			break;
		case 4:
			for(WORD i = 0; i < m_header.wColorMapLength; ++i)
			{
				*pColors++ = *reinterpret_cast< const COLORREF * >( pTempMap );
				pTempMap += 4;
			}
			break;
		}

		for(WORD i = m_header.wColorMapFirstEntryIndex + m_header.wColorMapLength; i < 256; ++i)
		{
			*pColors++ = RGB(0,0,0);
		}
	}
	// If we're not going to use the color map, skip it.
	else if (0 != m_pIO->seekOrReadForward(static_cast< long >( colorMapBytes )))
	{
		return false;
	}

	return true;
}

bool TGAFile::attemptSeekToAppropriateImage(SIZE *pSizeDesired, size_t *pDecodeWidth, size_t *pDecodeHeight)
{
#if 1
	// This code does work but it's a waste of time so I have disabled it.
	// a) Very few TGA files have an embedded thumbnail "postage stamp" image at all. Very few programs offer the option of including them.
	// b) The largest an TGA embedded thumbnail "postage stamp" can be is 255x255 and the TGA specs recommend 64x64.
	//    If Opus has dynamic thumbnail resizing enabled then we'll always be asked for 256x256 thumbs, meaning even if the TGA file has one
	//    it will probably be too small.
	// c) Even with fast seeking there is some cost of moving around the file, plus there's the chance of a bad embedded thumbnail and the
	//    consequent weirdness of a file that looks fine in the viewer but is b0rked in the file display.
	return true;
#else
	// Assumption: File pointer is at the start of the main image data.

	if (!m_pIO->CanSeek() || pSizeDesired == NULL || pSizeDesired->cx <= 0 || pSizeDesired->cy <= 0 || pSizeDesired->cx > 255 || pSizeDesired->cy > 255)
	{
		// One of the following is true, meaning that we should decode the main image and not the embedded thumbnail:
		// - We can't seek quickly (so it'll probably be just as quick to load the main image); or
		// - No thumbnail is being requested; or
		// - The requested thumbnail size is larger than a TGA embedded thumbnail could possbly be, so we need to provide the main image.
		return true;
	}

	// A thumbnail is being requested and we can seek quickly. See if there is one.

	unsigned __int64 mainImagePos = 0;

	if (!m_pIO->tell64(&mainImagePos)
	||	mainImagePos > LONG_MAX)
	{
		return false;
	}

	BYTE thumbWidth  = 0;
	BYTE thumbHeight = 0;

	// Try to load the extension area from the end of the file (returns immediately if we've already tried to load it).
	if (!attemptSeekReadExtensionAreaAndLosePosition()
	||	m_extensionArea.dwPostageStampOffset == 0
	||	0 != m_pIO->seek(m_extensionArea.dwPostageStampOffset, SEEK_SET)
	||	sizeof(thumbWidth)  != m_pIO->read(&thumbWidth,  sizeof(thumbWidth))
	||	sizeof(thumbHeight) != m_pIO->read(&thumbHeight, sizeof(thumbHeight))
	||	thumbWidth  < pSizeDesired->cx
	||	thumbHeight < pSizeDesired->cy)
	{
		// If there is no exension area, no embedded thumbnail, or the thumbnail is too small, we'll go back to decoding the full image.
		if (0 != m_pIO->seek(static_cast< long >(mainImagePos), SEEK_SET))
		{
			return false;
		}
		return true;
	}

	*pDecodeWidth  = thumbWidth;
	*pDecodeHeight = thumbHeight;

	// Embedded thumbnails have the same format as the main image, except they are never compressed.
	m_readProc = m_thumProc;

	return true;
#endif
}
