#pragma once

#include "MemoryImage.h"

namespace NGifDecoder
{
	enum { GD_MAXCOLORMAPSIZE	= 256 };

	class CGifFile
	{
	protected:
		enum { GF_MAX_LWZ_BITS = 12 };
		enum { GF_STACK_SIZE = ((1<<(GF_MAX_LWZ_BITS))*2) };

		enum GF_FileMode { GF_FM_NONE, GF_FM_DISKFILE, GF_FM_STREAM };

	protected:
		GF_FileMode		m_fileMode;
		HANDLE			m_hAbortEvent;
		HANDLE			m_hDiskFile;
		IStream *		m_pStream;

		BYTE			m_gifBuffer[32768];
		DWORD			m_dwBufSize;
		DWORD			m_dwBufPos;

		bool			m_ZeroDataBlock;
		BYTE			m_DoExtension_buf[256];
		BYTE			m_GetCode_buf[280];
		int				m_GetCode_curbit;
		int				m_GetCode_lastbit;
		bool			m_GetCode_done;
		int				m_GetCode_lastbyte;
		bool			m_LWZReadByte_fresh;
		int				m_LWZReadByte_codesize;
		int				m_LWZReadByte_setcodesize;
		int				m_LWZReadByte_maxcode;
		int				m_LWZReadByte_maxcodesize;
		int				m_LWZReadByte_firstcode;
		int				m_LWZReadByte_oldcode;
		int				m_LWZReadByte_clearcode;
		int				m_LWZReadByte_endcode;
		int				m_LWZReadByte_table[2][(1<<GF_MAX_LWZ_BITS)];
		int				m_LWZReadByte_stack[GF_STACK_SIZE];
		int *			m_LWZReadByte_sp;
	public:
		CGifFile(const TCHAR *szFilename, HANDLE hAbortEvent);
		CGifFile(IStream *pStream);
		~CGifFile();

		bool InitOK();

//		bool IsAborted() { return (m_hAbortEvent != NULL && WAIT_OBJECT_0 == WaitForSingleObject(m_hAbortEvent, 0)); }
		bool ReadOK(BYTE *pBufferOut, DWORD dwLen);
		bool ReadColorMap(RGBQUAD *pPalette, int cColours);
		int GetDataBlock(BYTE *pBuf); // pBuf must have at least 255 bytes
		void DoExtension(int label, int *pTransparent, int *pDisposal, int *pDelayTime);
		int GetCode(int code_size, bool bFlag);
		int LWZReadByte(bool bFlag, int input_code_size);
	protected:
		void commonInit();
		bool readData(BYTE *pBufferOut, DWORD dwMaxRead, DWORD *pdwActualReadOut);
	};

	class CGifImage : public CMemoryImage
	{
	public:

	static HBITMAP LoadGifToDIBSection(HDC hDC, NGifDecoder::CGifFile *pGifFile,
									   bool *pbHasUsedTransparency, bool *pbWantFrame, bool *pbRegenOnResize, SIZE *pSizeDesired, bool bThumbnailSprockets);

	// Note that *pbFileHasMultipleImages may be set true while only one image is returned if there's
	//      an error reading the second image
	static bool LoadGifImage(CAbstractImageList *pFrameList,
							 bool bReadOnlyFirstImage, bool *pbFileHasMultipleImages, bool *pbNotAllFramesLoadedDueToMemory,
							 NGifDecoder::CGifFile *pGifFile, const RGBQUAD *prgbViewerBackground, int *pOriginalBitDepth);


	protected:
		enum { GIC_STYLED			= (-2),
			   GIC_BRUSHED			= (-3),
			   GIC_STYLEDBRUSHED	= (-4),
			   GIC_TILED			= (-5),
			   GIC_TRANSPARENT		= (-6) };

		enum { GID_WHATEVER			= 0,
			   GID_LEAVE			= 1,
			   GID_BACKGROUND		= 2,
			   GID_PREVIOUS         = 3,
			   GID_PREVIOUS_ALT		= 4 }; // At least one bad encoder uses 4 instead of 3. :-(

