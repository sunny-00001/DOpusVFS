#include "StdAfx.h"
#include "LeoHelpers.h"
#include "GifConfig.h"
#include "GifDecoder.h"
#include "gifanim.h"
#include "GifConfig.h"
//#include "SprocketBorderImage.h"
#include "PictureFrameImage.h"
#include "EmptyImage.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NGifDecoder
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// static
HBITMAP NGifDecoder::CGifImage::LoadGifToDIBSection(HDC hDC, NGifDecoder::CGifFile *pGifFile,
													bool *pbHasUsedTransparency, bool *pbWantFrame, bool *pbRegenOnResize, SIZE *pSizeDesired, bool bThumbnailSprockets)
{
	HBITMAP hResult = NULL;

	if (pGifFile->InitOK())
	{
		CAbstractImageList imageList;

		bool bAnimation = false;

		if (LoadGifImage(&imageList, true, &bAnimation, NULL, pGifFile, NULL, NULL) && !imageList.IsEmpty())
		{
			bool bSprockets = (bAnimation && bThumbnailSprockets);

			// If we're not asking for Sprockets then we'll get the full-size image and return that, without shrinking it, also
			// allowing Opus to resize that image to make smaller thumbnails without calling us to do it.
			// If we are asking for Sprockets then we must generate the required size and tell Opus to ask us when it needs a
			// different size of thumbnail.

			hResult = imageList.GetFirstFrame()->CreateThumbnailDIBSection(hDC, bSprockets ? pSizeDesired : NULL, bSprockets, pbHasUsedTransparency, pbWantFrame, pbRegenOnResize);

			imageList.Clear();
		}
	}

	return(hResult);
}

