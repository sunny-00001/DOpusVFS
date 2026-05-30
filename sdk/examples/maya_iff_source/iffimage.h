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


#ifndef __IFF_IMAGE_H_
#define __IFF_IMAGE_H_

/* Define this to print image and debug info during image load. */
//#define __IFF_DEBUG_

/*!
\file iffimage.h
\author Luke Tokheim
\date May 22, 2003
\ingroup iffimage

*/

/*!
\mainpage Maya IFF Image Reader Documentation
\section intro Introduction

Here is my attempt to create a nice Maya IFF Image loader.
I thought that it would be nice to be able to access all of 
the nice things that Maya can export in the image, and do 
whatever you want with them. For example, post-process motion
blur or depth-compositing.

*/

/*!
\defgroup  iffimage iffimage
\brief  Routines for loading Maya IFF image files.

Example usage:

\code
iff_image * image = iff_load( "file.iff" );
if( image ) {
if( image->rgba ) {
// -- Process RGBA ...
}
if( image->zbuffer ) {
// -- Process Z-Buffer ...
}
if( image->blurvec ) {
// -- Process Blur Vectors ...
}

iff_free( image );

} else {
printf( "Error: %s\n", iff_error_string( iff_get_error() ) );
}
\endcode

*/ //! @{

#if defined(__cplusplus)
extern "C" {
#endif

	// Leo Davidson 27/Aug/2008: Abstracted file IO
	typedef struct _iff_file_io {
		/* Data for the caller, passed to the callback functions */
		void *pData;
		/* Callback functions */
		int (*open)(void *pData);
		int (*close)(void *pData);
		size_t (*read)(void *pData, void *pBuffer, size_t eleementSize, size_t elementCount);
		int (*seek)(void *pData, long offset, int origin);
		long (*tell)(void *pData);
	} iff_file_io;

	/*!
	\brief  Structure for all of the Maya IFF data.

	*/
	typedef struct _iff_image {
		unsigned width;   //!< Image width.
		unsigned height;  //!< Image height.
		unsigned depth;   //!< Color format, bytes per color pixel.
		unsigned char * rgba;   //!< The color data of size width*height*depth.
		float znear;  //!< The near clipping plane for the z buffer data.
		float zfar;   //!< The far clipping plane for the z buffer data.
		/*!
		Access pixel x,y as zbuffer[width*y + x]. (Starting from the top left.) 

		The stored values now are -1/z components in light eye space. When 
		reading a z depth value back, one thus need to compute (-1.0 / z) 
		to get the real z component in eye space. This format is the same as 
		the camera depth format. This format is always used, whatever the light 
		type is. In order to be able to compute real 3D distances of the shadow 
		samples there's an IFF tag: 'ESXY' 'Eye Space X Y' values are two float 
		values, the first one is the width size, the second one is the height size 
		matching the map width and map height. When reading the shadow map buffer, 
		one can thus convert from the pixel coordinates to the light eye space 
		xy coords. If the pixel coordinates are considered to be normalized in 
		the [-1.0, 1.0] range, the eye space xy are given by:

		X_light_eye_space = 3D normalized_pixel_x * (width / 2.0)

		Y_light_eye_space = 3D normalized_pixel_y * (heigth / 2.0)

		Once one get the (X,Y,Z) light eye space coordinates, the true 3D 
		distance from the light source is:

		true_distance = 3D sqrt(X=B2 + Y=B2 + Z=B2)

		(This was copied from the Alias|Wavefront API Knowledge Base.)
		*/
		float * zbuffer; //!< The z buffer data of size width*height.
		/*! 
		Eye x-ratio. This is a depth map specific field used to 
		compute the xy eye coordinates from the normalized pixel. 
		*/
		float zesx; 
		/*! 
		Eye y-ratio. This is a depth map specific field used to 
		compute the xy eye coordinates from the normalized pixel.
		*/
		float zesy;
		/*!
		This does not work right now! I do not know how to interpret these vectors.
		If you can figure it out, please let me know.
		*/
		float * blurvec; //!< The packed xy motion blur vectors of size 2*width*height.
	} iff_image;

	/*!
	\brief Load a Maya IFF image.

	\param  filename  Filename of a Maya IFF image.
	\return  A pointer to an iff_image struct, with the members initialized
	by whatever the file had to offer.


	The client is responsible for calling iff_free on the pointer returned 
	by this function. The client cannot specify the type of data to be read
	from the file. The file determines the format of the data. This may
	lead to a lot of data being read in (like blur vectors) even though 
	only the color data is wanted. This is to simplify the loading code 
	and there could certainly be a 'format' argument to this function.

	*/
	// Leo Davidson 27/Aug/2008: Changed from filename to a wrapper structure to abstract file IO
	// Leo Davidson 27/Aug/2008: Added fWantRGBA, fWantZBuffer, fWantBlurVec, fWantExtraInfo
	// Leo Davidson 27/Aug/2008: Added pOutIffError and removed iff_get_error as it wasn't suitable for concurrent instances.
	// Leo Davidson 29/Aug/2008: Added pIsOriginal16Bit. (Note that the output is always 8bit and this flag is only useful for reporting info about the file to the user.)
	iff_image * iff_load( iff_file_io *pFileIO, unsigned int *pOutIffError, int *pIsOriginal16Bit, int fWantRGBA, int fWantZBuffer, int fWantBlurVec, int fWantExtraInfo );

	/*!
	\brief  Free the memory in an iff_image struct.

	\param  image  An iff_image pointer, most likely created by an earlier
	call to iff_load.


	Leo Davidson 27/Aug/2008: This comment was here but is not true as the function takes a pointer not a pointer-to-a-pointer: "After a call to iff_free(p), p will set to null."

	*/
	void iff_free( iff_image * image );

	/*!
	\brief  Return the value of the error flag.

	\return  IFF_NO_ERROR if there is no error, otherwise
	an error code.

	When an error occurs, the error flag is set to the
	appropriate error code value. No other errors are
	recorded until iff_get_error is called, the error code is
	returned, and the flag is reset to IFF_NO_ERROR.
	*/
	// Leo Davidson 27/Aug/2008: Added pOutIffError and removed iff_get_error as it wasn't suitable for concurrent instances.
	// unsigned iff_get_error( iff_file_io *pFileIO );

	/*!
	\brief  Return an informative string for an error.

	\param  errno Error code, probably from a call to iff_get_error().

	\return String describing error.

	*/
	// Leo Davidson 27/Aug/2008: Changed from char to wchar_t
	const wchar_t * iff_error_string( unsigned iff_errno );

#if defined(__cplusplus)
}
#endif

//! @}

#endif