	protected:
		int			m_iLocalLeft;
		int			m_iLocalTop;
		int			m_iLocalWidth;
		int			m_iLocalHeight;

		int			m_iDisposal;
		int			m_iDelayTime;

	protected:

		CGifImage(int iGlobalWidth, int iGlobalHeight,
				  int iLocalLeft, int iLocalTop, int iLocalWidth, int iLocalHeight,
				  int iDisposal, int iDelayTime, int iScaleMode);

		bool ReadImage(CGifFile *pGifFile,
					   bool bInterlace,
					   std::list< CGifImage * > *pImageList,
					   const RGBQUAD *prgbGlobalBackgroundColor,
					   const RGBQUAD *prgbGlobalOriginalTransparentColor,
					   const RGBQUAD *prgbViewerBackground,
					   const RGBQUAD *pPalette);

		void inheritImageBuffer(std::list< CGifImage * > *pImageList,
								const RGBQUAD *prgbGlobalBackgroundColor,
								const RGBQUAD *prgbGlobalOriginalTransparentColor,
								const RGBQUAD *prgbViewerBackground);

	public:

		virtual ~CGifImage();

	private:

		CGifImage(const CGifImage &rhs); // disallow
		CGifImage &operator=(const CGifImage &rhs); // disallow

	public:

		virtual int GetDelayTime() const				{ return(m_iDelayTime);					}

		virtual const TCHAR *GetImageTypeName(LeoHelpers::OpusStringLoader *pSL) const
		{
			return(pSL->Get(STR_GIFANIM_GIF_IMAGE));
		}

		virtual bool RotateImage(int rotationAmount);

		// Most image types will not support borders and will return false for border calls.
		virtual bool WantBorders() const;
		virtual bool NeedNewBorders(CAbstractImageList::size_type oldNumCols, CAbstractImageList::size_type oldNumRows, CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows) const;
		virtual bool GetTotalBorderSize(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, SIZE *pSize) const;
		virtual CAbstractImageList::size_type CalcNumFramesFitHoriz(LONG lSpaceWidth, int iNormalisedZoomFactor, bool bBorders) const;
		// If CreateBorders returns true then all of the image pointers will have been set to either NULL or to images which must be deleted by the caller.
		// If CreateBorders returns false then all of the image pointers will have been set to NULL. (The callee must delete any images which were made but then not returned due to later error.)
		// The caller of CreateBorders should always delete the returned images (including before another call to CreateBorders).
		virtual bool CreateBorders(bool bUseAlphaChannel, CAbstractImage **ppBorderImage, RECT *pInnerRect, bool bFrameImage, DOpusPluginHelperUtil *pPluginHelper) const;

	protected:

		// These numbers must be even as they're divided by 2 to get the size of individual borders.
//		enum { BORDER_THICK = 24 };
//		enum { BORDER_THIN  = 6  };
//		enum { BORDER_OUTER_LEFTRIGHT = 8 };
//		enum { BORDER_OUTER_TOPBOTTOM = 8 };
//
//		bool getBorderMetrics(CAbstractImageList::size_type numCols, CAbstractImageList::size_type numRows, SIZE *pSizeImageBorder, SIZE *pSizeOuterBorder) const;

		inline void setPixel(int x, int y, int iPixelIndex, int color, const RGBQUAD *pPalette)
		{
			switch(color)
			{
			case GIC_STYLED:
			case GIC_STYLEDBRUSHED:
			case GIC_BRUSHED:
			case GIC_TILED:
				break;

			default:
				if (0 <= x && x < m_iWidth
				&&	0 <= y && y < m_iHeight
				&&	0 <= color && color < 256)
				{
					if (0 == pPalette[ color ].rgbReserved)
					{
						// Check if the pixel we inherited is transparent. If so then we use transparency.
						if (m_pPixelData[ iPixelIndex ].rgbReserved == 0)
						{
							m_bTransparencyActuallyUsed = true;
						}
					}
					else
					{
						m_pPixelData[ iPixelIndex ] = pPalette[ color ];
					}
				}
				break;
			}
		}
	};

	inline unsigned int LM_to_uint(unsigned int a, unsigned int b)
	{
		return ((b<<8)|a);
	}
};
