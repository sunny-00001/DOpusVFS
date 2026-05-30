/* Maya IFF image reading code
 * Copyright (C) 1997-1999 Mike Taylor
 * (email: mtaylor@aw.sgi.com, WWW: http://reality.sgi.com/mtaylor)
 *
 * Modifications copyright (C) 2003 Luke Tokheim
 * (WWW: http://stdout.org/~luke/)
 *
 * Modifications copyright (C) 2008 Leo Davidson
 * (email: leo@ox.compsoc.net, WWW: http://www.pretentiousname.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "stdafx.h"
#include "iffimage.h"


// Leo Davidson 27/Aug/2008: Removed "typedef char byte" as "byte" isn't actually used and it caused a warning due to a different typedef in one of the Windows headers.
//typedef char    byte;
typedef int     int32;
typedef short   int16;
typedef unsigned char uByte;
typedef unsigned int uInt32;
typedef unsigned short uInt16;
typedef float Float32;


#define RGB_FLAG     (1)
#define ALPHA_FLAG   (2)
#define ZBUFFER_FLAG (4)

#define CHUNK_STACK_SIZE (32)


/* Error code definitions */
#define IFF_NO_ERROR     (0)
#define IFF_OPEN_FAILS   (1)
#define IFF_READ_FAILS   (2)
#define IFF_BAD_TAG      (3)
#define IFF_BAD_COMPRESS (4)
#define IFF_BAD_STACK    (5)
#define IFF_BAD_CHUNK    (6)


/* Define the IFF tags we are looking for in the file. */
const uInt32 IFF_TAG_CIMG = ('C' << 24) | ('I' << 16) | ('M' << 8) | ('G'); 
const uInt32 IFF_TAG_FOR4 = ('F' << 24) | ('O' << 16) | ('R' << 8) | ('4'); 
const uInt32 IFF_TAG_TBHD = ('T' << 24) | ('B' << 16) | ('H' << 8) | ('D'); 
const uInt32 IFF_TAG_TBMP = ('T' << 24) | ('B' << 16) | ('M' << 8) | ('P');
const uInt32 IFF_TAG_RGBA = ('R' << 24) | ('G' << 16) | ('B' << 8) | ('A');
const uInt32 IFF_TAG_CLPZ = ('C' << 24) | ('L' << 16) | ('P' << 8) | ('Z');
const uInt32 IFF_TAG_ESXY = ('E' << 24) | ('S' << 16) | ('X' << 8) | ('Y');
const uInt32 IFF_TAG_ZBUF = ('Z' << 24) | ('B' << 16) | ('U' << 8) | ('F');
const uInt32 IFF_TAG_BLUR = ('B' << 24) | ('L' << 16) | ('U' << 8) | ('R');
const uInt32 IFF_TAG_BLRT = ('B' << 24) | ('L' << 16) | ('R' << 8) | ('T');
const uInt32 IFF_TAG_HIST = ('H' << 24) | ('I' << 16) | ('S' << 8) | ('T');


/* For the stack of chunks */
typedef struct _iff_chunk {
	uInt32 tag;
	uInt32 start;
	uInt32 size;
	uInt32 chunkType;
} iff_chunk;

// Leo 28/Aug/2008: Moved data out of global variables to enable concurrent instances use.
typedef struct _iff_instance {
	iff_chunk chunkStack[CHUNK_STACK_SIZE];
	int chunkDepth;
	/* The current error state. */
	unsigned int iff_error;
	/* The file callbacks */
	iff_file_io *pFileIO;
} iff_instance;

// Leo 27/Aug/2008: Many changes (which aren't otherwise indicated by comments like this) to add more error checking.

// -- Function prototypes, local to this file.
int iff_begin_read_chunk( iff_instance *pInstance, iff_chunk *pOutChunk );
int iff_end_read_chunk( iff_instance *pInstance );
uByte * iff_read_data( iff_instance *pInstance, int size );
uByte * iff_decompress_rle( iff_instance *pInstance, uInt32 numBytes, uByte * compressedData, uInt32 compressedDataSize, uInt32 * compressedIndex );
uByte * iff_decompress_tile_rle( iff_instance *pInstance, uInt16 width, uInt16 height, uInt16 depth, uByte* compressedData, uInt32 compressedDataSize );
uByte * iff_read_uncompressed_tile( iff_instance *pInstance, uInt16 width, uInt16 height, uInt16 depth );

/*
* -- Basic input functions.
*
*/

int iff_get_short( iff_instance *pInstance, uInt16 *pOut ) 
{
	uByte buf[2];
	size_t result = 0;

	result = pInstance->pFileIO->read( pInstance->pFileIO->pData, buf, 2, 1 );

	if( result != 1 ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		return( FALSE );
	}

	if (pOut) {
		*pOut = ( ( buf[0] << 8 ) + ( buf[1] ) );
	}
	return( TRUE );
}

