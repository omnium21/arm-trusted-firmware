/*
 * Copyright (c) 2014, ARM Limited and Contributors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of ARM nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific
 * prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include "io_storage.h"
#include "io_driver.h"
#include "semihosting.h"



/* Identify the device type as semihosting */
static io_type device_type_sh(void)
{
	return IO_TYPE_SEMIHOSTING;
}


/* Semi-hosting functions, device info and handle */

static int sh_dev_open(void *spec, struct io_dev_info **dev_info);
static int sh_file_open(struct io_dev_info *dev_info, const void *spec,
		struct io_entity *entity);
static int sh_file_seek(struct io_entity *entity, int mode, ssize_t offset);
static int sh_file_len(struct io_entity *entity, size_t *length);
static int sh_file_read(struct io_entity *entity, void *buffer, size_t length,
		size_t *length_read);
static int sh_file_write(struct io_entity *entity, const void *buffer,
		size_t length, size_t *length_written);
static int sh_file_close(struct io_entity *entity);

static struct io_dev_connector sh_dev_connector = {
	.dev_open = sh_dev_open
};


static struct io_dev_funcs sh_dev_funcs = {
	.type = device_type_sh,
	.open = sh_file_open,
	.seek = sh_file_seek,
	.size = sh_file_len,
	.read = sh_file_read,
	.write = sh_file_write,
	.close = sh_file_close,
	.dev_init = NULL,	/* NOP */
	.dev_close = NULL,	/* NOP */
};


static struct io_dev_info sh_dev_info = {
	.funcs = &sh_dev_funcs,
	.info = (uintptr_t)NULL
};


/* Open a connection to the semi-hosting device */
static int sh_dev_open(void *spec __unused, struct io_dev_info **dev_info)
{
	int result = IO_SUCCESS;
	assert(dev_info != NULL);
	*dev_info = &sh_dev_info;
	return result;
}


/* Open a file on the semi-hosting device */
static int sh_file_open(struct io_dev_info *dev_info __attribute__((unused)),
		const void *spec, struct io_entity *entity)
{
	int result = IO_FAIL;
	int sh_result = -1;
	const io_file_spec *file_spec = (io_file_spec *)spec;

	assert(file_spec != NULL);
	assert(entity != NULL);

	sh_result = semihosting_file_open(file_spec->path, file_spec->mode);

	if (sh_result > 0) {
		entity->info = sh_result;
		result = IO_SUCCESS;
	} else {
		result = IO_FAIL;
	}
	return result;
}


/* Seek to a particular file offset on the semi-hosting device */
static int sh_file_seek(struct io_entity *entity, int mode, ssize_t offset)
{
	int result = IO_FAIL;
	int file_handle, sh_result;

	assert(entity != NULL);

	file_handle = (int)entity->info;

	sh_result = semihosting_file_seek(file_handle, offset);

	result = (sh_result == 0) ? IO_SUCCESS : IO_FAIL;

	return result;
}


/* Return the size of a file on the semi-hosting device */
static int sh_file_len(struct io_entity *entity, size_t *length)
{
	int result = IO_FAIL;

	assert(entity != NULL);
	assert(length != NULL);

	int sh_handle = entity->info;
	int sh_result = semihosting_file_length(sh_handle);

	if (sh_result >= 0) {
		result = IO_SUCCESS;
		*length = (size_t)sh_result;
	}

	return result;
}


/* Read data from a file on the semi-hosting device */
static int sh_file_read(struct io_entity *entity, void *buffer, size_t length,
		size_t *length_read)
{
	int result = IO_FAIL;
	int sh_result = -1;
	int bytes = length;
	int file_handle;

	assert(entity != NULL);
	assert(buffer != NULL);
	assert(length_read != NULL);

	file_handle = (int)entity->info;

	sh_result = semihosting_file_read(file_handle, &bytes, buffer);

	if (sh_result >= 0) {
		*length_read = (bytes != length) ? bytes : length;
		result = IO_SUCCESS;
	} else
		result = IO_FAIL;

	return result;
}


/* Write data to a file on the semi-hosting device */
static int sh_file_write(struct io_entity *entity, const void *buffer,
		size_t length, size_t *length_written)
{
	int result = IO_FAIL;
	int sh_result = -1;
	int file_handle;
	int bytes = length;

	assert(entity != NULL);
	assert(buffer != NULL);
	assert(length_written != NULL);

	file_handle = (int)entity->info;

	sh_result = semihosting_file_write(file_handle, &bytes, buffer);

	if (sh_result >= 0) {
		*length_written = sh_result;
		result = IO_SUCCESS;
	} else
		result = IO_FAIL;

	return result;
}


/* Close a file on the semi-hosting device */
static int sh_file_close(struct io_entity *entity)
{
	int result = IO_FAIL;
	int sh_result = -1;
	int file_handle;

	assert(entity != NULL);

	file_handle = (int)entity->info;

	sh_result = semihosting_file_close(file_handle);

	result = (sh_result >= 0) ? IO_SUCCESS : IO_FAIL;

	return result;
}


/* Exported functions */

/* Register the semi-hosting driver with the IO abstraction */
int register_io_dev_sh(struct io_dev_connector **dev_con)
{
	int result = IO_FAIL;
	assert(dev_con != NULL);

	result = io_register_device(&sh_dev_info);
	if (result == IO_SUCCESS)
		*dev_con = &sh_dev_connector;

	return result;
}