// Note that *pbFileHasMultipleImages may be set true while only one image is returned if there's an error reading the second image.
// static
bool NGifDecoder::CGifImage::LoadGifImage(CAbstractImageList *pImageList,
										  bool bReadOnlyFirstImage, bool *pbFileHasMultipleImages, bool *pbNotAllFramesLoadedDueToMemory,
										  NGifDecoder::CGifFile *pGifFile, const RGBQUAD *prgbViewerBackground, int *pOriginalBitDepth)
{
	bool bLocalTempMultipleImages;

	if (NULL == pbFileHasMultipleImages)
	{
		pbFileHasMultipleImages = &bLocalTempMultipleImages;
	}

	*pbFileHasMultipleImages = false;

	if (NULL != pbNotAllFramesLoadedDueToMemory)
	{
		*pbNotAllFramesLoadedDueToMemory = false;
	}

	if (NULL != pOriginalBitDepth)
	{
		*pOriginalBitDepth = 0;
	}

	CAbstractImageList::size_type maxFramesInMemory = 0;

	pImageList->Clear();

	RGBQUAD rgbVBCopy;

	if (NULL != prgbViewerBackground)
	{
		rgbVBCopy = *prgbViewerBackground;
		rgbVBCopy.rgbReserved = 0; // Ensure it is marked transparent.
		prgbViewerBackground = &rgbVBCopy;
	}

	BYTE	buf[16];
	RGBQUAD	globalColorMap[GD_MAXCOLORMAPSIZE];
	RGBQUAD	localColorMap[GD_MAXCOLORMAPSIZE];

	// Default color map for images that don't have one at all. Set black & white and ramp the rest.
	globalColorMap[0].rgbRed       = 0;
	globalColorMap[0].rgbGreen     = 0;
	globalColorMap[0].rgbBlue      = 0;
	globalColorMap[0].rgbReserved  = 255;
	globalColorMap[1].rgbRed       = 255;
	globalColorMap[1].rgbGreen     = 255;
	globalColorMap[1].rgbBlue      = 255;
	globalColorMap[1].rgbReserved  = 255;
	for (int i = 2; i < GD_MAXCOLORMAPSIZE; i++)
	{
		globalColorMap[i].rgbRed      = i;
		globalColorMap[i].rgbGreen    = i;
		globalColorMap[i].rgbBlue     = i;
		globalColorMap[i].rgbReserved = 255;
	}

	// Set scaling mode.

	int iScaleMode = HALFTONE;

	// Read the GIF...

	CGifImage *pResult = NULL;

	if (pGifFile->ReadOK(buf,6)
	&&	(0 == strncmp((char *)buf,"GIF87a",3)
	||	 0 == strncmp((char *)buf,"GIF89a",3))
	&&	pGifFile->ReadOK(buf,7))
	{
		int iGlobalWidth  = LM_to_uint(buf[0],buf[1]);
		int iGlobalHeight = LM_to_uint(buf[2],buf[3]);

		int iBitDepthGlobal;//  = ((buf[4]>>4)&0x07)+1;
		int iNumColorsGlobal =  1<<((buf[4]&0x07)+1);

		if (iNumColorsGlobal <= 2)
		{
			iBitDepthGlobal = 1;
		}
		else if (iNumColorsGlobal <= 4)
		{
			iBitDepthGlobal = 2;
		}
		else if (iNumColorsGlobal <= 16)
		{
			iBitDepthGlobal = 4;
		}
		else
		{
			iBitDepthGlobal = 8;
		}

		if (NULL != pOriginalBitDepth)
		{
			*pOriginalBitDepth = iBitDepthGlobal;
		}

		bool bIsGlobalColorMap = (buf[4] & 0x80) ? true : false;

		int iBackgroundColor = bIsGlobalColorMap ? buf[5] : (-1);

		// Read the global colormap, unless the flag says there isn't one.
		if (iGlobalWidth > 0 && iGlobalHeight > 0
		&&	((!bIsGlobalColorMap)
		||	 (iNumColorsGlobal >= 0 && iNumColorsGlobal <= GD_MAXCOLORMAPSIZE && pGifFile->ReadColorMap(globalColorMap, iNumColorsGlobal))))
 		{
			const RGBQUAD *prgbGlobalBackgroundColor = ((-1) == iBackgroundColor) ? NULL : (globalColorMap+iBackgroundColor);
			RGBQUAD *prgbGlobalOriginalTransparentColor = NULL;
			int iTransparent = (-1);
			int iDisposal = 0;
			int iDelayTime = 0;

			std::list< CGifImage * > gifImageList;

			while (true)
			{
				BYTE c;

				if (! pGifFile->ReadOK(&c,1) )
				{
					break;
				}

				if (c == ';')	/* GIF terminator */
 				{
					break;
				}

				if (c == '!')	/* Extension */
				{
					if (! pGifFile->ReadOK(&c,1))
					{
						break;
					}

					pGifFile->DoExtension(c, &iTransparent, &iDisposal, &iDelayTime);

					continue;
				}

				if (c != ',')
				{
					continue; /* Not a valid start character */
				}

				if (! pGifFile->ReadOK(buf,9) )
				{
					break;
				}

				if (!pImageList->IsEmpty())
				{
					if (!(*pbFileHasMultipleImages))
					{
						*pbFileHasMultipleImages = true;

						if (bReadOnlyFirstImage)
						{
							break;
						}

						MEMORYSTATUS mstat;
						GlobalMemoryStatus(&mstat);
						if ((-1) != mstat.dwAvailPhys)
						{
							maxFramesInMemory = static_cast<CAbstractImageList::size_type>(mstat.dwAvailPhys / (pImageList->GetFirstFrame()->GetWidth() * pImageList->GetFirstFrame()->GetHeight() * 2 * sizeof(RGBQUAD)));
						}
					}

					if (0 != maxFramesInMemory && pImageList->GetNumberOfFrames() >= maxFramesInMemory)
					{
						if (NULL != pbNotAllFramesLoadedDueToMemory)
						{
							*pbNotAllFramesLoadedDueToMemory = true;
						}

						break;
					}
				}

				bool bIsLocalColorMap	= (buf[8] & 0x80) ? true : false;
				bool bInterlace			= (buf[8] & 0x40) ? true : false;

				int iNumColorsLocal = 1<<((buf[8]&0x07)+1);

				int iLocalLeft   = LM_to_uint(buf[0],buf[1]);
				int iLocalTop    = LM_to_uint(buf[2],buf[3]);
				int iLocalWidth  = LM_to_uint(buf[4],buf[5]);
				int iLocalHeight = LM_to_uint(buf[6],buf[7]);

				if (iLocalLeft   < 0
				||	iLocalTop    < 0
				||	iLocalWidth  < 0
				||	iLocalHeight < 0
				||	(iLocalLeft + iLocalWidth)  > iGlobalWidth
				||	(iLocalTop  + iLocalHeight) > iGlobalHeight)
				{
					break;
				}

				if (bIsLocalColorMap
				&&	(iNumColorsLocal < 0 || iNumColorsLocal > GD_MAXCOLORMAPSIZE || !pGifFile->ReadColorMap(localColorMap, iNumColorsLocal)))
				{
					break;
				}

			//	if ((-1) != iTransparent)
				if (0 <= iTransparent
				&&	GD_MAXCOLORMAPSIZE > iTransparent)
				{
					if (pImageList->IsEmpty()
					&&	prgbGlobalOriginalTransparentColor == NULL)
					{
						prgbGlobalOriginalTransparentColor = new RGBQUAD;
						prgbGlobalOriginalTransparentColor->rgbRed      = (bIsLocalColorMap ? localColorMap : globalColorMap)[ iTransparent ].rgbRed;
						prgbGlobalOriginalTransparentColor->rgbGreen    = (bIsLocalColorMap ? localColorMap : globalColorMap)[ iTransparent ].rgbGreen;
						prgbGlobalOriginalTransparentColor->rgbBlue     = (bIsLocalColorMap ? localColorMap : globalColorMap)[ iTransparent ].rgbBlue;
						prgbGlobalOriginalTransparentColor->rgbReserved = 0; // Transparent.
					}

					(bIsLocalColorMap ? localColorMap : globalColorMap)[ iTransparent ].rgbReserved = 0;
				}

				NGifDecoder::CGifImage *pNextImage;
				pNextImage = new CGifImage(iGlobalWidth, iGlobalHeight,
										   iLocalLeft, iLocalTop, iLocalWidth, iLocalHeight,
										   iDisposal, iDelayTime, iScaleMode);

				if (! ( pNextImage->InitOK()
				&&      pNextImage->ReadImage(pGifFile,
											  bInterlace,
											  &gifImageList,
											  prgbGlobalBackgroundColor,
										 	  prgbGlobalOriginalTransparentColor,
											  prgbViewerBackground,
											  bIsLocalColorMap ? localColorMap : globalColorMap) ) )
				{
					delete pNextImage;
					pNextImage = NULL;
				}
				else
				{
					gifImageList.push_back(pNextImage);
					pImageList->AddFrame(pNextImage, true);
				}

			//	if ((-1) != iTransparent)
				if (0 <= iTransparent
				&&	GD_MAXCOLORMAPSIZE > iTransparent)
				{
					(bIsLocalColorMap ? localColorMap : globalColorMap)[ iTransparent ].rgbReserved = 255;
				}

				// Reset for next image.
				iTransparent = (-1);
				iDisposal = 0;
				iDelayTime = 0;

				if (NULL == pNextImage)
				{
					break; // failure.
				}

				// success... look for another image.
			}

			delete prgbGlobalOriginalTransparentColor;
		}
	}

	return(!pImageList->IsEmpty());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CGifFile
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NGifDecoder::CGifFile::CGifFile(const TCHAR *szFilename, HANDLE hAbortEvent)
{
	commonInit();

	m_fileMode = GF_FM_DISKFILE;
	m_hAbortEvent = hAbortEvent;

	m_hDiskFile = CreateFile(szFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
}

NGifDecoder::CGifFile::CGifFile(IStream *pStream)
{
	commonInit();

	m_fileMode = GF_FM_STREAM;
	m_pStream = pStream;
}

void NGifDecoder::CGifFile::commonInit()
{
	m_fileMode = GF_FM_NONE;
	m_hDiskFile = INVALID_HANDLE_VALUE;
	m_hAbortEvent = NULL;
	m_pStream = NULL;

	m_dwBufSize = 0;
	m_dwBufPos = 0;

	m_ZeroDataBlock = false;
	m_GetCode_curbit = 0;
	m_GetCode_lastbit = 0;
	m_GetCode_done = false;
	m_GetCode_lastbyte = 0;
	m_LWZReadByte_fresh = false;
	m_LWZReadByte_codesize = 0;
	m_LWZReadByte_setcodesize = 0;
	m_LWZReadByte_maxcode = 0;
	m_LWZReadByte_maxcodesize = 0;
	m_LWZReadByte_firstcode = 0;
	m_LWZReadByte_oldcode = 0;
	m_LWZReadByte_clearcode = 0;
	m_LWZReadByte_endcode = 0;
	m_LWZReadByte_sp = NULL;

//	int i;

	ZeroMemory(m_gifBuffer, sizeof(m_gifBuffer));
//	for (i = 0; i < sizeof(m_gifBuffer); i++)
//	{
//		m_gifBuffer[i] = 0;
//	}

	ZeroMemory(m_DoExtension_buf, sizeof(m_DoExtension_buf));
//	for (i = 0; i < sizeof(m_DoExtension_buf); i++)
//	{
//		m_DoExtension_buf[i] = 0;
//	}

	ZeroMemory(m_GetCode_buf, sizeof(m_GetCode_buf));
//	for (i = 0; i < sizeof(m_GetCode_buf); i++)
//	{
//		m_GetCode_buf[i] = 0;
//	}

	ZeroMemory(m_LWZReadByte_table, sizeof(m_LWZReadByte_table));
//	for (i = 0; i < 2; i++)
//	{
//		for (int j = 0; j < (1<<GF_MAX_LWZ_BITS); j++)
//		{
//			m_LWZReadByte_table[i][j] = 0;
//		}
//	}

	ZeroMemory(m_LWZReadByte_stack, sizeof(m_LWZReadByte_stack));
//	for (i = 0; i < sizeof(m_LWZReadByte_stack)/sizeof(m_LWZReadByte_stack[0]); i++)
//	{
//		m_LWZReadByte_stack[i] = 0;
//	}
}

NGifDecoder::CGifFile::~CGifFile()
{
	if (INVALID_HANDLE_VALUE != m_hDiskFile)
	{
		CloseHandle(m_hDiskFile);
	}
}

bool NGifDecoder::CGifFile::InitOK()
{
	bool bResult = false;

	switch(m_fileMode)
	{
	case(GF_FM_DISKFILE):
		if (INVALID_HANDLE_VALUE != m_hDiskFile)
		{
			bResult = true;
		}
		break;
	case(GF_FM_STREAM):
		if (NULL != m_pStream)
		{
			bResult = true;
		}
		break;
	case(GF_FM_NONE):
	default:
		break;
	}

	return(bResult);
}

bool NGifDecoder::CGifFile::ReadOK(BYTE *pBufferOut, DWORD dwLen)
{
	DWORD dwTemp;

	// Loop until satisfied
	while (0 < dwLen)
	{
		// Data in buffer?
		if (m_dwBufPos < m_dwBufSize)
		{
			dwTemp = m_dwBufSize - m_dwBufPos;

			if (dwTemp > dwLen)
			{
				dwTemp = dwLen;
			}

			CopyMemory(pBufferOut, m_gifBuffer + m_dwBufPos, dwTemp);

			m_dwBufPos += dwTemp;
			pBufferOut += dwTemp;
			dwLen -= dwTemp;

			// Got all the data asked for?
			if (0 == dwLen)
			{
				return(true);
			}
		}

		// Read data into buffer
		m_dwBufPos = 0;
		m_dwBufSize = 0;

		if ((!readData(m_gifBuffer, sizeof(m_gifBuffer), &dwTemp)) || 0 == dwTemp)
		{
			return(false);
		}

		m_dwBufSize = dwTemp;
	}

	return(true);
}

bool NGifDecoder::CGifFile::readData(BYTE *pBufferOut, DWORD dwMaxRead, DWORD *pdwActualReadOut)
{
	bool bResult = false;

	if (NULL == m_hAbortEvent || WAIT_OBJECT_0 != WaitForSingleObject(m_hAbortEvent, 0))
	{
		switch(m_fileMode)
		{
		case(GF_FM_DISKFILE):
			if (INVALID_HANDLE_VALUE != m_hDiskFile
			&&	ReadFile(m_hDiskFile, pBufferOut, dwMaxRead, pdwActualReadOut, NULL))
			{
				bResult = true;
			}
			break;
		case(GF_FM_STREAM):
			if (NULL != m_pStream
			&&  S_OK == m_pStream->Read(pBufferOut, dwMaxRead, pdwActualReadOut))
			{
				bResult = true;
			}
			break;
		default:
		case(GF_FM_NONE):
			break;
		}
	}

	return(bResult);
}

bool NGifDecoder::CGifFile::ReadColorMap(RGBQUAD *pPalette, int cColours)
{
	bool bResult = true;

	unsigned char   rgb[3];

	if (GD_MAXCOLORMAPSIZE < cColours)
	{
		cColours = GD_MAXCOLORMAPSIZE;
	}

	for (int i = 0; i < cColours; i++)
	{
		if (! ReadOK(rgb, sizeof(rgb)))
		{
			bResult = false;
			break;
		}

		pPalette[i].rgbRed		= rgb[0];
		pPalette[i].rgbGreen	= rgb[1];
		pPalette[i].rgbBlue		= rgb[2];
		pPalette[i].rgbReserved	= 255;
	}

	return(bResult);
}

int NGifDecoder::CGifFile::GetDataBlock(BYTE *pBuf) // pBuf must have at least 255 bytes
{
	BYTE count;

	if (! ReadOK(&count, 1))
	{
		return(-1);
	}

	m_ZeroDataBlock = (0 == count) ? true : false;

	if ((!m_ZeroDataBlock) && (!ReadOK(pBuf, count)))
	{
		return(-1);
	}

	return(count);
}

void NGifDecoder::CGifFile::DoExtension(int label, int *pTransparent, int *pDisposal, int *pDelayTime)
{
	switch (label)
	{
	case 0xf9:              /* Graphic Control Extension */
		{
			int iBlockSize = GetDataBlock(m_DoExtension_buf);

			if (3 <= iBlockSize)
			{
				if (NULL != pDisposal)
				{
					*pDisposal = (m_DoExtension_buf[0] >> 2) & 0x7;
				}

				if (NULL != pDelayTime)
				{
					*pDelayTime = NGifDecoder::LM_to_uint(m_DoExtension_buf[1], m_DoExtension_buf[2]);
				}

				if (NULL != pTransparent && 0 != (m_DoExtension_buf[0] & 0x1) && 4 <= iBlockSize)
				{
					*pTransparent = m_DoExtension_buf[3];
				}
			}
		}

		while (0 < GetDataBlock(m_DoExtension_buf))
		{
		}

		break;

	default:
		while (0 < GetDataBlock(m_DoExtension_buf))
		{
		}

		break;
	}
}

int NGifDecoder::CGifFile::GetCode(int code_size, bool bFlag)
{
	int i, j, ret;
	int count;

	if (bFlag)
	{
		m_GetCode_curbit = 0;
		m_GetCode_lastbit = 0;
		m_GetCode_done = false;
		return(0);
	}

	if (code_size < 0 || code_size >= 256)
	{
		return(-1);
	}

	if ( (m_GetCode_curbit + code_size) >= m_GetCode_lastbit)
	{
		if (m_GetCode_done)
		{
			if (m_GetCode_curbit >= m_GetCode_lastbit)
			{
				/* Oh well */
			}

			return(-1);
		}

		if (m_GetCode_lastbyte > 2 && m_GetCode_lastbyte <= sizeof(m_GetCode_buf))
		{
			m_GetCode_buf[0] = m_GetCode_buf[ m_GetCode_lastbyte - 2 ];
			m_GetCode_buf[1] = m_GetCode_buf[ m_GetCode_lastbyte - 1 ];
		}

		count = GetDataBlock(m_GetCode_buf + 2);
		if ((-1) == count)
		{
			return(-1);
		}
		else if (0 == count)
		{
			m_GetCode_done = true;
		}

		m_GetCode_lastbyte = 2 + count;
		m_GetCode_curbit = (m_GetCode_curbit - m_GetCode_lastbit) + 16;
		m_GetCode_lastbit = (2 + count) * 8;
	}

	ret = 0;

	for (i = m_GetCode_curbit, j = 0; j < code_size; i++, j++)
	{
		ret |= ((m_GetCode_buf[ i / 8 ] & (1 << (i % 8))) != 0) << j;
	}

	m_GetCode_curbit += code_size;

	return(ret);
}

int NGifDecoder::CGifFile::LWZReadByte(bool bFlag, int input_code_size)
{
	int             code, incode;
	register int    i;

	if (bFlag)
	{
		m_LWZReadByte_setcodesize	= input_code_size;
		m_LWZReadByte_codesize		= 1 +  m_LWZReadByte_setcodesize;
		m_LWZReadByte_clearcode		= 1 << m_LWZReadByte_setcodesize;
		m_LWZReadByte_endcode		= 1 +  m_LWZReadByte_clearcode;
		m_LWZReadByte_maxcodesize	= 2 *  m_LWZReadByte_clearcode;
		m_LWZReadByte_maxcode		= 2 +  m_LWZReadByte_clearcode;

		GetCode(0, true);

		m_LWZReadByte_fresh = true;

		if (m_LWZReadByte_clearcode >= (1<<GF_MAX_LWZ_BITS))
		{
			return (-1);
		}

		for (i = 0; i < m_LWZReadByte_clearcode; i++)
		{
			m_LWZReadByte_table[0][i] = 0;
			m_LWZReadByte_table[1][i] = i;
		}

		for (; i < (1 << GF_MAX_LWZ_BITS); i++)
		{
			m_LWZReadByte_table[0][i] = m_LWZReadByte_table[1][0] = 0;
		}

		m_LWZReadByte_sp = m_LWZReadByte_stack;

		return(0);
	}
	else if (m_LWZReadByte_fresh)
	{
		m_LWZReadByte_fresh = false;

		do
		{
			m_LWZReadByte_firstcode = m_LWZReadByte_oldcode = GetCode(m_LWZReadByte_codesize, false);
		} while (m_LWZReadByte_firstcode == m_LWZReadByte_clearcode);

		return(m_LWZReadByte_firstcode);
	}

	if (m_LWZReadByte_sp > m_LWZReadByte_stack)
	{
		return(*--m_LWZReadByte_sp);
	}

	while ((code = GetCode(m_LWZReadByte_codesize, false)) >= 0)
	{
		if (code == m_LWZReadByte_clearcode)
		{
			if (m_LWZReadByte_clearcode >= (1<<GF_MAX_LWZ_BITS))
			{
				return (-1);
			}

			for (i = 0; i < m_LWZReadByte_clearcode; i++)
			{
				m_LWZReadByte_table[0][i] = 0;
				m_LWZReadByte_table[1][i] = i;
			}

			for (; i < (1<<GF_MAX_LWZ_BITS); i++)
			{
            	m_LWZReadByte_table[0][i] = m_LWZReadByte_table[1][i] = 0;
			}

			m_LWZReadByte_codesize		= m_LWZReadByte_setcodesize + 1;
			m_LWZReadByte_maxcodesize	= m_LWZReadByte_clearcode * 2;
			m_LWZReadByte_maxcode		= m_LWZReadByte_clearcode + 2;
			m_LWZReadByte_sp			= m_LWZReadByte_stack;
			m_LWZReadByte_firstcode		= m_LWZReadByte_oldcode = GetCode(m_LWZReadByte_codesize, false);

			return(m_LWZReadByte_firstcode);
		}
		else if (code == m_LWZReadByte_endcode)
		{
			int             count;
			unsigned char   buf[260];

			if (m_ZeroDataBlock)
			{
				return(-2);
			}

			while (0 < (count = GetDataBlock(buf)))
			{
			}

			if (0 != count)
			{
            	return(-2);
			}
		}

		incode = code;

		if (m_LWZReadByte_sp >= (m_LWZReadByte_stack + GF_STACK_SIZE))
		{
			/* Bad compressed data stream */
			return(-1);
		}

		if (code >= m_LWZReadByte_maxcode)
		{
			*m_LWZReadByte_sp++ = m_LWZReadByte_firstcode;
			code = m_LWZReadByte_oldcode;
		}

		while (code >= m_LWZReadByte_clearcode)
		{
			if (m_LWZReadByte_sp >= (m_LWZReadByte_stack + GF_STACK_SIZE)
			||	code >= (1<<GF_MAX_LWZ_BITS)
			||	code < 0)
			{
				/* Bad compressed data stream */
				return(-1);
			}

			*m_LWZReadByte_sp++ = m_LWZReadByte_table[1][code];

			if (code == m_LWZReadByte_table[0][code])
			{
				/* Oh well */
			}

			code = m_LWZReadByte_table[0][code];
		}

		if (m_LWZReadByte_sp >= (m_LWZReadByte_stack + GF_STACK_SIZE)
		||	code >= (1<<GF_MAX_LWZ_BITS)
		||	code < 0)
		{
			/* Bad compressed data stream */
			return(-1);
		}

		*m_LWZReadByte_sp++ = m_LWZReadByte_firstcode = m_LWZReadByte_table[1][code];

		code = m_LWZReadByte_maxcode;

		if (code < (1<<GF_MAX_LWZ_BITS)
		&&	code >= 0)
		{
			m_LWZReadByte_table[0][code] = m_LWZReadByte_oldcode;
			m_LWZReadByte_table[1][code] = m_LWZReadByte_firstcode;
			m_LWZReadByte_maxcode++;

			if ((m_LWZReadByte_maxcode >= m_LWZReadByte_maxcodesize)
			&&	((1<<GF_MAX_LWZ_BITS) > m_LWZReadByte_maxcodesize))
			{
				m_LWZReadByte_maxcodesize *= 2;
				m_LWZReadByte_codesize++;
			}
		}

		m_LWZReadByte_oldcode = incode;

		if (m_LWZReadByte_sp > m_LWZReadByte_stack)
		{
			return(*--m_LWZReadByte_sp);
		}
	}

	return(code);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CGifImage
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

NGifDecoder::CGifImage::CGifImage(	int iGlobalWidth, int iGlobalHeight,
									int iLocalLeft, int iLocalTop, int iLocalWidth, int iLocalHeight,
									int iDisposal, int iDelayTime, int iScaleMode)
: CMemoryImage(iGlobalWidth, iGlobalHeight, iScaleMode)
{
	m_iLocalLeft   = iLocalLeft;
	m_iLocalTop    = iLocalTop;
	m_iLocalWidth  = iLocalWidth;
	m_iLocalHeight = iLocalHeight;

	m_iDisposal = iDisposal;
	m_iDelayTime = iDelayTime;
}

/*
// This constructor flattens an image list into a single image.
// The image list is assumed to be non-empty.
NGifDecoder::CGifImage::CGifImage(std::list< CAbstractImage * > *pImageList, int iNumberOfColumns, const RGBQUAD *prgbViewerBackground)
{
	int iNumberOfRows;

	if (0 == iNumberOfColumns)
	{
		iNumberOfColumns = pImageList->size();
		iNumberOfRows = 1;
	}
	else if (1 == iNumberOfColumns)
	{
		iNumberOfRows = pImageList->size();
	}
	else
	{
		iNumberOfRows = (pImageList->size() + (iNumberOfColumns - 1)) / iNumberOfColumns;
	}

	int iFrameWidth  = pImageList->front()->GetWidth();
	int iFrameHeight = pImageList->front()->GetHeight();

	int iWidth  = iFrameWidth  * iNumberOfColumns;
	int iHeight = iFrameHeight * iNumberOfRows;

	int iSkipOffset = iFrameWidth * (iNumberOfColumns - 1);

	int iTotalImages = iNumberOfRows * iNumberOfColumns;

	RGBQUAD rgbBackground;

	if (pImageList->front()->GetHasUsedTransparency() && NULL != prgbViewerBackground)
	{
		rgbBackground.rgbRed      = prgbViewerBackground->rgbRed;
		rgbBackground.rgbGreen    = prgbViewerBackground->rgbGreen;
		rgbBackground.rgbBlue     = prgbViewerBackground->rgbBlue;
		rgbBackground.rgbReserved = 0;
	}
	else
	{
		COLORREF bgCol = pImageList->front()->GetGeneratedBackgroundColor();

		rgbBackground.rgbRed      = GetRValue(bgCol);
		rgbBackground.rgbGreen    = GetGValue(bgCol);
		rgbBackground.rgbBlue     = GetBValue(bgCol);
		rgbBackground.rgbReserved = 255;
	}

	m_pPixelData = new(std::nothrow) RGBQUAD[ iWidth * iHeight ];

	if (NULL != m_pPixelData)
	{
		std::list< CAbstractImage * >::iterator iter = pImageList->begin();

		for (int iImageNumber = 0; iImageNumber < iTotalImages; iImageNumber++)
		{
			RGBQUAD *pOut = m_pPixelData
						+ (iWidth * iFrameHeight * (iImageNumber / iNumberOfColumns))
						+ (         iFrameWidth  * (iImageNumber % iNumberOfColumns));

			if (iter != pImageList->end())
			{
				CGifImage *pGI = static_cast< CGifImage * >( *iter );

				RGBQUAD *pIn = pGI->m_pPixelData;

				for (int y = 0; y < pGI->GetHeight(); y++)
				{
					for (int x = 0; x < pGI->GetWidth(); x++)
					{
						copyPixelWithDither(pOut++, pIn++, x, y, pGI->GetWidth(), pGI->GetHeight(), 0, NULL, NULL);
					}

					pOut += iSkipOffset;
				}

				iter++;
			}
			else
			{
				for (int y = 0; y < iFrameHeight; y++)
				{
					for (int x = 0; x < iFrameWidth; x++)
					{
						*pOut++ = rgbBackground;
					}

					pOut += iSkipOffset;
				}
			}
		}
	}


	m_iWidth	= iWidth;
	m_iHeight	= iHeight;

	m_iLocalLeft   = 0;
	m_iLocalTop    = 0;
	m_iLocalWidth  = iWidth;
	m_iLocalHeight = iHeight;

	COLORREF tempColRefOTC = pImageList->front()->GetOriginalTransparentColor();
	COLORREF tempColRefGBC = pImageList->front()->GetGeneratedBackgroundColor();

	m_bTransparencyActuallyUsed    = pImageList->front()->GetHasUsedTransparency();
	m_bGotGeneratedBackgroundColor = true;
	m_rgbOriginalTransparentColor.rgbRed      = GetRValue(tempColRefOTC);
	m_rgbOriginalTransparentColor.rgbGreen    = GetGValue(tempColRefOTC);
	m_rgbOriginalTransparentColor.rgbBlue     = GetBValue(tempColRefOTC);
	m_rgbOriginalTransparentColor.rgbReserved = 255;
	m_rgbGeneratedBackgroundColor.rgbRed      = GetRValue(tempColRefGBC);
	m_rgbGeneratedBackgroundColor.rgbGreen    = GetGValue(tempColRefGBC);
	m_rgbGeneratedBackgroundColor.rgbBlue     = GetBValue(tempColRefGBC);
	m_rgbGeneratedBackgroundColor.rgbReserved = 255;

	m_iDisposal = GID_WHATEVER;
	m_iDelayTime = 0;

	m_iScaleMode = pImageList->front()->GetScaleMode();
}
*/

NGifDecoder::CGifImage::~CGifImage()
{
}

void NGifDecoder::CGifImage::inheritImageBuffer(std::list< CGifImage * > *pImageList,
												const RGBQUAD *prgbGlobalBackgroundColor,
												const RGBQUAD *prgbGlobalOriginalTransparentColor,
												const RGBQUAD *prgbViewerBackground)
{
	if (InitOK())
	{
		LONG lTotalPixels = GetWidth() * GetHeight();

		// Work out the color to use if we need to do any filling.
		RGBQUAD rgbBlack;
		rgbBlack.rgbRed		 = 0;
		rgbBlack.rgbGreen	 = 0;
		rgbBlack.rgbBlue	 = 0;
		rgbBlack.rgbReserved = 255; // opaque; not transparent

		const RGBQUAD *prgbToFillWith = &rgbBlack;

		if (NULL != prgbGlobalOriginalTransparentColor)
		{
			if (NULL != prgbViewerBackground)
			{
				prgbToFillWith = prgbViewerBackground;
			}
			else
			{
				prgbToFillWith = prgbGlobalOriginalTransparentColor;
			}
		}
		else if (NULL != prgbGlobalBackgroundColor)
		{
			prgbToFillWith = prgbGlobalBackgroundColor;
		}

		// Find and copy the buffer from a previous image.

		bool bPreviousImageFound = false;

		for (std::list< CGifImage * >::reverse_iterator riter = pImageList->rbegin(); riter != pImageList->rend(); riter++)
		{
			CGifImage *pGI = *riter;

			if (GID_PREVIOUS != pGI->m_iDisposal && GID_PREVIOUS_ALT != pGI->m_iDisposal)
			{
				bPreviousImageFound = true;

				// Copy the previous image's buffer.

				for (LONG i = 0; i < lTotalPixels; i++)
				{
					m_pPixelData[ i ] = pGI->m_pPixelData[ i ];
				}

				if (GID_BACKGROUND == pGI->m_iDisposal)
				{
					int iGILocalRight  = pGI->m_iLocalLeft + pGI->m_iLocalWidth;
					int iGILocalBottom = pGI->m_iLocalTop  + pGI->m_iLocalHeight;

					int iNonLacedYOffset = (m_iWidth - pGI->m_iLocalWidth);

					int iPixelIndex = pGI->m_iLocalLeft + pGI->m_iLocalTop * m_iWidth;

					for (int y = pGI->m_iLocalTop; y < iGILocalBottom; ++y)
					{
						for (int x = pGI->m_iLocalLeft; x < iGILocalRight; ++x)
						{
							if (0 <= x && x < m_iWidth
							&&	0 <= y && y < m_iHeight)
							{
								m_pPixelData[ iPixelIndex ] = *prgbToFillWith;
							}

							++iPixelIndex;
						}

						iPixelIndex += iNonLacedYOffset;
					}
				}

				// Check any pixels we won't paint over for transparency.
				if (!m_bTransparencyActuallyUsed
				&&	(pGI->GetHasUsedTransparency() || prgbToFillWith->rgbReserved == 0)
				&&	(0 != m_iLocalLeft || 0 != m_iLocalTop || GetWidth() != m_iLocalWidth || GetHeight() != m_iLocalHeight))
				{
					// I think this is the first time I've used goto in C or C++. :-)

					int w = GetWidth();
					int h = GetHeight();

					LONG i = 0; // reused in each loop

					// Top strip, including sides.
					LONG lNumTopPixels = w * m_iLocalTop;
					while(lNumTopPixels-- > 0)
					{
						if (m_pPixelData[ i++ ].rgbReserved == 0) goto foundTrans;
					}

					// Left and right sides between top and bottom strips.
					int endLine = m_iLocalTop + m_iLocalHeight;
					int startRow2 = m_iLocalLeft + m_iLocalWidth;

					for (int y = m_iLocalTop; y < endLine; ++y)
					{
						// Left side
						for (int x = 0; x < m_iLocalLeft; ++x)
						{
							if (m_pPixelData[ i++ ].rgbReserved == 0) goto foundTrans;
						}

						i += m_iLocalWidth;

						// Right side
						for (int x = startRow2; x < w; ++x)
						{
							if (m_pPixelData[ i++ ].rgbReserved == 0) goto foundTrans;
						}
					}

					// Bottom strip, including sides.
					LONG lNumBottomPixels = w * (h - (m_iLocalTop + m_iLocalHeight));

					while(lNumBottomPixels-- > 0)
					{
						if (m_pPixelData[ i++ ].rgbReserved == 0) goto foundTrans;
					}
				}

				break;
foundTrans:
				m_bTransparencyActuallyUsed = true;
				break;
			}
		}

		// If there was no previous image to copy, fill a background.
		if (!bPreviousImageFound)
		{
			if (NULL != prgbGlobalOriginalTransparentColor
			&&	(0 != m_iLocalLeft || 0 != m_iLocalTop || GetWidth() != m_iLocalWidth || GetHeight() != m_iLocalHeight))
			{
				// There's a transparent border around the image so transparency is actually used.
				// (The flag exists because some gifs have a transparent color which is actually never used and
				// in such cases we want to behave as if there is no transparent color.)
				m_bTransparencyActuallyUsed = true;
			}

			for (LONG i = 0; i < lTotalPixels; i++)
			{
				m_pPixelData[ i ] = *prgbToFillWith;
			}
		}
	}
}

bool NGifDecoder::CGifImage::ReadImage(CGifFile *pGifFile,
									   bool bInterlace,
									   std::list< CGifImage * > *pImageList,
									   const RGBQUAD *prgbGlobalBackgroundColor,
									   const RGBQUAD *prgbGlobalOriginalTransparentColor,
									   const RGBQUAD *prgbViewerBackground,
									   const RGBQUAD *pPalette)
{
	bool bResult = false;

	if (InitOK() && NULL != pPalette)
	{
		// Inherit buffer from a previous image.
		inheritImageBuffer(pImageList, prgbGlobalBackgroundColor, prgbGlobalOriginalTransparentColor, prgbViewerBackground);

		BYTE	c;
		int		v;
		int		xpos = m_iLocalLeft;
		int		ypos;
		int		pass = 0;
		int		iPixelIndex;

		int iLocalRight  = m_iLocalLeft + m_iLocalWidth;
		int iLocalBottom = m_iLocalTop  + m_iLocalHeight;

		int iNonLacedYOffset = (m_iWidth - m_iLocalWidth);

		int piLacedYPosOffsets[4] =
		{
			8,
			8,
			4,
			2
		};
		int piLacedYOffsets[4] =
		{
			8 * m_iWidth - m_iLocalWidth,
			8 * m_iWidth - m_iLocalWidth,
			4 * m_iWidth - m_iLocalWidth,
			2 * m_iWidth - m_iLocalWidth
		};

		int piLacedYStartOffsets[4] =
		{
			m_iLocalTop,
			m_iLocalTop + 4,
			m_iLocalTop + 2,
			m_iLocalTop + 1
		};
		int piLacedPixelStartOffsets[4] =
		{
			m_iLocalLeft + m_iLocalTop * m_iWidth,
			m_iLocalLeft + (m_iLocalTop + 4) * m_iWidth,
			m_iLocalLeft + (m_iLocalTop + 2) * m_iWidth,
			m_iLocalLeft + (m_iLocalTop + 1) * m_iWidth
		};

		ypos = piLacedYStartOffsets[0];
		iPixelIndex = piLacedPixelStartOffsets[0];

		/* Initialize the Compression routines */

		if (pGifFile->ReadOK(&c,1)
		&&	0 <= pGifFile->LWZReadByte(true, c)
		&&	xpos < iLocalRight
		&&	ypos < iLocalBottom)
		{
			while (0 <= (v = pGifFile->LWZReadByte(false, c)))
			{
				setPixel(xpos, ypos, iPixelIndex, v, pPalette);

				++xpos;
				++iPixelIndex;

				if (xpos >= iLocalRight)
				{
					xpos = m_iLocalLeft;

					if (!bInterlace)
					{
						++ypos;
						iPixelIndex += iNonLacedYOffset;
					}
					else
					{
						ypos += piLacedYPosOffsets[pass];

						if (ypos < iLocalBottom)
						{
							iPixelIndex += piLacedYOffsets[pass];
						}
						else
						{
							// For short (sub-)images this may skip an interlace level completely.
							// For example, if the image is less than 8 pixels tell then the second pass
							// does nothing because it does every 8th row. (Or something like that.)
							// money_eyes.gif used to reproduce a bug that was fixed here.
							while (ypos >= iLocalBottom && ++pass <= 3)
							{
								ypos = piLacedYStartOffsets[pass];
								iPixelIndex = piLacedPixelStartOffsets[pass];
							}

							if (ypos >= iLocalBottom || pass > 3)
							{
								break;
							}
						}
					}

					if (ypos >= iLocalBottom)
					{
						break;
					}
				}
			}

			if (0 <= pGifFile->LWZReadByte(false, c))
			{
				/* Ignore extra */
			}

			bResult = true;
		}
	}

	if (bResult)
	{
		// Store Colours.

		if (prgbGlobalOriginalTransparentColor != NULL && m_bTransparencyActuallyUsed)
		{
			m_crAutomaticBackgroundColor = RGB(prgbGlobalOriginalTransparentColor->rgbRed, prgbGlobalOriginalTransparentColor->rgbGreen, prgbGlobalOriginalTransparentColor->rgbBlue);
		}
		else
		{
			m_crAutomaticBackgroundColor = RGB(m_pPixelData->rgbRed, m_pPixelData->rgbGreen, m_pPixelData->rgbBlue);
		}

		if (prgbViewerBackground != NULL)
		{
			m_crCurrentBackgroundColor = RGB(prgbViewerBackground->rgbRed, prgbViewerBackground->rgbGreen, prgbViewerBackground->rgbBlue);
		}
		else
		{
			m_crCurrentBackgroundColor = m_crAutomaticBackgroundColor;
		}

		RGBQUAD rgbBg;
		rgbBg.rgbRed      = GetRValue(m_crCurrentBackgroundColor);
		rgbBg.rgbGreen    = GetGValue(m_crCurrentBackgroundColor);
		rgbBg.rgbBlue     = GetBValue(m_crCurrentBackgroundColor);
		rgbBg.rgbReserved = 0;
	}

	return(bResult);
}

bool NGifDecoder::CGifImage::RotateImage(int rotationAmount)
{
	bool bResult = false;

	rotationAmount %= 360;

	if (0 > rotationAmount)
	{
		rotationAmount += 360;
	}

	if (InitOK())
	{
		DeleteCachedPaintBitmap();

		int origGlobalX = m_iWidth;
		int origGlobalY = m_iHeight;

		int origLocalLeft   = m_iLocalLeft;
		int origLocalTop    = m_iLocalTop;
		int origLocalWidth  = m_iLocalWidth;
		int origLocalHeight = m_iLocalHeight;

		if (270 <= rotationAmount)
		{
			if (rotate270( &m_pPixelData, origGlobalX, origGlobalY ))
			{
				// Transpose width and height.
				m_iWidth  = origGlobalY;
				m_iHeight = origGlobalX;

				m_iLocalLeft   = origLocalTop;
				m_iLocalTop    = origGlobalX - (origLocalLeft + origLocalWidth);
				m_iLocalWidth  = origLocalHeight;
				m_iLocalHeight = origLocalWidth;

				bResult = true;
			}
		}
		else if (180 <= rotationAmount)
		{
			if (rotate180( &m_pPixelData, origGlobalX, origGlobalY ))
			{
				// Width and height are unchanged.
				// Adjust the inner square
				m_iLocalLeft   = origGlobalX - (origLocalLeft + origLocalWidth);
				m_iLocalTop    = origGlobalY - (origLocalTop  + origLocalHeight);

				bResult = true;
			}
		}
		else if (90 <= rotationAmount)
		{
			if (rotate90( &m_pPixelData, origGlobalX, origGlobalY ))
			{
				// Transpose width and height.
				m_iWidth  = origGlobalY;
				m_iHeight = origGlobalX;

				// Adjust the inner square
				m_iLocalLeft   = origGlobalY - (origLocalTop + origLocalHeight);
				m_iLocalTop    = origLocalLeft;
				m_iLocalWidth  = origLocalHeight;
				m_iLocalHeight = origLocalWidth;

				bResult = true;
			}
		}
		else if (0 <= rotationAmount)
		{
			bResult = true;
		}
	}

	return(bResult);
}

bool NGifDecoder::CGifImage::WantBorders() const
{
	return true;
}

bool NGifDecoder::CGifImage::NeedNewBorders(CAbstractImageList::size_type oldNumCols, CAbstractImageList::size_type oldNumRows, CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows) const
{
	return (oldNumCols < 1 || oldNumRows < 1);

		// Extra checks were needed when we had borders which changed width/height depending on the layout.
//	     || (oldNumCols == 1 && numCols != 1)
//	     || (oldNumCols != 1 && numCols == 1));
}

/*
bool NGifDecoder::CGifImage::getBorderMetrics(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, SIZE *pSizeImageBorder, SIZE *pSizeOuterBorder) const
{
	if (numCols > 1)
	{
		if (pSizeImageBorder != NULL)
		{
			pSizeImageBorder->cx = BORDER_THIN;
			pSizeImageBorder->cy = BORDER_THICK;
		}
		if (pSizeOuterBorder != NULL)
		{
			pSizeOuterBorder->cx = BORDER_OUTER_LEFTRIGHT;
			pSizeOuterBorder->cy = 0;
		}
		return true;
	}
	else if (numCols == 1)
	{
		if (pSizeImageBorder != NULL)
		{
			pSizeImageBorder->cx = BORDER_THICK;
			pSizeImageBorder->cy = BORDER_THIN;
		}
		if (pSizeOuterBorder != NULL)
		{
			pSizeOuterBorder->cx = 0;
			pSizeOuterBorder->cy = BORDER_OUTER_TOPBOTTOM;
		}
		return true;
	}
	else
	{
		if (pSizeImageBorder != NULL)
		{
			pSizeImageBorder->cx = 0;
			pSizeImageBorder->cy = 0;
		}
		if (pSizeOuterBorder != NULL)
		{
			pSizeOuterBorder->cx = 0;
			pSizeOuterBorder->cy = 0;
		}
		return false;
	}
}
*/

bool NGifDecoder::CGifImage::GetTotalBorderSize(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, SIZE *pSize) const
{
	/*
	SIZE sizeImageBorder;
	SIZE sizeOuterBorder;

	if (!getBorderMetrics(numCols, numRows, &sizeImageBorder, &sizeOuterBorder))
	{
		return false;
	}

	pSize->cx = sizeOuterBorder.cx + numCols * sizeImageBorder.cx;
	pSize->cy = sizeOuterBorder.cy + numRows * sizeImageBorder.cy;
	*/

	pSize->cx = numCols * OPUSVIEWER_IMAGE_FRAME_SIZE;
	pSize->cy = numRows * OPUSVIEWER_IMAGE_FRAME_SIZE;

	return true;
}

CAbstractImageList::size_type NGifDecoder::CGifImage::CalcNumFramesFitHoriz(LONG lSpaceWidth, int iNormalisedZoomFactor, bool bBorders) const
{
	if (!bBorders)
	{
		return LeoHelpers::MulDivRoundDown(lSpaceWidth, 100, GetWidth() * iNormalisedZoomFactor);
	}

	return LeoHelpers::MulDivRoundDown(lSpaceWidth, 100, (GetWidth() + OPUSVIEWER_IMAGE_FRAME_SIZE) * iNormalisedZoomFactor);

	/*
	int leftRightBorderSize = (BORDER_OUTER_LEFTRIGHT * iNormalisedZoomFactor) / 100;
	int numMultiColumns = ((lSpaceWidth - leftRightBorderSize) * 100) / ((GetWidth() + BORDER_THIN) * iNormalisedZoomFactor);

	if (numMultiColumns > 1)
	{
		// If more than one frame will fit then we use the multi-column border size.
		return numMultiColumns;
	}

	// If only one frame will fit (or none will fit at all) then we use the single-column border size.
	// Don't need to consider BORDER_OUTER_TOPBOTTOM as it only affects the height.
//	return (lSpaceWidth * 100) / ((GetWidth() + BORDER_THICK) * iNormalisedZoomFactor);
	return 1; // No need to calculate anything here; it's just 1. Even if 1 won't fit, we still want 1 as the number of columns.
	*/
}

/*
// If CreateBorders returns true then all of the image pointers will have been set to either NULL or to images which must be deleted by the caller.
// If CreateBorders returns false then all of the image pointers will have been set to NULL. (The callee must delete any images which were made but then not returned due to later error.)
// The caller of CreateBorders should always delete the returned images (including before another call to CreateBorders).
bool NGifDecoder::CGifImage::CreateBorders(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, bool bUseAlphaChannel,
	CAbstractImage **ppTop, CAbstractImage **ppLeft, CAbstractImage **ppRight, CAbstractImage **ppBottom, bool *pbTopSpans,
	CAbstractImage **ppOutTop, CAbstractImage **ppOutLeft, CAbstractImage **ppOutRight, CAbstractImage **ppOutBottom,
	CAbstractImage **ppOutTopLeft, CAbstractImage **ppOutTopRight, CAbstractImage **ppOutBottomLeft, CAbstractImage **ppOutBottomRight) const
{
	*ppTop = NULL;
	*ppLeft = NULL;
	*ppRight = NULL;
	*ppBottom = NULL;

	*ppOutTop = NULL;
	*ppOutLeft = NULL;
	*ppOutRight = NULL;
	*ppOutBottom = NULL;

	*ppOutTopLeft = NULL;
	*ppOutTopRight = NULL;
	*ppOutBottomLeft = NULL;
	*ppOutBottomRight = NULL;

	SIZE sizeImageBorder;
	SIZE sizeOuterBorder;

	if (!getBorderMetrics(numCols, numRows, &sizeImageBorder, &sizeOuterBorder))
	{
		return false;
	}

	int iSingleBorderWidth  = sizeImageBorder.cx / 2;
	int iSingleBorderHeight = sizeImageBorder.cy / 2;

	int iSingleOuterBorderWidth  = sizeOuterBorder.cx / 2;
	int iSingleOuterBorderHeight = sizeOuterBorder.cy / 2;

	int w = GetWidth();
	int h = GetHeight();

	int newW = w + sizeImageBorder.cx;
	int newH = h + sizeImageBorder.cy;

	int iScaleMode = HALFTONE;

	COLORREF crBack = GetCurrentBackgroundColor();
	RGBQUAD rgbBack;
	rgbBack.rgbRed   = GetRValue(crBack);
	rgbBack.rgbGreen = GetGValue(crBack);
	rgbBack.rgbBlue  = GetBValue(crBack);
	rgbBack.rgbReserved = 255;

	COLORREF crFill = GetAutomaticBackgroundColor();
	RGBQUAD rgbFill;
	rgbFill.rgbRed   = GetRValue(crFill);
	rgbFill.rgbGreen = GetGValue(crFill);
	rgbFill.rgbBlue  = GetBValue(crFill);
	rgbFill.rgbReserved = 255;

	int iFillAlpha = GetHasUsedTransparency() ? 0 : 128;

	if (sizeImageBorder.cy >= sizeImageBorder.cx)
	{
		*pbTopSpans = true;

		*ppTop    = new CSprocketBorderImage(CSprocketBorderImage::TOP_THICK,    newW,               iSingleBorderHeight, iSingleBorderWidth, h,                   iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppLeft   = new CSprocketBorderImage(CSprocketBorderImage::LEFT_THIN,    iSingleBorderWidth, h,                   newW,               iSingleBorderHeight, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppRight  = new CSprocketBorderImage(CSprocketBorderImage::RIGHT_THIN,   iSingleBorderWidth, h,                   newW,               iSingleBorderHeight, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppBottom = new CSprocketBorderImage(CSprocketBorderImage::BOTTOM_THICK, newW,               iSingleBorderHeight, iSingleBorderWidth, h,                   iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);

		*ppOutLeft  = new CSprocketBorderImage(CSprocketBorderImage::LEFT_OUTER,  iSingleOuterBorderWidth, newH, 0, 0, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppOutRight = new CSprocketBorderImage(CSprocketBorderImage::RIGHT_OUTER, iSingleOuterBorderWidth, newH, 0, 0, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
	}
	else
	{
		*pbTopSpans = false;

		*ppTop    = new CSprocketBorderImage(CSprocketBorderImage::TOP_THIN,    w,                  iSingleBorderHeight, iSingleBorderWidth, newH,                iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppLeft   = new CSprocketBorderImage(CSprocketBorderImage::LEFT_THICK,  iSingleBorderWidth, newH,                w,                  iSingleBorderHeight, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppRight  = new CSprocketBorderImage(CSprocketBorderImage::RIGHT_THICK, iSingleBorderWidth, newH,                w,                  iSingleBorderHeight, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppBottom = new CSprocketBorderImage(CSprocketBorderImage::BOTTOM_THIN, w,                  iSingleBorderHeight, iSingleBorderWidth, newH,                iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);

		*ppOutTop    = new CSprocketBorderImage(CSprocketBorderImage::TOP_OUTER,    newW, iSingleOuterBorderHeight, 0, 0, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
		*ppOutBottom = new CSprocketBorderImage(CSprocketBorderImage::BOTTOM_OUTER, newW, iSingleOuterBorderHeight, 0, 0, iScaleMode, bUseAlphaChannel, &rgbBack, &rgbFill, iFillAlpha);
	}

	return true;
}
*/

// If CreateBorders returns true then all of the image pointers will have been set to either NULL or to images which must be deleted by the caller.
// If CreateBorders returns false then all of the image pointers will have been set to NULL. (The callee must delete any images which were made but then not returned due to later error.)
// The caller of CreateBorders should always delete the returned images (including before another call to CreateBorders).
bool NGifDecoder::CGifImage::CreateBorders(bool bUseAlphaChannel, CAbstractImage **ppBorderImage, RECT *pInnerRect, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper) const
{
	*ppBorderImage = NULL;
	::SetRectEmpty(pInnerRect);

	const int iSingleBorderWidth  = OPUSVIEWER_IMAGE_FRAME_SIZE/2;
	const int iSingleBorderHeight = OPUSVIEWER_IMAGE_FRAME_SIZE/2;

	const int iOuterWidth  = OPUSVIEWER_IMAGE_FRAME_SIZE + GetWidth();
	const int iOuterHeight = OPUSVIEWER_IMAGE_FRAME_SIZE + GetHeight();

	pInnerRect->left   = iSingleBorderWidth;
	pInnerRect->right  = iSingleBorderWidth + GetWidth();
	pInnerRect->top    = iSingleBorderWidth;
	pInnerRect->bottom = iSingleBorderWidth + GetHeight();

	if (bFrameImage)
	{
		*ppBorderImage = new CPictureFrameImage(iOuterWidth, iOuterHeight, *pInnerRect, HALFTONE, GetCurrentBackgroundColor(), pPluginHelper);
	}
	else
	{
		RGBQUAD rgbBackground;
		COLORREF cr2 = GetCurrentBackgroundColor();
		rgbBackground.rgbRed   = GetRValue(cr2);
		rgbBackground.rgbGreen = GetGValue(cr2);
		rgbBackground.rgbBlue  = GetBValue(cr2);
		rgbBackground.rgbReserved = 0;

		*ppBorderImage = new CEmptyImage(iOuterWidth, iOuterHeight, HALFTONE, &rgbBackground);
	}

	return true;
}
