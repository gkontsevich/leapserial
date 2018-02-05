// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include "Benchmark.h"
#include <chrono>
#include <iosfwd>
#include <vector>
#include <cstddef>

class Encryption :
  public IBenchmark
{
public:
  Encryption(void);

private:
  const size_t ncbRead = 1024 * 1024 * 100;
  std::vector<uint8_t> buffer;

  std::chrono::nanoseconds SimpleRead(void);
  std::chrono::nanoseconds DirectEncryptECB(void);
  std::chrono::nanoseconds DirectEncryptCFB(void);
  std::chrono::nanoseconds EncryptedRead(void);

public:
  int Benchmark(std::ostream& os) override;
};
