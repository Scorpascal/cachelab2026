// Independent validation only. Build from the repository root:
// g++ -std=c++23 -O1 -g -fsanitize=address,undefined -I. validation/check_gemm.cpp gemm.cpp -o /tmp/check-gemm
#include "cachelab.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <vector>

using Kernel = void (*)(ptr_reg, ptr_reg, ptr_reg, ptr_reg);

int main()
{
  const std::array<Kernel, 4> kernels = {gemm_case0, gemm_case1, gemm_case2, gemm_case3};
  const int shapes[4][3] = {{2, 2, 2}, {16, 16, 16}, {32, 32, 32}, {27, 41, 27}};
  int checked = 0;
  for (int test = 0; test < 4; ++test)
  {
    const auto [m, n, p] = shapes[test];
    const int na = m * n, nb = n * p, nc = m * p;
    for (int seed = 0; seed < 32; ++seed)
    {
      std::mt19937 rng(20261110 + seed);
      std::uniform_int_distribution<int> values(-16, 16);
      // Exact allocation lets ASan detect accesses outside the provided pool.
      std::vector<int> pool(na + nb + nc + BUFFER_SIZE, 314159);
      for (int t = 0; t < na + nb; ++t)
        pool[t] = seed == 0 ? 0 : seed == 1 ? 1 : seed == 2 ? (t % 2 ? -1 : 1) : values(rng);
      const auto before = pool;
      ptr_reg::access_logs.clear();
      max_reg_count = 0;
      ptr_reg::base = pool.data();
      ptr_reg::base_offset = reinterpret_cast<int *>(0x30000000);
      {
        ptr_reg a(pool.data()), b(pool.data() + na), c(pool.data() + na + nb);
        ptr_reg buffer(pool.data() + na + nb + nc);
        kernels[test](std::move(a), std::move(b), std::move(c), std::move(buffer));
      }
      if (current_reg_count != 0 || max_reg_count > 36)
        throw std::runtime_error("Register lifetime or limit failure");
      if (!std::equal(before.begin(), before.begin() + na + nb, pool.begin()))
        throw std::runtime_error("Input matrix changed");
      for (const auto &access : ptr_reg::access_logs)
      {
        const auto offset = access.addr_ - pool.data();
        if (access.type_ == MemoryAccessType::READ)
        {
          if (offset < 0 || offset >= na + nb)
            throw std::runtime_error("Read outside the input matrices");
        }
        else if (access.type_ != MemoryAccessType::WRITE ||
                 offset < na + nb || offset >= na + nb + nc)
          throw std::runtime_error("Write outside the output matrix");
      }
      for (int i = 0; i < m; ++i)
        for (int j = 0; j < p; ++j)
        {
          int expected = 0;
          for (int k = 0; k < n; ++k)
            expected += before[i * n + k] * before[na + k * p + j];
          if (pool[na + nb + i * p + j] != expected)
            throw std::runtime_error("Matrix result mismatch");
        }
      // This implementation does not need buffer; check its initial sentinel.
      if (!std::equal(before.end() - BUFFER_SIZE, before.end(), pool.end() - BUFFER_SIZE))
        throw std::runtime_error("Unexpected buffer write");
      ++checked;
    }
    std::cout << "case" << test << ": 32 input patterns passed; peak " << max_reg_count << " registers\n";
  }
  std::cout << "PASS: " << checked << " cases; signed/random/zero inputs, nonzero initial C, unchanged A/B/buffer, register lifetimes.\n";
}
