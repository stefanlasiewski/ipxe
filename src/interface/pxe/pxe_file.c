/** @file
 *
 * PXE FILE API
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <byteswap.h>
#include <gpxe/uaccess.h>
#include <gpxe/posix_io.h>
#include <gpxe/features.h>
#include <pxe.h>

/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

FEATURE ( FEATURE_MISC, "PXEXT", DHCP_EB_FEATURE_PXE_EXT, 1 );

/**
 * FILE OPEN
 *
 * @v file_open				Pointer to a struct s_PXENV_FILE_OPEN
 * @v s_PXENV_FILE_OPEN::FileName	URL of file to open
 * @ret #PXENV_EXIT_SUCCESS		File was opened
 * @ret #PXENV_EXIT_FAILURE		File was not opened
 * @ret s_PXENV_FILE_OPEN::Status	PXE status code
 * @ret s_PXENV_FILE_OPEN::FileHandle	Handle of opened file
 *
 */
PXENV_EXIT_t pxenv_file_open ( struct s_PXENV_FILE_OPEN *file_open ) {
	userptr_t filename;
	size_t filename_len;
	int fd;

	DBG ( "PXENV_FILE_OPEN" );

	/* Copy name from external program, and open it */
	filename = real_to_user ( file_open->FileName.segment,
			      file_open->FileName.offset );
	filename_len = strlen_user ( filename, 0 );
	{
		char uri_string[ filename_len + 1 ];

		copy_from_user ( uri_string, filename, 0,
				 sizeof ( uri_string ) );
		DBG ( " %s", uri_string );
		fd = open ( uri_string );
	}

	if ( fd < 0 ) {
		file_open->Status = PXENV_STATUS ( fd );
		return PXENV_EXIT_FAILURE;
	}

	DBG ( " as file %d", fd );

	file_open->FileHandle = fd;
	file_open->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * FILE CLOSE
 *
 * @v file_close			Pointer to a struct s_PXENV_FILE_CLOSE
 * @v s_PXENV_FILE_CLOSE::FileHandle	File handle
 * @ret #PXENV_EXIT_SUCCESS		File was closed
 * @ret #PXENV_EXIT_FAILURE		File was not closed
 * @ret s_PXENV_FILE_CLOSE::Status	PXE status code
 *
 */
PXENV_EXIT_t pxenv_file_close ( struct s_PXENV_FILE_CLOSE *file_close ) {

	DBG ( "PXENV_FILE_CLOSE %d", file_close->FileHandle );

	close ( file_close->FileHandle );
	file_close->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * FILE SELECT
 *
 * @v file_select			Pointer to a struct s_PXENV_FILE_SELECT
 * @v s_PXENV_FILE_SELECT::FileHandle	File handle
 * @ret #PXENV_EXIT_SUCCESS		File has been checked for readiness
 * @ret #PXENV_EXIT_FAILURE		File has not been checked for readiness
 * @ret s_PXENV_FILE_SELECT::Status	PXE status code
 * @ret s_PXENV_FILE_SELECT::Ready	Indication of readiness
 *
 */
PXENV_EXIT_t pxenv_file_select ( struct s_PXENV_FILE_SELECT *file_select ) {
	fd_set fdset;
	int ready;

	DBG ( "PXENV_FILE_SELECT %d", file_select->FileHandle );

	FD_ZERO ( &fdset );
	FD_SET ( file_select->FileHandle, &fdset );
	if ( ( ready = select ( &fdset, 0 ) ) < 0 ) {
		file_select->Status = PXENV_STATUS ( ready );
		return PXENV_EXIT_FAILURE;
	}

	file_select->Ready = ( ready ? RDY_READ : 0 );
	file_select->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * FILE READ
 *
 * @v file_read				Pointer to a struct s_PXENV_FILE_READ
 * @v s_PXENV_FILE_READ::FileHandle	File handle
 * @v s_PXENV_FILE_READ::BufferSize	Size of data buffer
 * @v s_PXENV_FILE_READ::Buffer		Data buffer
 * @ret #PXENV_EXIT_SUCCESS		Data has been read from file
 * @ret #PXENV_EXIT_FAILURE		Data has not been read from file
 * @ret s_PXENV_FILE_READ::Status	PXE status code
 * @ret s_PXENV_FILE_READ::Ready	Indication of readiness
 * @ret s_PXENV_FILE_READ::BufferSize	Length of data read
 *
 */
PXENV_EXIT_t pxenv_file_read ( struct s_PXENV_FILE_READ *file_read ) {
	userptr_t buffer;
	ssize_t len;

	DBG ( "PXENV_FILE_READ %d to %04x:%04x+%04x", file_read->FileHandle,
	      file_read->Buffer.segment, file_read->Buffer.offset,
	      file_read->BufferSize );

	buffer = real_to_user ( file_read->Buffer.segment,
				file_read->Buffer.offset );
	if ( ( len = read_user ( file_read->FileHandle, buffer, 0,
				file_read->BufferSize ) ) < 0 ) {
		file_read->Status = PXENV_STATUS ( len );
		return PXENV_EXIT_FAILURE;
	}

	DBG ( " read %04zx", ( ( size_t ) len ) );

	file_read->BufferSize = len;
	file_read->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}

/**
 * GET FILE SIZE
 *
 * @v get_file_size			Pointer to a struct s_PXENV_GET_FILE_SIZE
 * @v s_PXENV_GET_FILE_SIZE::FileHandle	File handle
 * @ret #PXENV_EXIT_SUCCESS		File size has been determined
 * @ret #PXENV_EXIT_FAILURE		File size has not been determined
 * @ret s_PXENV_GET_FILE_SIZE::Status	PXE status code
 * @ret s_PXENV_GET_FILE_SIZE::FileSize	Size of file
 */
PXENV_EXIT_t pxenv_get_file_size ( struct s_PXENV_GET_FILE_SIZE
				   *get_file_size ) {
	ssize_t filesize;

	DBG ( "PXENV_GET_FILE_SIZE %d", get_file_size->FileHandle );

	filesize = fsize ( get_file_size->FileHandle );
	if ( filesize < 0 ) {
		get_file_size->Status = PXENV_STATUS ( filesize );
		return PXENV_EXIT_FAILURE;
	}

	DBG ( " is %zd", ( ( size_t ) filesize ) );

	get_file_size->FileSize = filesize;
	get_file_size->Status = PXENV_STATUS_SUCCESS;
	return PXENV_EXIT_SUCCESS;
}