int iff_get_long( iff_instance *pInstance, uInt32 *pOut ) 
{
	uByte buffer[4];

	size_t result = pInstance->pFileIO->read( pInstance->pFileIO->pData, buffer, 4, 1 );	
	if( result != 1 ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		return( FALSE );
	}

	if (pOut) {
		*pOut = ( buffer[0] << 24 ) + ( buffer[1] << 16 ) 
			  + ( buffer[2] << 8  ) + ( buffer[3] << 0  );
	}
	return( TRUE );
}

int iff_get_float( iff_instance *pInstance, Float32 *pOut ) 
{
	uByte buffer[4];
	uInt32 value;

	size_t result = pInstance->pFileIO->read( pInstance->pFileIO->pData, buffer, 4, 1 );
	if( result != 1 ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		return( FALSE );
	}

	if (pOut) {
		value = ( buffer[3] << 24 ) + ( buffer[2] << 16 ) 
			  + ( buffer[1] << 8  ) + ( buffer[0] << 0  );

		*pOut = *((Float32 *)&value);
	}
	return( TRUE );
}


/*
* IFF Chunking Routines.
*
*/

int iff_begin_read_chunk( iff_instance *pInstance, iff_chunk *pOutChunk ) 
{
	long tempTell;

	pInstance->chunkDepth++;
	if( (pInstance->chunkDepth >= CHUNK_STACK_SIZE) || (pInstance->chunkDepth < 0) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_BAD_STACK;
		}
		memset( pOutChunk, 0, sizeof(iff_chunk) );
		return( FALSE );
	}

	tempTell = pInstance->pFileIO->tell( pInstance->pFileIO->pData );

	if ( tempTell == -1L ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_BAD_STACK;
		}
		memset( pOutChunk, 0, sizeof(iff_chunk) );
		return( FALSE );
	}

	pInstance->chunkStack[pInstance->chunkDepth].start = tempTell;

	if ( !iff_get_long( pInstance, &pInstance->chunkStack[pInstance->chunkDepth].tag )
	||	 !iff_get_long( pInstance, &pInstance->chunkStack[pInstance->chunkDepth].size ) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		memset( pOutChunk, 0, sizeof(iff_chunk) );
		return( FALSE );
	}

	if( pInstance->chunkStack[pInstance->chunkDepth].tag == IFF_TAG_FOR4 ) {
		// -- We have a form, so read the form type tag as well. 
		if ( !iff_get_long( pInstance, &pInstance->chunkStack[pInstance->chunkDepth].chunkType ) ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			memset( pOutChunk, 0, sizeof(iff_chunk) );
			return( FALSE );
		}
	} else {
		pInstance->chunkStack[pInstance->chunkDepth].chunkType = 0;
	} 

#ifdef __IFF_DEBUG_
	printf( "Beginning Chunk: %c%c%c%c",
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 24) & 0xFF),
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 16) & 0xFF),
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 8) & 0xFF),
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 0) & 0xFF));

	printf("  start: %lu", pInstance->chunkStack[pInstance->chunkDepth].start );
	printf("  size: %lu", pInstance->chunkStack[pInstance->chunkDepth].size );
	if( pInstance->chunkStack[pInstance->chunkDepth].chunkType != 0 ) {
		printf("  type: %c%c%c%c",
			(((&pInstance->chunkStack[pInstance->chunkDepth].chunkType)[0] >> 24) & 0xFF),
			(((&pInstance->chunkStack[pInstance->chunkDepth].chunkType)[0] >> 16) & 0xFF),
			(((&pInstance->chunkStack[pInstance->chunkDepth].chunkType)[0] >> 8) & 0xFF),
			(((&pInstance->chunkStack[pInstance->chunkDepth].chunkType)[0] >> 0) & 0xFF));
	}
	printf( "  depth: %d\n", pInstance->chunkDepth );
#endif

	*pOutChunk = pInstance->chunkStack[pInstance->chunkDepth];
	return( TRUE );
}

int iff_end_read_chunk( iff_instance *pInstance ) 
{
	uInt32 end;
	int part;

	if( (pInstance->chunkDepth >= CHUNK_STACK_SIZE) || (pInstance->chunkDepth < 0) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_BAD_STACK;
		}
		return( FALSE );
	}

	end = pInstance->chunkStack[pInstance->chunkDepth].start + pInstance->chunkStack[pInstance->chunkDepth].size + 8;

	if ( pInstance->chunkStack[pInstance->chunkDepth].chunkType != 0 ) {
		end += 4;
	}
	// Add padding 
	part = end % 4;
	if ( part != 0 ) {
		end += 4 - part;   
	}

	if ( -1 == pInstance->pFileIO->seek( pInstance->pFileIO->pData, end, SEEK_SET ) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_BAD_STACK;
		}
		return( FALSE );
	}

