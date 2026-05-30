#include "StdAfx.h"
#include "LeoHelpers.h"
#include "Win32IOWrapper.h"
#include "ReaderPPM.h"

// The Win32IOWrapper does not need to support seeking.
// The lpVPFileInfo and phBitmap parameters can be NULL if not wanted.
bool ReaderPNM::Process(Win32IOWrapper *pIO, CRITICAL_SECTION *pReadCS, LPVIEWERPLUGINFILEINFOW lpVPFileInfo, HBITMAP *phBitmap)
{
	assert(pReadCS==NULL); // Not implemented or thought-through.

	if (phBitmap)
	{
		*phBitmap = NULL;
	}

	bool bRGB = true;
	char tb[4];

	bool bReadError = false;

	// We only support P5 and P6 binary images for now.
	if (3 != pIO->read(tb, 3)
	||	tb[0] != 'P' || (tb[1] != '6' && tb[1] != '5')
	||	!isspace(tb[2]))
	{
		bReadError = true;
	}
	else
	{
		bRGB = (tb[1] == '6');

		std::string strings[3];

		const std::string::size_type stringLimit = 10; // No valid line in the header would be longer than 10 characters (ignoring comments which don't go into any of our buffers).
		std::string::size_type commentLimitRemain = 4096; // Read at most 4KB of comments before giving up on the file. Avoids spending forever on a stupid file with, say, a gig of what looks like comments.

		for(int i = 0; !bReadError && i < _countof(strings); ++i)
		{
			while(true)
			{
				if (stringLimit < strings[i].length()
				||	1 != pIO->read(tb, 1))
				{
					bReadError = true;
					break;
				}

				// "Before the whitespace character that delimits the raster, any characters from a "#" through the next
				// carriage return or newline character, is a comment and is ignored. Note that this is rather unconventional,
				// because a comment can actually be in the middle of what you might consider a token. Note also that this
				// means if you have a comment right before the raster, the newline at the end of the comment is not sufficient
				// to delimit the raster."

				while (tb[0] == '#' && !bReadError) // while-loop handles multiple comments in a row.
				{
					while(tb[0] != '\n' && tb[0] != '\r')
					{
						if (0 >= commentLimitRemain || 1 != pIO->read(tb, 1))
						{
							bReadError = true;
							break;
						}
						--commentLimitRemain;
					}

					if (!bReadError && 1 != pIO->read(tb, 1))
					{
						bReadError = true;
					}
				}

				if (bReadError)
				{
					break;
				}

				if (isspace(tb[0]))
				{
					// At the start of a string skip the space.
					// At the end of a string skip the space and move to the next string.
					if (!strings[i].empty())
					{
						break;
					}
				}
				else
				{
					strings[i].push_back(tb[0]);
				}
			}
		}

		for(int i = 0; !bReadError && i < _countof(strings); ++i)
		{
			if (strings[i].empty())
			{
				bReadError = true;
				break;
			}

			for(std::string::const_iterator strIter = strings[i].begin(); strIter != strings[i].end(); ++strIter)
			{
				if (!isdigit(*strIter))
				{
					bReadError = true;
					break;
				}
			}
		}

		if (!bReadError)
		{
			int iWidth      = atoi(strings[0].c_str());
			int iHeight     = atoi(strings[1].c_str());
			int iMaxColours = atoi(strings[2].c_str());

			if (iWidth      <= 0
			||	iHeight     <= 0 
			||	iMaxColours <= 0 || iMaxColours > 0xFFFF)
			{
				bReadError = true;
			}
			else
			{
				if (lpVPFileInfo != NULL)
				{
					lpVPFileInfo->dwFlags = DVPFIF_CanReturnThumbnail | DVPFIF_CanReturnBitmap;
					lpVPFileInfo->iColorSpace = bRGB ? DVPColorSpace_RGB : DVPColorSpace_Grayscale;
					lpVPFileInfo->wMajorType = DVPMajorType_Image;
					lpVPFileInfo->wMinorType = 0;
					lpVPFileInfo->szImageSize.cx = iWidth;
					lpVPFileInfo->szImageSize.cy = iHeight;

					// Work out how many bits are needed per channel, given the max colour value.
					for(lpVPFileInfo->iNumBits = 1; iMaxColours > (1<<(lpVPFileInfo->iNumBits)); ++(lpVPFileInfo->iNumBits))
					{
					}

					if (bRGB)
					{
						lpVPFileInfo->iNumBits *= 3; // Since it's RGB.
					}

					LeoHelpers::WriteFileInfoInfoLine(lpVPFileInfo, bRGB ? L"PPM Image" : L"PGM Image");
				}

				if (phBitmap)
				{
					// We only support 8-bit greyscale and 24-bit RGB images at the moment as it means the decoder can be much faster than it would be otherwise.
					// Supporting other iMaxColours values would be easy enough but probably isn't that useful. If it is done we should keep
					// the optimized code path for when iMaxColours==255 as it's critical for speedy Raw viewing.
					// The 0xFFFF max width/height are arbitrary.
					if (iWidth  > 0xFFFF
					||	iHeight > 0xFFFF
					||	iMaxColours != 255)
					{
						bReadError = true;
					}
					else
					{
						HDC hDC = ::CreateCompatibleDC(0);

						if (hDC == NULL)
						{
							bReadError = true;
						}
						else
						{
							BITMAPINFO bmi = {0};
							bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
							bmi.bmiHeader.biWidth       = iWidth;
							bmi.bmiHeader.biHeight      = -iHeight;
							bmi.bmiHeader.biPlanes      = 1;
							bmi.bmiHeader.biBitCount    = 24; // Even for greyscale we output 24-bit RGB. We could use indexed 8-bit but it doesn't seem worth bothering with given how rarely we'll encounter PGM images.
							bmi.bmiHeader.biCompression = BI_RGB;

							BYTE *pBits = NULL;

							HBITMAP hbm = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, reinterpret_cast<void**>(&pBits), NULL, 0);

							if (hbm != NULL)
							{
								const size_t outputLineBytes = 3 * iWidth;
								const size_t outputLineBufferPadding = ((outputLineBytes+3)&~3)-outputLineBytes;
								const size_t inputLineBytes = (bRGB ? 3 : 1) * iWidth;
								const size_t inputReadBytes = inputLineBytes * iHeight;

								// If this assert is false (e.g. with a 16-bit image) then we can't do the read & conversion in-place.
								assert(outputLineBytes >= inputLineBytes);

								// Read the entire file in in one go. Reading it line-by-line means that two images being read in parallel thrashes the HDD.
								// After we have read it we will expand each line to include the required padding while also swapping the Red and Blue components.

							//	if (pReadCS != NULL)
							//	{
							//		// Todo: Turn this into a mutex that we wait on with a time limit.
							//		// Maybe it should be a mutex created for each drive as well?
							//		EnterCriticalSection(pReadCS);
							//	}

								if (inputReadBytes != pIO->read(pBits, inputReadBytes))
								{
									bReadError = true;
								}

							//	if (pReadCS != NULL)
							//	{
							//		LeaveCriticalSection(pReadCS);
							//	}

								// Point to the last pixels
								BYTE *pIn  = pBits + inputReadBytes - (bRGB ? 3 : 1);
								BYTE *pOut = pBits + outputLineBytes * iHeight + outputLineBufferPadding * (iHeight-1) - 3;
								register BYTE t;

								if (bRGB)
								{
									for(int i = 0; i < iHeight; ++i)
									{
										if (pOut != pIn)
										{
											for(int j = 0; j < iWidth; ++j)
											{
												// RGB->BGR conversion.
												pOut[0] = pIn[2];
												pOut[1] = pIn[1];
												pOut[2] = pIn[0];
												pIn  -=3;
												pOut -=3;
											}
										}
										else
										{
											for(int j = 0; j < iWidth; ++j)
											{
												// RGB->BGR conversion.
												t = pOut[0];
												pOut[0] = pOut[2];
												pOut[2] = t;
												pOut -= 3;
											}
											pIn = pOut;
										}

										pOut -= outputLineBufferPadding;
									}
								}
								else
								{
									for(int i = 0; i < iHeight; ++i)
									{
										for(int j = 0; j < iWidth; ++j)
										{
											// Greyscale->BGR conversion.
											pOut[0] = pIn[0];
											pOut[1] = pIn[0];
											pOut[2] = pIn[0];
											pIn  -=1;
											pOut -=3;
										}

										pOut -= outputLineBufferPadding;
									}
								}

								if (bReadError)
								{
									DeleteObject(hbm);
								}
								else
								{
									*phBitmap = hbm;
								}
							}

							DeleteDC(hDC);
						}
					}
				}
			}
		}
	}

	return !bReadError;
}
