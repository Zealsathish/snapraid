/*
 * Copyright (C) 2011 Andrea Mazzoleni
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __STREAM_H
#define __STREAM_H

#include "util.h"

/****************************************************************************/
/* stream */

/**
 * Size of the buffer of the stream.
 *
 * It's not a constant for testing purpose.
 */
unsigned STREAM_SIZE;

#define STREAM_STATE_READ 0 /**< The stream is in a normal state of read. */
#define STREAM_STATE_WRITE 1 /**< The stream is in a normal state of write. */
#define STREAM_STATE_ERROR -1 /**< An error was encoutered. */
#define STREAM_STATE_EOF 2 /**< The end of file was encoutered. */

struct stream_handle {
	int f; /**< Handle of the file. */
	char path[PATH_MAX]; /**< Path of the file. */
};

struct stream {
	unsigned char* buffer; /**< Buffer of the stream. */
	unsigned char* pos; /**< Current position in the buffer. */
	unsigned char* end; /**< End position of the buffer. */
	int state; /**< State of the stream. One of STREAM_STATE. */
	int state_index; /**< Index of the handle causing a state change. */
	unsigned handle_size; /**< Number of handles. */
	struct stream_handle* handle; /**< Set of handles. */
	off_t offset; /**< Offset into the file. */
	off_t offset_uncached; /**< Offset into the file excluding the cached data. */

	/**
	 * CRC of the data read or written in the file.
	 *
	 * If reading, it's the CRC of all data read from the file,
	 * including the one in the buffer.
	 * If writing it's all the data wrote to the file,
	 * excluding the one still in buffer yet to be written.
	 */
	uint32_t crc;

	/**
	 * CRC of the file excluding the cached data in the buffer.
	 *
	 * If reading, it's the CRC of the data read from the file,
	 * excluding the one in the buffer.
	 * If writing it's all the data wrote to the file,
	 * excluding the one still in buffer yet to be written.
	 */
	uint32_t crc_uncached;

	/**
	 * CRC of the data written to the stream.
	 *
	 * This is an extra check of the data that is written to
	 * file to ensure that it's consistent even in case
	 * of memory errors.
	 *
	 * This extra check takes about 2 seconds for each GB of
	 * content file with the Intel CRC instruction,
	 * and about 4 seconds without it.
	 * But usually this doesn't slow down the write process,
	 * as the disk is the bottle-neck.
	 *
	 * Note that this CRC doesn't have the IV processing.
	 *
	 * Not used in reading.
	 * In writing, it's all the data wrote calling sput() functions.
	 */
	uint32_t crc_stream;
};

/**
 * Opaque STREAM type. Like ::FILE.
 */
typedef struct stream STREAM;

/**
 * Opens a stream for reading. Like fopen("r").
 */
STREAM* sopen_read(const char* file);

/**
 * Opens a stream for writing. Like fopen("w").
 */
STREAM* sopen_write(const char* file);

/**
 * Opens a set of streams for writing. Like fopen("w").
 */
STREAM* sopen_multi_write(unsigned count);

/**
 * Specifies the file to open.
 */
int sopen_multi_file(STREAM* s, unsigned i, const char* file);

/**
 * Closes a stream. Like fclose().
 */
int sclose(STREAM* s);

/**
 * Returns the handle of the file.
 * In case of multi file, the first one is returned.
 */
int shandle(STREAM* s);

/**
 * Fills the read stream buffer and read a char.
 * \note Don't call this directly, but use sgetc().
 * \return The char read, or EOF on error.
 */
int sfill(STREAM* s);

/**
 * Flushes the write stream buffer.
 * \return 0 on success, or EOF on error.
 */
int sflush(STREAM* s);

/**
 * Gets the file pointer.
 */
int64_t stell(STREAM* s);

/**
 * Gets the CRC of the processed data.
 */
uint32_t scrc(STREAM* s);

/**
 * Gets the CRC of the processed data in put.
 */
uint32_t scrc_stream(STREAM* s);

/**
 * Checks if the buffer has enough data loaded.
 */
static inline int sptrlookup(STREAM* s, int size)
{
	return s->pos + size <= s->end;
}

/**
 * Gets the current stream ptr.
 */
static inline unsigned char* sptrget(STREAM* s)
{
	return s->pos;
}

/**
 * Sets the current stream ptr.
 */
static inline void sptrset(STREAM* s, unsigned char* ptr)
{
	s->pos = ptr;
}

/**
 * Checks the error status. Like ferror().
 */
static inline int serror(STREAM* s)
{
	return s->state == STREAM_STATE_ERROR;
}

/**
 * Checks the eof status. Like feof().
 */
static inline int seof(STREAM* s)
{
	return s->state == STREAM_STATE_EOF;
}

/**
 * Gets the index of the handle that caused the error.
 */
static inline int serrorindex(STREAM* s)
{
	return s->state_index;
}

/**
 * Gets the path of the handle that caused the error.
 */
static inline const char* serrorfile(STREAM* s)
{
	return s->handle[s->state_index].path;
}

/**
 * Sync the stream. Like fsync().
 */
int ssync(STREAM* s);

/****************************************************************************/
/* get */

/**
 * Reads a char. Like fgetc().
 */