#ifdef __IFF_DEBUG_
	printf( "Closing Chunk: %c%c%c%c\n\n",
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 24) & 0xFF),
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 16) & 0xFF),
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 8) & 0xFF),
		(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 0) & 0xFF) );
#endif

	pInstance->chunkDepth--; 

	return( TRUE );
}

uByte * iff_read_data( iff_instance *pInstance, int size ) 
{
	uByte * buffer = (uByte *)malloc( size * sizeof( uByte ) );
	size_t result = 0;

	if( !buffer ) {
		if ( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		return( 0 );
	}

#ifdef __IFF_DEBUG_
	printf( "read_data  size: %d\n", size );
#endif

	result = pInstance->pFileIO->read( pInstance->pFileIO->pData, buffer, size, 1 );

	if( result != 1 ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		free( buffer );
		return( 0 );
	}

	return( buffer );
}



/*
* Compression Routines 
*
*/

uByte * iff_decompress_rle( iff_instance *pInstance, uInt32 numBytes, 
						   uByte * compressedData,
						   uInt32 compressedDataSize, 
						   uInt32 * compressedIndex ) 
{
	uByte * data = (uByte *)malloc( numBytes * sizeof( uByte ) );
	uByte nextChar, count;
	int i;
	uInt32 byteCount = 0; 

	if( !data ) {
		if ( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		return( 0 );
	}

	memset(data, 0, numBytes * sizeof( uByte ));

#ifdef __IFF_DEBUG_
	printf( "Decompressing data %d\n", numBytes );
#endif

	while ( byteCount < numBytes ) {

		if( *compressedIndex >= compressedDataSize ) {
			break;
		}

		nextChar = compressedData[ *compressedIndex ];
		(*compressedIndex)++;

		count = (nextChar & 0x7f) + 1;
		if ( ( byteCount + count ) > numBytes ) break;

		if ( nextChar & 0x80 ) {

			// We have a duplication run

			nextChar = compressedData[ *compressedIndex ];
			(*compressedIndex)++;

			//assert( ( byteCount + count ) <= numBytes ); 
			for ( i = 0; i < count; ++i ) {
				data[byteCount] = nextChar;
				byteCount++;
			}
		} 
		else {
			// We have a verbatim run 
			for ( i = 0; i < count; ++i ) {

				data[byteCount] = compressedData[ *compressedIndex ];
				(*compressedIndex)++;
				byteCount++;
			}
		}
		assert( byteCount <= numBytes ); 
	}

	return( data );    
}

uByte * iff_decompress_tile_rle( iff_instance *pInstance, 
								uInt16 width, uInt16 height, uInt16 depth,
								uByte * compressedData,
								uInt32 compressedDataSize ) 
{
	uByte * channels[4];
	uByte * data;
	int i, k, row, column;
	uInt32 compressedIndex = 0;

#ifdef __IFF_DEBUG_    
	printf( "Decompressing tile [ %hd, ", width );
	printf( "%hd, ", height );
	printf( "%hd ]\n", depth );
#endif

	// -- Decompress the different channels (RGBA)
	// -- ERROR CHECK, MUST OPERATE ON RGBA !!!
	// Leo Davidson 28/Aug/2008: Allow depths of 3 and 1, and check compressedData too. Not sure why the original code required RGBA as it works fine with RGB and looks like it'd work with grey.
	if( !compressedData || (depth != 4 && depth != 3 && depth != 1)) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_BAD_COMPRESS;
		}
		return( 0 );
	}

	for( i=(depth-1); i >= 0; --i ) {

		channels[i] = iff_decompress_rle( pInstance, width * height, compressedData, compressedDataSize, &compressedIndex );

		if ( !channels[i] ) {
			while( --i >= 0 ) {
				free( channels[i] );
			}
			if ( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			return( 0 );
		}
	}

	// -- Pack all of the channels from the decompression into an RGBA array.
	data = (uByte *)malloc( width * height * depth * sizeof( uByte ) );

	// Leo Davidson 28/Aug/2008: Check malloc result.
	if( !data ) {
		if ( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		for( i=0; i<depth; i++ ) {
			free( channels[i] );
		}
		return( 0 );
	}

	for( row=0; row<height; row++ ) {
		for( column=0; column<width; column++ ) {
			for( k = 0; k < depth; k++) {
				data[depth*(row*width + column) + k] = channels[k][row*width + column];
			}
		}
	}


	for( i=0; i<depth; i++ ) {
		free( channels[i] );
	}

	return( data );   
}

uByte * iff_read_uncompressed_tile( iff_instance *pInstance, 
								   uInt16 width, uInt16 height, uInt16 depth ) 
{
	uByte * data = 0; 
	uByte pixel[4];
	int i, j, d, index;
	size_t result;
	data = (uByte *)malloc( width * height * depth * sizeof( uByte ) );

	// Leo Davidson 28/Aug/2008: Check malloc result.
	if( !data ) {
		if ( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		return( 0 );
	}


#ifdef __IFF_DEBUG_
	printf("Begin reading uncompressed tile\n",NULL);
#endif

	for ( i = 0; i < height; i++)
	{  
		index = i * width * depth;
		for ( j = 0; j < width; ++j ) 
		{ 
			result = pInstance->pFileIO->read( pInstance->pFileIO->pData, pixel, depth, 1 );
			if ( result != 1 ) {
				if ( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				free( data );
				return( 0 );
			}
			for ( d = ( depth - 1 ); d >= 0; --d ) 
			{ 
				data[index] = pixel[d];
				++index;
			}
		}
	}

#ifdef __IFF_DEBUG_
	printf("End reading uncompressed tile\n",NULL);
#endif

	return( data );
}

// Leo Davidson 27/Aug/2008: Changed from filename to a wrapper structure to abstract file IO
// Leo Davidson 27/Aug/2008: Added fWantRGBA, fWantZBuffer, fWantBlurVec, fWantExtraInfo
// Leo Davidson 27/Aug/2008: Added pOutIffError and removed iff_get_error as it wasn't suitable for concurrent instances.
// Leo Davidson 29/Aug/2008: Added pIsOriginal16Bit. (Note that the output is always 8bit and this flag is only useful for reporting info about the file to the user.)
iff_image * iff_load( iff_file_io *pFileIO, unsigned int *pOutIffError, int *pIsOriginal16Bit, int fWantRGBA, int fWantZBuffer, int fWantBlurVec, int fWantExtraInfo )
{
	iff_instance *pInstance;
	iff_chunk chunkInfo;
	iff_image * image;

	// -- Header info.
	uInt32 width, height, depth, npixels;
	uInt32 flags, compress;
	uInt16 tiles;
	uInt16 tbmpfound;

	uInt16 x1, x2, y1, y2, tile_width, tile_height;
	uInt32 tile;
	uInt32 ztile;

	uInt32 iscompressed, i;
	uByte *tileData;
	uInt32 remainingDataSize;

	long oldSpot;
	long tellTemp;
	uInt32 fileLength;

	if (pIsOriginal16Bit) {
		*pIsOriginal16Bit = FALSE;
	}

	if ( !pFileIO ) {
		if (pOutIffError) {
			*pOutIffError = IFF_OPEN_FAILS;
		}
		return( 0 );
	}

	pInstance = malloc( sizeof(iff_instance) );

	if ( !pInstance )
	{
		if (pOutIffError) {
			*pOutIffError = IFF_OPEN_FAILS;
		}
		return( 0 );
	}

	memset( pInstance, 0, sizeof(iff_instance) );
	pInstance->iff_error = IFF_NO_ERROR;
	pInstance->pFileIO = pFileIO;

	// -- Initialize the top of the chunk stack.
	pInstance->chunkDepth = -1;
	image = 0;

	// -- Open the file.    
	if( !pFileIO->open( pFileIO->pData ) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_OPEN_FAILS;
		}
		goto CleanUp;
	}

	// -- File should begin with a FOR4 chunk of type CIMG
	if ( !iff_begin_read_chunk( pInstance, &chunkInfo ) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		goto CleanUp;
	}

	if ( chunkInfo.chunkType != IFF_TAG_CIMG ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			// -- This is not a CIMG, it is not an IFF Image.
			pInstance->iff_error = IFF_BAD_TAG;
		}
		goto CleanUp;
	}

	/*
	* Read the image header
	* OK, we have a FOR4 of type CIMG, look for the following tags
	*		FVER	
	*		TBHD	bitmap header, definition of size, etc.
	*		AUTH
	*		DATE
	*/
	while ( 1 ) {

		if ( !iff_begin_read_chunk( pInstance, &chunkInfo ) ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			goto CleanUp;
		}

		// -- Right now, the only info we need about the image is in TBHD
		// -- so search this level until we find it.
		if( chunkInfo.tag == IFF_TAG_TBHD ) {

			uInt16 bytes = 0;

			// -- Header chunk found
			if ( !iff_get_long(  pInstance, &width )
			||	 !iff_get_long(  pInstance, &height )
			||	 !iff_get_short( pInstance, 0 ) // -- Don't support 
			||	 !iff_get_short( pInstance, 0 ) // -- Don't support 
			||	 !iff_get_long(  pInstance, &flags )
			||	 !iff_get_short( pInstance, &bytes ) // 0 => 8-bit samples, 1 => 16-bit samples. (Our output is 8-bit regardless.)
			||	 !iff_get_short( pInstance, &tiles )
			||	 !iff_get_long(  pInstance, &compress ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}

			if (pIsOriginal16Bit && bytes==1) {
				*pIsOriginal16Bit = TRUE;
			}

#ifdef __IFF_DEBUG_			
			printf( "****************************************\n" );
			printf( "Width: %u\n",width);
			printf( "Height: %u\n",height);
			printf( "flags: 0x%X\n",flags);
			printf( "tiles: %hu\n",tiles);
			printf( "compress: %u\n",compress);
			printf( "****************************************\n" );
#endif

			if ( !iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}

			if( compress > 1 ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_BAD_COMPRESS;
				}
				goto CleanUp;
			}

			break;
		} else {

#ifdef __IFF_DEBUG_
			// Skipping unused data at FOR4 <size> CIMG depth
			printf("Skipping Chunk: %c%c%c%c\n",
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 24) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 16) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 8) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 0) & 0xFF));
#endif

			if ( !iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}
		}
	} /* END find TBHD while loop */


	// -- Number of channels.
	depth = 0;

	if( flags & RGB_FLAG ) {
		depth += 3;
	}

	if( flags & ALPHA_FLAG ) {
		depth += 1;
	}

	if (depth == 0) {
		depth = 1; // If RGB_FLAG isn't set then it's greyscale. (If ALPHA_FLAG is set and RGB_FLAG isn't then I'm not sure what that means. We want to load 1, 3 or 4 channels only.
	}


	npixels = width*height;


	// -- Allocate the image struct.
	image = (iff_image *)malloc( sizeof( iff_image ) ); 

	if( !image ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		goto CleanUp;
	}

	image->width = width;
	image->height = height;
	image->depth = depth;
	image->znear = 0.0;
	image->zfar =  0.0;
	image->zesx = 0.0;
	image->zesy = 0.0;
	image->rgba = 0;
	image->zbuffer = 0;
	image->blurvec = 0;

	// Leo Davidson 27/Aug/2008: If only the header is wanted then we're done.
	if (!fWantRGBA && !fWantZBuffer && !fWantBlurVec && !fWantExtraInfo)
	{
		goto CleanUp;
	}

	if (fWantRGBA)
	{
		image->rgba = (uByte *)malloc( npixels * depth * sizeof( uByte ) );

		if( !(image->rgba) ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			goto CleanUp;
		}

		memset( image->rgba, 0, npixels * depth * sizeof( uByte ) );
	}

	if( fWantZBuffer && ( flags & ZBUFFER_FLAG ) )
	{
		image->zbuffer = (Float32 *)malloc( npixels * sizeof( Float32 ) );

		if( !(image->zbuffer) ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			goto CleanUp;
		}

		memset( image->zbuffer, 0, npixels * sizeof( Float32 ) );
	}

	// -- Assume the next FOR4 of type TBMP
	tbmpfound = 0;


	// Read the tiled image data
	while ( !tbmpfound ) {

		uInt16 tile_area;

		if ( !iff_begin_read_chunk( pInstance, &chunkInfo ) ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			goto CleanUp;
		}

		/*
		* OK, we have a FOR4 of type TBMP, (embedded FOR4)
		* look for the following tags
		*		RGBA	color data,	RLE compressed tiles of 32 bbp data
		*		ZBUF	z-buffer data, 32 bit float values
		*		CLPZ	depth map specific, clipping planes, 2 float values
		*		ESXY	depth map specific, eye x-y ratios, 2 float values
		*		HIST	
		*		VERS
		*		FOR4 <size>	BLUR (twice embedded FOR4)
		*/
		if( chunkInfo.chunkType == IFF_TAG_TBMP ) {
			tbmpfound = 1;

			// Image data found
			tile = 0;
			ztile = 0;


#ifdef __IFF_DEBUG_			
			printf( "Reading image tiles\n" );
#endif


			if( !(flags & ZBUFFER_FLAG) ) {
				ztile = tiles;
			}

			if( depth == 0 ) {
				tile = tiles;
			} 


			// -- Read tiles
			while( ( tile < tiles ) || ( ztile < tiles ) )  {

				if ( !iff_begin_read_chunk( pInstance, &chunkInfo ) ) {
					if( pInstance->iff_error == IFF_NO_ERROR ) {
						pInstance->iff_error = IFF_READ_FAILS;
					}
					goto CleanUp;
				}

				if( !(chunkInfo.tag == IFF_TAG_RGBA) && !(chunkInfo.tag == IFF_TAG_ZBUF) ) {
					if( pInstance->iff_error == IFF_NO_ERROR ) {
						pInstance->iff_error = IFF_BAD_CHUNK;
					}
					goto CleanUp;
				}

				// Get tile size and location info
				if ( !iff_get_short( pInstance, &x1 )
				||	 !iff_get_short( pInstance, &y1 )
				||	 !iff_get_short( pInstance, &x2 )
				||	 !iff_get_short( pInstance, &y2 ) ) {
					if( pInstance->iff_error == IFF_NO_ERROR ) {
						pInstance->iff_error = IFF_READ_FAILS;
					}
					goto CleanUp;
				}

				remainingDataSize = chunkInfo.size - 8;

				tile_width = x2 - x1 + 1;
				tile_height = y2 - y1 + 1;
				tile_area = tile_width * tile_height;

#ifdef __IFF_DEBUG_
				printf( "Tile x1: %hu  ", x1 );
				printf( "y1: %hu  ", y1 );
				printf( "x2: %hu  ", x2 );
				printf( "y2: %hu\n", y2 );			
#endif

				// Leo Davidson 27/Aug/2008: Ensure the tile is within the bounds of the image, else we'll write over some other memory.
				if (x1 > x2 || ((unsigned int)(x1 + 1)) >= image->width
				||	y1 > y2 || ((unsigned int)(y1 + 1)) >= image->height) {
					if( pInstance->iff_error == IFF_NO_ERROR ) {
						pInstance->iff_error = IFF_BAD_CHUNK;
					}
					goto CleanUp;
				}

				// Leo 27/Aug/2008: This used to assume the data was uncompressed if its size was >= the uncompressed size.
				// I've changed it to require the size to be == the uncompressed size after being given a test image which
				// decoded incorrectly because of compressed tiles that were actually larger than the uncompressed size.
				// Seems rather odd for the encoder to do that, and Photoshop's Maya IFF plugin wouldn't load the image at all,
				// but making this change makes everything work (it seems). Presumably if encoders can generate compressed
				// tiles that are larger than the uncompressed size then they can also generate ones which are exactly equal
				// to it. Is there a way to detect whether a tile is compressed other than by its size?
				if( (int)chunkInfo.size == ( tile_width * tile_height * depth + 8 ) ) {
					// -- Compression was not used for this tile.
					iscompressed = 0;
				} else {
					iscompressed = 1;   
				}

				// -- OK, we found an RGBA chunk, eat it.
				if ( chunkInfo.tag == IFF_TAG_RGBA ) {

					// Leo Davidson 27/Aug/2008: Increment tile counter if the depth is zero and we skip the chunk, else we'll never stop looping (unless I've missed something?)
					// Leo Davidson 27/Aug/2008: Skip the chunk if we don't want RGBA.
					if ( depth == 0 || !fWantRGBA )
					{
						if ( !iff_end_read_chunk( pInstance ) ) {
							if( pInstance->iff_error == IFF_NO_ERROR ) {
								pInstance->iff_error = IFF_READ_FAILS;
							}
							goto CleanUp;
						}
						tile++;
						continue;
					}

					tileData = 0;

					if( iscompressed ) {  
						uByte * data = iff_read_data( pInstance, remainingDataSize );
						if( data ) {
							tileData = iff_decompress_tile_rle( pInstance, tile_width, tile_height, depth, data, remainingDataSize );
							free( data );
						}
					} else {
						tileData = iff_read_uncompressed_tile( pInstance, tile_width, tile_height, depth );
					}

					if( !tileData ) {
						if( pInstance->iff_error == IFF_NO_ERROR ) {
							pInstance->iff_error = IFF_READ_FAILS;
						}
						goto CleanUp;
					}

					// Dummy block for stupic C variable scope rules.
					{
						/* Dump RGBA data to our data structure */
						uInt16 i;

						// -- base is the tile offset into the packed array
						uInt32 base = image->depth*(image->width*y1 + x1);

						for( i=0; i<tile_height; i++ ) {
							memcpy( &image->rgba[base + image->depth*i*width],
								&tileData[depth*i*tile_width],
								tile_width*depth*sizeof( uByte ) );
						}
					}

					/* End RGBA dump */						

					free( tileData );
					tileData = 0;

					if ( !iff_end_read_chunk( pInstance ) ) {
						if( pInstance->iff_error == IFF_NO_ERROR ) {
							pInstance->iff_error = IFF_READ_FAILS;
						}
						goto CleanUp;
					}
					tile++;
				} /* END RGBA chunk */

				// -- OK, we found a ZBUF chunk, eat it....hmmm, tasty
				else if( chunkInfo.tag == IFF_TAG_ZBUF ) {

					// Leo Davidson 27/Aug/2008: Skip the chunk if we don't want RGBA.
					if (!fWantZBuffer)
					{
						if ( !iff_end_read_chunk( pInstance ) ) {
							if( pInstance->iff_error == IFF_NO_ERROR ) {
								pInstance->iff_error = IFF_READ_FAILS;
							}
							goto CleanUp;
						}
						ztile++;
						continue;
					}

					tileData = 0;

					// -- Read in the tile data.
					if ( iscompressed )  {
						uByte * data = iff_read_data( pInstance, remainingDataSize );
						if( data ) {
							tileData = iff_decompress_tile_rle( pInstance, tile_width, tile_height, 4, data, remainingDataSize );
							free( data );
						}
					} else {
						tileData = iff_read_uncompressed_tile( pInstance, tile_width, tile_height, 4 );
					}

					if( !tileData ) {
						if( pInstance->iff_error == IFF_NO_ERROR ) {
							pInstance->iff_error = IFF_READ_FAILS;
						}
						goto CleanUp;
					}

					/*
					* Dump DEPTH data into our structure of floats
					*/

					// Dummy block for stupic C variable scope rules.
					{
						int i, j, base;
						uInt32 value, index;

						base = y1*width + x1; // Leo 27/Aug/2008: This was being recalculated for every point. Moved it out of the loop.

						for( i=0; i<tile_height; i++) {
							for( j=0; j<tile_width; j++) {

								index = 4*(i*tile_width + j);

								/*
								* If MSB of float is 1, it is negative
								* as it should be, so let's process it
								* if it's not 1, just set the depth to 0
								*
								* NOTE: depth values are stored as
								* -1/z in light eye space in the IFF file.
								*/

								//if( tileData[index+3] & 0x80 ) {

								value = ( tileData[index+3] << 24) +
										( tileData[index+2] << 16) +
										( tileData[index+1] << 8 ) +
										( tileData[index]	<< 0 );

								image->zbuffer[base + i*width + j] = *((Float32 *)&value);
								//( -1.0f * (*( (Float32 *)&value ) ));

								//} else {
								//image->zbuffer[base + i*width+j] = 0.0;
								//}

							} /* End DEPTH dump */
						}
					}

					free( tileData );

					if ( !iff_end_read_chunk( pInstance ) ) {
						if( pInstance->iff_error == IFF_NO_ERROR ) {
							pInstance->iff_error = IFF_READ_FAILS;
						}
						goto CleanUp;
					}
					ztile++;

				} /* END ZBUF chunk */

			} /* END while TBMP tiles */

		} /* END if TBMP */

		else {

#ifdef __IFF_DEBUG_
			// Skipping unused data IN THE BEGINNING OF THE FILE
			printf( "Skipping Chunk in search of TBMP: %c%c%c%c\n",
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 24) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 16) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 8) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 0) & 0xFF));
#endif

			if ( !iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}
		}
	}

	// Leo Davidson 27/Aug/2008: If nothing else is wanted then we're done.
	if (!fWantBlurVec && !fWantExtraInfo)
	{
		goto CleanUp;
	}

	if ( -1L == (oldSpot  = pFileIO->tell( pFileIO->pData ))
	||	 -1  == pFileIO->seek( pFileIO->pData, 0, SEEK_END )
	||	 -1L == (tellTemp = pFileIO->tell( pFileIO->pData ))
	||	 -1  == pFileIO->seek( pFileIO->pData, oldSpot, SEEK_SET ) ) {
		if( pInstance->iff_error == IFF_NO_ERROR ) {
			pInstance->iff_error = IFF_READ_FAILS;
		}
		goto CleanUp;
	}

	fileLength = tellTemp;

	while( 1 ) {

		if ( !iff_begin_read_chunk( pInstance, &chunkInfo ) ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			goto CleanUp;
		}

		if( chunkInfo.tag == IFF_TAG_CLPZ ) {

			if( !iff_get_float( pInstance, &image->znear )
			||	!iff_get_float( pInstance, &image->zfar )
			||	!iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}

#ifdef __IFF_DEBUG_
			printf( "Got clipping info: %f %f\n", image->znear, image->zfar );
#endif

		} /* END CLPZ chunk */

		else if( chunkInfo.tag == IFF_TAG_ESXY ) {
			if( !iff_get_float( pInstance, &image->zesx )
			||	!iff_get_float( pInstance, &image->zesy )
			||	!iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}

#ifdef __IFF_DEBUG_
			printf( "Got esxy info: %f %f\n", image->zesx, image->zesy );
#endif
		} /* END ESXY chunk */

		else if( chunkInfo.tag == IFF_TAG_FOR4 ) {

			// Leo 27/Aug/2008: Only dig into the IFF_TAG_BLUR if the caller wants the blur vec.
			if ( chunkInfo.chunkType == IFF_TAG_BLUR && fWantBlurVec) {

				// -- FIXME: GET THE BLUR INFO HERE
				if( image->blurvec ) {
					free( image->blurvec );
				}

				while ( 1 ) {

					if ( !iff_begin_read_chunk( pInstance, &chunkInfo ) ) {
						if( pInstance->iff_error == IFF_NO_ERROR ) {
							pInstance->iff_error = IFF_READ_FAILS;
						}
						goto CleanUp;
					}

					if( chunkInfo.tag == IFF_TAG_BLRT ) {

						// read in values, uncompressed and in a linear sort
						// of manner, uh huh...
						/*	
						printf( "%d\n", iff_get_long( pInstance ) );
						printf( "%d\n", iff_get_long( pInstance ) );
						printf( "%d\n", iff_get_long( pInstance ) );
						printf( "%d\n", iff_get_long( pInstance ) );
						*/

						if ( !iff_get_long( pInstance, 0 ) // -- Don't know what these are
						||	 !iff_get_long( pInstance, 0 )
						||	 !iff_get_long( pInstance, 0 )
						||	 !iff_get_long( pInstance, 0 ) ) {
							if( pInstance->iff_error == IFF_NO_ERROR ) {
								pInstance->iff_error = IFF_READ_FAILS;
							}
							goto CleanUp;
						}

						image->blurvec = (Float32 *)malloc( npixels * 2 * sizeof( Float32 ) );

						if ( !(image->blurvec) ) {
							if( pInstance->iff_error == IFF_NO_ERROR ) {
								pInstance->iff_error = IFF_READ_FAILS;
							}
							goto CleanUp;
						}

						for( i=0; i<npixels; i++ ) {
							if( !iff_get_float( pInstance, &image->blurvec[2*i] )
							||	!iff_get_float( pInstance, &image->blurvec[2*i+1] ) ) {
								if( pInstance->iff_error == IFF_NO_ERROR ) {
									pInstance->iff_error = IFF_READ_FAILS;
								}
								goto CleanUp;
							}
						}

						if ( !iff_end_read_chunk( pInstance ) ) {
							if( pInstance->iff_error == IFF_NO_ERROR ) {
								pInstance->iff_error = IFF_READ_FAILS;
							}
							goto CleanUp;
						}

						break;
					}

					else {
#ifdef __IFF_DEBUG_
						printf( "Skipping Chunk in search of BLRT: %c%c%c%c\n",
							(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 24) & 0xFF),
							(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 16) & 0xFF),
							(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 8) & 0xFF),
							(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 0) & 0xFF) );
#endif

						if ( !iff_end_read_chunk( pInstance ) ) {
							if( pInstance->iff_error == IFF_NO_ERROR ) {
								pInstance->iff_error = IFF_READ_FAILS;
							}
							goto CleanUp;
						}
					}
				}
