// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "FilterStreamBase.h"
#include <algorithm>
#include <memory.h>
#include <zlib/zlib.h>

using namespace leap;

InputFilterStreamBase::InputFilterStreamBase(std::unique_ptr<IInputStream>&& is) :
  is(std::move(is)),
  inputChunk(1024, 0),
  buffer(1024, 0)
{}

std::streamsize InputFilterStreamBase::Read(void* pBuf, std::streamsize ncb) {
  if (fail)
    return -1;

  std::streamsize total = 0;
  while (ncb)
    // If there are any data bytes available in holding, prefer to return those:
    if (ncbAvail) {
      // Decide on how many bytes to move, then move those over
      size_t ncbCopy = std::min(ncbAvail, static_cast<size_t>(ncb));
      memcpy(pBuf, &*(buffer.end() - ncbAvail), ncbCopy);
      total += ncbCopy;

      // Reduce by the number of bytes we moved
      reinterpret_cast<uint8_t*&>(pBuf) += ncbCopy;
      ncbAvail -= ncbCopy;
      ncb -= ncbCopy;
    } else {
      // Pump in from the underlying stream
      auto nRead = is->Read(inputChunk.data() + inChunkRemain, inputChunk.size() - inChunkRemain);
      if (nRead < 0)
        // Treat an error condition as "zero bytes read".  It's possible that we have everything we
        // need because we still have buffer from the prior read operation.
        nRead = 0;

      // Increment by the number of bytes unprocessed in the last filter operation
      nRead += inChunkRemain;
      if (nRead == 0) {
        eof = true;
        return total;
      }

      // Handoff to transform behavior:
      size_t ncbIn = static_cast<size_t>(nRead);
      buffer.resize(1024);
      ncbAvail = buffer.size();
      if(!Transform(inputChunk.data(), ncbIn, buffer.data(), ncbAvail))
        return -1;

      // Shift over what we didn't consume under decompression
      buffer.resize(ncbAvail);
      inChunkRemain = static_cast<size_t>(nRead) - ncbIn;
      memmove(inputChunk.data(), inputChunk.data() + nRead - inChunkRemain, inChunkRemain);
    }

  // EOF if we hit the end prematurely
  eof = ncb != 0;
  return total;
}

std::streamsize InputFilterStreamBase::Skip(std::streamsize ncb) {
  return 0;
}

OutputFilterStreamBase::OutputFilterStreamBase(std::unique_ptr<IOutputStream>&& os) :
  os(std::move(os)),
  buffer(1024, 0)
{
}

OutputFilterStreamBase::~OutputFilterStreamBase(void) {}

bool OutputFilterStreamBase::Write(const void* pBuf, std::streamsize ncb, bool flush) {
  if (fail)
    throw std::runtime_error("Cannot write if compression stream is in a failed state");

  // Trivial return check:
  if (!ncb && !flush)
    return true;

  do {
    size_t ncbOut = buffer.size();
    size_t ncbIn = static_cast<size_t>(ncb);
    fail = !Transform(pBuf, ncbIn, buffer.data(), ncbOut, flush);
    if(fail)
      return false;

    ncb -= ncbIn;
    reinterpret_cast<const uint8_t*&>(pBuf) += ncbIn;
    fail = !os->Write(buffer.data(), static_cast<std::streamsize>(ncbOut));
    if (fail)
      return false;
  } while(ncb);
  return true;
}

bool OutputFilterStreamBase::Write(const void* pBuf, std::streamsize ncb) {
  return Write(pBuf, ncb, false);
}

void OutputFilterStreamBase::Flush(void) {
  Write(nullptr, 0, true);
}
