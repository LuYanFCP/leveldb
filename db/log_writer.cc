// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/log_writer.h"

#include <cstdint>

#include "leveldb/env.h"
#include "util/coding.h"
#include "util/crc32c.h"

namespace leveldb {
namespace log {

static void InitTypeCrc(uint32_t* type_crc) {
  for (int i = 0; i <= kMaxRecordType; i++) {
    char t = static_cast<char>(i);
    type_crc[i] = crc32c::Value(&t, 1);
  }
}

Writer::Writer(WritableFile* dest) : dest_(dest), block_offset_(0) {
  InitTypeCrc(type_crc_);
}

Writer::Writer(WritableFile* dest, uint64_t dest_length)
    : dest_(dest), block_offset_(dest_length % kBlockSize) {
  InitTypeCrc(type_crc_);
}

Writer::~Writer() = default;

Status Writer::AddRecord(const Slice& slice) {
  const char* ptr = slice.data();
  size_t left = slice.size();

  // Fragment the record if necessary and emit it.  Note that if slice
  // is empty, we still want to iterate once to emit a single
  // zero-length record
  Status s;
  bool begin = true;
  do {
    const int leftover = kBlockSize - block_offset_;  // 
    assert(leftover >= 0);
    if (leftover < kHeaderSize) {
      // Switch to a new block
      if (leftover > 0) {
        // Fill the trailer (literal below relies on kHeaderSize being 7)
        static_assert(kHeaderSize == 7, "");
        dest_->Append(Slice("\x00\x00\x00\x00\x00\x00", leftover));   // 补充0
      }
      block_offset_ = 0;
    }

    // Invariant: we never leave < kHeaderSize bytes in a block.
    assert(kBlockSize - block_offset_ - kHeaderSize >= 0);

    const size_t avail = kBlockSize - block_offset_ - kHeaderSize;  // 剩余的内容
    const size_t fragment_length = (left < avail) ? left : avail;

    RecordType type;
    const bool end = (left == fragment_length);  // 如果不能填充一个block
    if (begin && end) {
      type = kFullType;  // 全部crc
    } else if (begin) {
      type = kFirstType;  // 开头
    } else if (end) {  // 结尾
      type = kLastType;
    } else { // 中间
      type = kMiddleType;
    }

    s = EmitPhysicalRecord(type, ptr, fragment_length);   // 开始排空
    ptr += fragment_length;
    left -= fragment_length;
    begin = false;
  } while (s.ok() && left > 0);
  return s;
}

Status Writer::EmitPhysicalRecord(RecordType t, const char* ptr,
                                  size_t length) {
  assert(length <= 0xffff);  // Must fit in two bytes
  assert(block_offset_ + kHeaderSize + length <= kBlockSize);

  // Format the header
  char buf[kHeaderSize];
  // 0-3 是CRC checksum
  buf[4] = static_cast<char>(length & 0xff);   // 长度
  buf[5] = static_cast<char>(length >> 8);  // 
  buf[6] = static_cast<char>(t);  // 类型，开头、中间、结尾

  // Compute the crc of the record type and the payload.
  uint32_t crc = crc32c::Extend(type_crc_[t], ptr, length);
  crc = crc32c::Mask(crc);  // Adjust for storage
  EncodeFixed32(buf, crc);  // 将CRC填入

  // Write the header and the payload
  Status s = dest_->Append(Slice(buf, kHeaderSize));  // 写入头
  if (s.ok()) {
    s = dest_->Append(Slice(ptr, length));  // 写数据
    if (s.ok()) {
      s = dest_->Flush();  // Flush
    }
  }
  /**
   * @brief 
   * 1. 开头和中间的都是block_offset = KBlock
   * 2. 如果是结尾那就是真实的offset 
   */
  block_offset_ += kHeaderSize + length;  // block_offset_
  return s;
}

}  // namespace log
}  // namespace leveldb