#ifdef __IFF_DEBUG_
				printf("Found FOR4 BLUR\n");
#endif
			}

			if ( !iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}
		}

		else {

#ifdef __IFF_DEBUG_
			// Skipping unused data IN THE BEGINNING OF THE FILE
			printf("Skipping Chunk in search of CLPZ: %c%c%c%c\n",
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 24) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 16) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 8) & 0xFF),
				(((&pInstance->chunkStack[pInstance->chunkDepth].tag)[0] >> 0) & 0xFF));
#endif

			if ( !iff_end_read_chunk( pInstance ) ) {
				if( pInstance->iff_error == IFF_NO_ERROR ) {
					pInstance->iff_error = IFF_READ_FAILS;
				}
				goto CleanUp;
			}
		}

		tellTemp = pFileIO->tell( pFileIO->pData );

		if ( -1L == tellTemp ) {
			if( pInstance->iff_error == IFF_NO_ERROR ) {
				pInstance->iff_error = IFF_READ_FAILS;
			}
			goto CleanUp;
		}

		if( (width*height + tellTemp) > fileLength ) {

#ifdef __IFF_DEBUG_
			printf ( "End of parsable data, time to quit\n" );
#endif

			break ;
		}

	}

CleanUp:

	pFileIO->close( pFileIO->pData );

	if (pOutIffError) {
		*pOutIffError = pInstance->iff_error;
	}

	if ( pInstance->iff_error != IFF_NO_ERROR ) {
		iff_free( image );
		image = 0;
	}

	free( pInstance );

	return( image );
}