static inline int sgetc(STREAM* s)
{
	if (s->pos == s->end)
		return sfill(s);
	return *s->pos++;
}

/**
 * Unreads a char.
 * Like ungetc() but you have to unget the same char read.
 */
static inline void sungetc(int c, STREAM* s)
{
	if (c != EOF)
		--s->pos;
}

/**
 * Reads a fixed amount of chars.
 * Returns 0 on success, or -1 on error.
 */
int sread(STREAM* f, void* void_data, unsigned size);

/**
 * Gets a char from a stream, ignoring one '\r'.
 */
static inline int sgeteol(STREAM* f)
{
	int c;

	c = sgetc(f);
	if (c == '\r')
		c = sgetc(f);

	return c;
}

/**
 * Reads all the spaces and tabs.
 * Returns the number of spaces and tabs read.
 */
static inline int sgetspace(STREAM* f)
{
	int count = 0;
	int c;

	c = sgetc(f);
	while (c == ' ' || c == '\t') {
		++count;
		c = sgetc(f);
	}

	sungetc(c, f);
	return count;
}

/**
 * Reads until the first space or tab.
 * Stops at the first ' ', '\t', '\n' or EOF.
 * Returns <0 if the buffer is too small, or the number of chars read.
 */
int sgettok(STREAM* f, char* str, int size);

/**
 * Reads until the end of line.
 * Stops at the first '\n' or EOF. Note that '\n' is left in the stream.
 * Returns <0 if the buffer is too small, or the number of chars read.
 */
int sgetline(STREAM* f, char* str, int size);

/**
 * Like sgetline() but removes ' ' and '\t' at the end.
 */
int sgetlasttok(STREAM* f, char* str, int size);

/**
 * Reads a 32 bit number.
 * Stops at the first not digit char or EOF.
 * Returns <0 if there isn't enough to read.
 */
int sgetu32(STREAM* f, uint32_t* value);

/**
 * Reads a 64 bit number.
 * Stops at the first not digit char.
 * Returns <0 if there isn't enough to read.
 */
int sgetu64(STREAM* f, uint64_t* value);

/**
 * Reads an hexadecimal string of fixed length.
 * Returns <0 if there isn't enough to read.
 */
int sgethex(STREAM* f, void* data, int size);

/****************************************************************************/
/* binary get */

/**
 * Reads a binary 32 bit number in packet format.
 * Returns <0 if there isn't enough to read.
 */
int sgetb32(STREAM* f, uint32_t* value);

/**
 * Reads a binary 64 bit number in packet format.
 * Returns <0 if there isn't enough to read.
 */
int sgetb64(STREAM* f, uint64_t* value);

/**
 * Reads a binary 32 bit number in little endian format.
 * Returns <0 if there isn't enough to read.
 */
int sgetble32(STREAM* f, uint32_t* value);

/**
 * Reads a binary string.
 * Returns -1 on error or if the buffer is too small, or the number of chars read.
 */
int sgetbs(STREAM* f, char* str, int size);

/****************************************************************************/
/* put */

/**
 * Writes a char. Like fputc().
 * Returns 0 on success or -1 on error.
 */
static inline int sputc(int c, STREAM* s)
{
	if (s->pos == s->end) {
		if (sflush(s) != 0)
			return -1;
	}
	s->crc_stream = crc32c_plain(s->crc_stream, c);
	*s->pos++ = c;
	return 0;
}

/**
 * Writes a end of line.
 * Returns 0 on success or -1 on error.
 */
static inline int sputeol(STREAM* s)
{
#ifdef _WIN32
	if (sputc('\r', s) != 0)
		return -1;
#endif
	return sputc('\n', s);
}

/**
 * Writes a string.
 * Returns 0 on success or -1 on error.
 */
int sputs(const char* str, STREAM* f);

/**
 * Write a sized string.
 * Returns 0 on success or -1 on error.
 */
int swrite(const void* data, unsigned size, STREAM* f);

/**
 * Write a string literal (constant string).
 * Returns 0 on success or -1 on error.
 */
#define sputsl(str, f) swrite(str, sizeof(str) - 1, f)

/**
 * Writes a 32 bit number.
 * Returns 0 on success or -1 on error.
 */
int sputu32(uint32_t value, STREAM* s);

/**
 * Writes a 64 bit number.
 * Returns 0 on success or -1 on error.
 */
int sputu64(uint64_t value, STREAM* s);

/**
 * Writes a hexadecimal string of fixed length.
 * Returns 0 on success or -1 on error.
 */
int sputhex(const void* void_data, int size, STREAM* s);

/****************************************************************************/
/* binary put */

/**
 * Writes a binary 32 bit number in packed format.
 * Returns 0 on success or -1 on error.
 */
int sputb32(uint32_t value, STREAM* s);

/**
 * Writes a binary 64 bit number in packed format.
 * Returns 0 on success or -1 on error.
 */
int sputb64(uint64_t value, STREAM* s);

/**
 * Writes a binary 32 bit number in little endian format.
 * Returns 0 on success or -1 on error.
 */
int sputble32(uint32_t value, STREAM* s);

/**
 * Writes a binary string.
 * Returns 0 on success or -1 on error.
 */
int sputbs(const char* str, STREAM* s);

#endif

