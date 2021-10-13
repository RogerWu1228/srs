#pragma once
#include <memory>

class StreamBuffer
{
public:
	StreamBuffer();
	~StreamBuffer();
	void Append(const uint8_t* data, uint32_t data_len);
	void Remove(uint32_t data_len);
	bool CanAppend(uint32_t data_len);
	uint32_t data_size() const { return _data_size; }
	uint8_t* data() { return _buf; }

private:
	uint8_t *_buf = nullptr;
	const uint32_t _capacity = 1024 * 1024 * 50;
	uint32_t _data_size = 0;
};

