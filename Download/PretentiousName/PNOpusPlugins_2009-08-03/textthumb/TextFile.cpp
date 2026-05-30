#include "stdafx.h"
#include "LeoHelpers.h"
#include "TextFile.h"

bool CMemoryTextFile::open()
{
	m_i = 0;
	return(true);
}

bool CMemoryTextFile::close()
{
	m_i = 0;
	return(true);
}

bool CMemoryTextFile::GetByteCount(ULARGE_INTEGER *puli)
{
	puli->QuadPart = m_ulBufferSize;

	return(true);
}

bool CMemoryTextFile::read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *ulBytesRead)
{
	bool bResult = true;

	*ulBytesRead = 0;

	while(*ulBytesRead < ulBytesToRead)
	{
		if ( m_i >= m_ulBufferSize )
		{
			bResult = false;
			break;
		}

		pBuffer[ (*ulBytesRead)++ ] = m_pBuffer[ m_i++ ];
	}

	return(bResult);
}


bool CFileTextFile::open()
{
	bool bResult = false;

	if (close())
	{
#ifdef UNICODE
		m_hFile = CreateFile(m_wstrFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
#else
		m_hFile = CreateFile(m_strFilename.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
#endif

		if (INVALID_HANDLE_VALUE != m_hFile)
		{
			bResult = true;
		}
	}

	return(bResult);
}

bool CFileTextFile::close()
{
	bool bResult = true;

	if (INVALID_HANDLE_VALUE != m_hFile)
	{
		if (!CloseHandle(m_hFile))
		{
			bResult = false;
		}

		m_hFile = INVALID_HANDLE_VALUE;
	}

	return(bResult);
}

bool CFileTextFile::GetByteCount(ULARGE_INTEGER *puli)
{
	bool bResult = false;

	if (INVALID_HANDLE_VALUE != m_hFile)
	{
		DWORD dwHighWord = 0;
		DWORD dwLowWord = GetFileSize(m_hFile, &dwHighWord);

		if (dwLowWord != INVALID_FILE_SIZE || NO_ERROR == GetLastError())
		{
			puli->LowPart = dwLowWord;
			puli->HighPart = dwHighWord;
			bResult = true;
		}
	}

	return(bResult);
}

bool CFileTextFile::read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *pulBytesRead)
{
	bool bResult = false;

	if (INVALID_HANDLE_VALUE != m_hFile)
	{
		if (ReadFile(m_hFile, pBuffer, ulBytesToRead, pulBytesRead, NULL))
		{
			bResult = true;
		}
	}

	return(bResult);
}



bool CStreamTextFile::open()
{
	return true;
}

bool CStreamTextFile::close()
{
	return true;
}

bool CStreamTextFile::GetByteCount(ULARGE_INTEGER *puli)
{
	bool bResult = false;

	STATSTG streamStats;
	ZeroMemory(&streamStats, sizeof(streamStats));

	// If STATFLAG_NONAME isn't used we would have to CoTaskMemFree(streamStats.pwcsName)

	if (S_OK == m_pStream->Stat(&streamStats, STATFLAG_NONAME))
	{
		puli->QuadPart = streamStats.cbSize.QuadPart;
		bResult = true;
	}

	return(bResult);
}

bool CStreamTextFile::read(BYTE *pBuffer, ULONG ulBytesToRead, ULONG *pulBytesRead)
{
	bool bResult = false;

	if (S_OK == m_pStream->Read(pBuffer, ulBytesToRead, pulBytesRead))
	{
		bResult = true;
	}

	return(bResult);
}




bool CBaseTextFile::ReadLines(std::wstring *pwstrText, int iLines, unsigned int iMaxLength, WCHAR wcSeparator, bool bSkipSpaces, bool bSkipBlankLines, bool bAlwaysTruncate, DWORD dwCodePage)
{
	bool bResult = true;

	pwstrText->erase();

	ULARGE_INTEGER uliFileSizeBytes;
	uliFileSizeBytes.QuadPart = 0;

	if (!this->open())
	{
		bResult = false;
	}
	else
	{
		if (!this->GetByteCount(&uliFileSizeBytes))
		{
			bResult = false;
		}
		else
		{
			unsigned int iBytesLeft = static_cast<unsigned int>(uliFileSizeBytes.QuadPart);

			if (iMaxLength > m_iMaxLength || 0 == iMaxLength)
			{
				iMaxLength = m_iMaxLength;
			}

			bool bMaxLengthReached = false;
			bool bCheckedHeader = false;
			bool bUnicode = false;
			bool bUnicodeReverse = false;

			if (iBytesLeft > 0)
			{
				int iLineCount = 0;

				BYTE buffer[512];
				const unsigned int iBufferReadSize = sizeof(buffer);

				ULONG ulBytesRead;

				bool bLastWasReturn = true;
				bool bLastWasSpace = true;
				bool bEncounteredBadCharacter = false;
				ULONG ulPrintableCount = 0;
				ULONG ulNonPrintableCount = 0;

				// If we get part of a multi-byte/word UTF-8 or UTF-16 character at the end of a read,
				// save the bytes in here to put at the start of the next read.
				ULONG ulLeftOverByteCount;
				std::vector< unsigned char > vecLeftoverBytes;

				while(true)
				{
					ulLeftOverByteCount = 0;

					for(std::vector< unsigned char >::const_iterator vlbIter = vecLeftoverBytes.begin(); vlbIter != vecLeftoverBytes.end(); ++vlbIter)
					{
						buffer[ ulLeftOverByteCount++ ] = *vlbIter;
					}

					if (!this->read(buffer + ulLeftOverByteCount, (iBufferReadSize - ulLeftOverByteCount) < iBytesLeft ? (iBufferReadSize - ulLeftOverByteCount) : iBytesLeft, &ulBytesRead)
					||	0 == ulBytesRead)
					{
						// If there was a real error, still return success if we got some text out.
						bResult = !pwstrText->empty();
						break;
					}
					else
					{
						ulBytesRead += ulLeftOverByteCount;
						vecLeftoverBytes.clear();

						ULONG i = 0;

						if (!bCheckedHeader)
						{
							if (ulBytesRead >= 2)
							{
								// Check for Unicode UTF-16 and UTF-8 text-file markers.
								// UTF-32 isn't supported for now.

								if (buffer[0] == 0xFF && buffer[1] == 0xFE)
								{
									bUnicode = true;
									i += 2;
								}
								else if (buffer[0] == 0xFE && buffer[1] == 0xFF)
								{
									bUnicode = true;
									bUnicodeReverse = true; // opposite byte order
									i += 2;
								}
								else if (ulBytesRead >= 3 && buffer[0] == 0xEF && buffer[1] == 0xBB && buffer[2] == 0xBF)
								{
									dwCodePage = CP_UTF8;
									i += 3;
								}
							}

							bCheckedHeader = true;
						}

						// If we appear to have read up to the middle of a multi-byte/word sequence, remove it from the current
						// read buffer and save it for the next read.

						if (!bUnicode)
						{
							// Look for UTF-8 multi-byte characters at the end of the buffer.
							//
							// Char. number range  |        UTF-8 octet sequence
							//    (hexadecimal)    |              (binary)
							// --------------------+---------------------------------------------
							// 0000 0000-0000 007F | 0xxxxxxx
							// 0000 0080-0000 07FF | 110xxxxx 10xxxxxx
							// 0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
							// 0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

							ULONG startOfEndIdx = ((ulBytesRead - i) > 3) ? (ulBytesRead - 3) : i;
							ULONG endIdx = ulBytesRead - 1;
							int numMB = 0;

							while(startOfEndIdx <= endIdx)
							{
								unsigned char uc = reinterpret_cast<unsigned char *>(buffer)[ endIdx ];

								// 0x80 = 1000000
								// 0xC0 = 1100000

								if (0x80 == (uc & 0xC0)) // uc is 10xxxxxx
								{
									numMB++; // This belongs to something before it.
									endIdx--;
								}
								else if (0xC0 == (uc & 0xC0)) // uc is 11xxxxxx
								{
									int expectedChars = 1;
									while(0 != ((uc<<expectedChars) & 0x80)) // 1xxxxxxx
									{
										if (++expectedChars > 4) { break; }
									}

									if (expectedChars != 1 && expectedChars <= 4 && expectedChars != numMB + 1)
									{
										// This looks like UTF-8 and we have split something in two.
										// Take it out of this read and keep it to add to the start of the next one.
										while(endIdx < ulBytesRead)
										{
											vecLeftoverBytes.push_back( buffer[endIdx++] );
										}
										ulBytesRead -= static_cast<ULONG>(vecLeftoverBytes.size());
									}

									break; // Whatever happened we don't need to look further back.
								}
								else
								{
									break; // This isn't UTF-8 or it is UTF-8 but we didn't split anything in two.
								}
							}
						}
						else // Unicode
						{
							// If we read an odd number of chars then we can save the last one straight away.

							bool bSavedLast = false;
							char cLast = '\0';

							if (((ulBytesRead - i) >= 1) && 0 != (ulBytesRead & 1)) // The >= 1 check should be redundant but doesn't hurt.
							{
								cLast = buffer[ --ulBytesRead ];
								bSavedLast = true;
							}

							// If the last 2 bytes are the first half of a surrogate then save them for the next read.
							if ((ulBytesRead - i) >= 2)
							{
								BYTE wcBuffer[2];
								WCHAR *pwc = reinterpret_cast<WCHAR *>(wcBuffer);

								if (bUnicodeReverse)
								{
									wcBuffer[0] = buffer[ ulBytesRead - 1 ];
									wcBuffer[1] = buffer[ ulBytesRead - 2 ];
								}
								else
								{
									wcBuffer[0] = buffer[ ulBytesRead - 2 ];
									wcBuffer[1] = buffer[ ulBytesRead - 1 ];
								}

								if (*pwc >= 0xD800 && *pwc <= 0xDBFF)
								{
									// We've got half a surrogate. Remove it from this read and save it for the next one.
									vecLeftoverBytes.push_back( buffer[ ulBytesRead - 2] );
									vecLeftoverBytes.push_back( buffer[ ulBytesRead - 1] );
									ulBytesRead -= 2;
								}
							}

							if (bSavedLast)
							{
								vecLeftoverBytes.push_back( cLast );
							}
						}

						if (i == ulBytesRead)
						{
							break;
						}

						// Convert the buffer into a UTF-16LE

						LeoHelpers::MultiArrayScoper arrayCleanup;

						ULONG goodBufferIdx = 0;
						ULONG goodBufferWcharCount = 0;
						WCHAR *goodBuffer = NULL;

						if (!bUnicode)
						{
							goodBufferWcharCount = LeoHelpers::MBtoWC(&goodBuffer, reinterpret_cast<char *>(buffer + i), ulBytesRead - i, dwCodePage);

							assert(goodBufferWcharCount == 0 || goodBuffer[goodBufferWcharCount-1]==L'\0');

							if (goodBufferWcharCount > 0 && goodBuffer[goodBufferWcharCount-1]==L'\0')
							{
								--goodBufferWcharCount; // We don't want to include the null at the end in our character count.
							}

							// delete[] goodBufferWcharCount when arrayCleanup goes out of scope. goodBufferWcharCount can be null here.
							arrayCleanup.Add(goodBuffer);
						}
						else // Unicode
						{
							if (bUnicodeReverse)
							{
								for (ULONG revIdx = i; revIdx < ulBytesRead; revIdx += 2)
								{
									BYTE bt = buffer[ revIdx ];
									buffer[ revIdx ] = buffer[ revIdx + 1 ];
									buffer[ revIdx + 1 ] = bt;
								}
							}

							goodBufferWcharCount = (ulBytesRead - i) / 2;
							goodBuffer = reinterpret_cast<WCHAR *>(buffer + i);
						}

						if (goodBufferWcharCount == 0 || goodBuffer == NULL)
						{
							break;
						}

						// Finally, we have a UTF16LE buffer that we can actually copy characters out of.

						while(goodBufferIdx < goodBufferWcharCount)
						{
							WCHAR wc = goodBuffer[ goodBufferIdx++ ];

							if (wc == L'\n' || wc == L'\r')
							{
								ulPrintableCount++;

								if (wc == L'\n' && ((!bLastWasReturn) || (!bSkipBlankLines)))
								{
									if (! (bSkipSpaces && bLastWasSpace) )
									{
										if (pwstrText->length() >= (iMaxLength-1))
										{
											bMaxLengthReached = true;
											break;
										}
										iLineCount++;
										bLastWasReturn = true;
										bLastWasSpace = true;
										*pwstrText += wcSeparator;
									}
								}
							}
							else if (wc == L'\0')
							{
								bEncounteredBadCharacter = true;
								break;
							}
							else
							{
								bool bIsPrintable = true;

								bool bSpace = (0 == iswspace( wc ) ? false : true);

								if (bSpace || iswprint( wc ))
								{
									bIsPrintable = true;
								}
								else if (wc >= 0xD800 && wc <= 0xDBFF)
								{
									if (goodBufferIdx < goodBufferWcharCount
									&&	goodBuffer[goodBufferIdx] >= 0xDC00
									&&	goodBuffer[goodBufferIdx] <= 0xDFFF)
									{
										bIsPrintable = true; // Are surrogates always printable?
									}
								}
								else if (wc >= 0xDC00 && wc <= 0xDFFF)
								{
									if (goodBufferIdx > 1
									&&	goodBuffer[goodBufferIdx - 2] >= 0xD800
									&&	goodBuffer[goodBufferIdx - 2] <= 0xDBFF)
									{
										bIsPrintable = true; // Are surrogates always printable?
									}
								}

								if (bIsPrintable)
								{
									ulPrintableCount++;
								}
								else
								{
									ulNonPrintableCount++;
									wc = L'?';
								}

								if (!( bSpace && bSkipSpaces && bLastWasSpace ))
								{
									if (pwstrText->length() >= (iMaxLength-1))
									{
										bMaxLengthReached = true;
										break;
									}
									bLastWasReturn = false;
									bLastWasSpace = bSpace;
									*pwstrText += (bSpace && bSkipSpaces ? L' ' : wc);
								}
							}
						}

						if (bEncounteredBadCharacter || bMaxLengthReached || (0 != iLines && iLineCount >= iLines) || ulBytesRead >= iBytesLeft)
						{
							break;
						}

						iBytesLeft -= ulBytesRead;
					}
				}

				// Monty, you terrible count!
				ULONG ulTotalCount = ulPrintableCount + ulNonPrintableCount;

				if (bEncounteredBadCharacter || ulTotalCount < (ulNonPrintableCount*100)/3)
				{
					pwstrText->erase();
					bResult = false;
				}
				else if ( iMaxLength > 25 && pwstrText->length() > 10 && (bMaxLengthReached || bAlwaysTruncate) )
				{
					std::wstring::size_type iSpacePos = pwstrText->length() - 3;

					if (bSkipSpaces)
					{
						while( iSpacePos > 25 && !iswspace( (*pwstrText)[ iSpacePos ] ) )
						{
							iSpacePos--;
						}

						while( iSpacePos > 25 && L'.' == (*pwstrText)[ iSpacePos - 1 ] )
						{
							iSpacePos--;
						}
					}
					else if (wcSeparator != (*pwstrText)[ iSpacePos - 1 ])
					{
						(*pwstrText)[ iSpacePos - 1 ] = wcSeparator;
					}

					(*pwstrText)[ iSpacePos     ] = L'.';
					(*pwstrText)[ iSpacePos + 1 ] = L'.';
					(*pwstrText)[ iSpacePos + 2 ] = L'.';
					pwstrText->resize( iSpacePos + 3 );
				}
			}
		}
		this->close();
	}
	return(bResult);
}
