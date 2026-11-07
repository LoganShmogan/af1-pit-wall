// Stand-in for the ESP32 ROM's miniz header (implemented with the PC's zlib in fakes.cpp)
#pragma once
#include <cstddef>
#define TINFL_DECOMPRESS_MEM_TO_MEM_FAILED ((size_t)(-1))
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);
