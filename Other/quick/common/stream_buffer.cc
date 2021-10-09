#include "stream_buffer.h"
#include <assert.h>
#include <string.h>

StreamBuffer::StreamBuffer()
{
	_buf = new uint8_t[_capacity];
	assert(_buf);
	memset(_buf, 0, _capacity);
}

StreamBuffer::~StreamBuffer()
{
	delete[] _buf;
}

void StreamBuffer::Append(const uint8_t* data, uint32_t data_len)
{
	assert(CanAppend(data_len));
	memcpy(_buf + _data_size, data, data_len);
	_data_size += data_len;
}

void StreamBuffer::Remove(uint32_t data_len)
{
	assert(data_len <= _data_size);
	uint32_t need_move_bytes = _data_size - data_len;
	memmove(_buf, _buf + data_len, need_move_bytes);
	_data_size -= data_len;
}

bool StreamBuffer::CanAppend(uint32_t data_len)
{
	return (_capacity - _data_size) >= data_len;
}