void iff_free( iff_image * image )
{
	if( image ) {

		if( image->rgba ) {
			free( image->rgba );
		}
		if( image->zbuffer ) {
			free( image->zbuffer );
		}
		if( image->blurvec ) {
			free( image->blurvec );
		}

		free( image );

		image = 0;
	}
}

/*
// Leo Davidson 27/Aug/2008: Added pOutIffError and removed iff_get_error as it wasn't suitable for concurrent instances.
unsigned iff_get_error( iff_file_io *pFileIO )
{
	unsigned err = pInstance->iff_error;
	pInstance->iff_error = IFF_NO_ERROR;
	return( err );
}
*/

// Leo Davidson 27/Aug/2008: Changed from char to wchar_t
const wchar_t * iff_error_string( unsigned iff_errno )
{
	switch( iff_errno ) {
		case( IFF_NO_ERROR ):
			return( L"no error" );
		case( IFF_OPEN_FAILS ):
			return( L"cannot open file" );
		case( IFF_READ_FAILS ):
			return( L"cannot read file" );
		case( IFF_BAD_TAG ):
			return( L"unexpected tag" );
		case( IFF_BAD_COMPRESS ):
			return( L"unknown compression format" );
		case( IFF_BAD_STACK ):
			return( L"tag stack corrupt" );
		case( IFF_BAD_CHUNK ):
			return( L"unexpected chunk" );
		default:
			return( L"" );
	}
}
