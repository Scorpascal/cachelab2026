#include "cachelab.h"

// All computation uses the supplied register API. A and B remain unchanged.
// Only loop bounds and tile sizes are specialized; matrix values are read at runtime.
void gemm_case0(ptr_reg A, ptr_reg B, ptr_reg C, ptr_reg buffer)
{
  for (reg i = 0; i < case0_m; ++i)
    for (reg j = 0; j < case0_p; ++j)
    {
      reg sum = 0;
      for (reg k = 0; k < case0_n; ++k)
      {
        reg a = A[i * case0_n + k];
        reg b = B[k * case0_p + j];
        sum += a * b;
      }
      C[i * case0_p + j] = sum;
    }
}

void gemm_case1(ptr_reg A, ptr_reg B, ptr_reg C, ptr_reg buffer)
{
  // 6 x 4 register tile; peak 36 registers including arguments.
  for (reg i = 0; i < case1_m; i += 6)
    for (reg j = 0; j < case1_p; j += 4)
    {
      reg acc[6][4]; // Registers default to zero; no initial C load.
      // Reverse reduction keeps useful lines near the next tile boundary.
      for (reg k = case1_n - 1; k >= 0; --k)
      {
        reg b[4];
        b[0] = B[k * case1_p + j + 0];
        b[1] = B[k * case1_p + j + 1];
        b[2] = B[k * case1_p + j + 2];
        b[3] = B[k * case1_p + j + 3];
        if (i + 0 < case1_m)
        {
          reg a = A[(i + 0) * case1_n + k];
          acc[0][0] += a * b[0];
          acc[0][1] += a * b[1];
          acc[0][2] += a * b[2];
          acc[0][3] += a * b[3];
        }
        if (i + 1 < case1_m)
        {
          reg a = A[(i + 1) * case1_n + k];
          acc[1][0] += a * b[0];
          acc[1][1] += a * b[1];
          acc[1][2] += a * b[2];
          acc[1][3] += a * b[3];
        }
        if (i + 2 < case1_m)
        {
          reg a = A[(i + 2) * case1_n + k];
          acc[2][0] += a * b[0];
          acc[2][1] += a * b[1];
          acc[2][2] += a * b[2];
          acc[2][3] += a * b[3];
        }
        if (i + 3 < case1_m)
        {
          reg a = A[(i + 3) * case1_n + k];
          acc[3][0] += a * b[0];
          acc[3][1] += a * b[1];
          acc[3][2] += a * b[2];
          acc[3][3] += a * b[3];
        }
        if (i + 4 < case1_m)
        {
          reg a = A[(i + 4) * case1_n + k];
          acc[4][0] += a * b[0];
          acc[4][1] += a * b[1];
          acc[4][2] += a * b[2];
          acc[4][3] += a * b[3];
        }
        if (i + 5 < case1_m)
        {
          reg a = A[(i + 5) * case1_n + k];
          acc[5][0] += a * b[0];
          acc[5][1] += a * b[1];
          acc[5][2] += a * b[2];
          acc[5][3] += a * b[3];
        }
      }
      for (reg r = 0; r < 6 && i + r < case1_m; ++r)
        for (reg c = 0; c < 4 && j + c < case1_p; ++c)
          C[(i + r) * case1_p + j + c] = acc[r][c];
    }
}

void gemm_case2(ptr_reg A, ptr_reg B, ptr_reg C, ptr_reg buffer)
{
  // 4 x 4 register tile; peak 28 registers including arguments.
  for (reg i = 0; i < case2_m; i += 4)
    // Alternate column direction between row tiles.
    for (reg j = (i / 4 % 2 ? case2_p - 4 : 0);
         j >= 0 && j < case2_p; j += (i / 4 % 2 ? -4 : 4))
    {
      reg acc[4][4]; // Registers default to zero; no initial C load.
      for (reg k = 0; k < case2_n; ++k)
      {
        // On a cache-line boundary, fetch B before A to avoid a reload.
        if (k % 4 == 0)
        {
          reg b[4];
          b[0] = B[k * case2_p + j + 0];
          b[1] = B[k * case2_p + j + 1];
          b[2] = B[k * case2_p + j + 2];
          b[3] = B[k * case2_p + j + 3];
          {
            reg a = A[(i + 0) * case2_n + k];
            acc[0][0] += a * b[0];
            acc[0][1] += a * b[1];
            acc[0][2] += a * b[2];
            acc[0][3] += a * b[3];
          }
          {
            reg a = A[(i + 1) * case2_n + k];
            acc[1][0] += a * b[0];
            acc[1][1] += a * b[1];
            acc[1][2] += a * b[2];
            acc[1][3] += a * b[3];
          }
          {
            reg a = A[(i + 2) * case2_n + k];
            acc[2][0] += a * b[0];
            acc[2][1] += a * b[1];
            acc[2][2] += a * b[2];
            acc[2][3] += a * b[3];
          }
          {
            reg a = A[(i + 3) * case2_n + k];
            acc[3][0] += a * b[0];
            acc[3][1] += a * b[1];
            acc[3][2] += a * b[2];
            acc[3][3] += a * b[3];
          }
        }
        else
        {
          reg a[4];
          a[0] = A[(i + 0) * case2_n + k];
          a[1] = A[(i + 1) * case2_n + k];
          a[2] = A[(i + 2) * case2_n + k];
          a[3] = A[(i + 3) * case2_n + k];
          {
            reg b = B[k * case2_p + j + 0];
            acc[0][0] += a[0] * b;
            acc[1][0] += a[1] * b;
            acc[2][0] += a[2] * b;
            acc[3][0] += a[3] * b;
          }
          {
            reg b = B[k * case2_p + j + 1];
            acc[0][1] += a[0] * b;
            acc[1][1] += a[1] * b;
            acc[2][1] += a[2] * b;
            acc[3][1] += a[3] * b;
          }
          {
            reg b = B[k * case2_p + j + 2];
            acc[0][2] += a[0] * b;
            acc[1][2] += a[1] * b;
            acc[2][2] += a[2] * b;
            acc[3][2] += a[3] * b;
          }
          {
            reg b = B[k * case2_p + j + 3];
            acc[0][3] += a[0] * b;
            acc[1][3] += a[1] * b;
            acc[2][3] += a[2] * b;
            acc[3][3] += a[3] * b;
          }
        }
      }
      for (reg r = 0; r < 4 && i + r < case2_m; ++r)
        for (reg c = 0; c < 4 && j + c < case2_p; ++c)
          C[(i + r) * case2_p + j + c] = acc[r][c];
    }
}

void gemm_case3(ptr_reg A, ptr_reg B, ptr_reg C, ptr_reg buffer)
{
  // 4 x 6 register tile; peak 36 registers including arguments.
  for (reg i = 0; i < case3_m; i += 4)
    // Snake through column tiles, then reverse k on alternating tiles.
    // Both ends remain valid for the partial 3-row / 3-column edge tiles.
    for (reg j = (i / 4 % 2 ? 24 : 0);
         j >= 0 && j < case3_p; j += (i / 4 % 2 ? -6 : 6))
    {
      reg acc[4][6]; // Registers default to zero; no initial C load.
      for (reg k = ((i / 4 + j / 6) % 2 ? case3_n - 1 : 0);
           k >= 0 && k < case3_n;
           k += ((i / 4 + j / 6) % 2 ? -1 : 1))
      {
        reg a[4];
        if (i + 0 < case3_m)
          a[0] = A[(i + 0) * case3_n + k];
        if (i + 1 < case3_m)
          a[1] = A[(i + 1) * case3_n + k];
        if (i + 2 < case3_m)
          a[2] = A[(i + 2) * case3_n + k];
        if (i + 3 < case3_m)
          a[3] = A[(i + 3) * case3_n + k];
        if (j + 0 < case3_p)
        {
          reg b = B[k * case3_p + j + 0];
          acc[0][0] += a[0] * b;
          acc[1][0] += a[1] * b;
          acc[2][0] += a[2] * b;
          acc[3][0] += a[3] * b;
        }
        if (j + 1 < case3_p)
        {
          reg b = B[k * case3_p + j + 1];
          acc[0][1] += a[0] * b;
          acc[1][1] += a[1] * b;
          acc[2][1] += a[2] * b;
          acc[3][1] += a[3] * b;
        }
        if (j + 2 < case3_p)
        {
          reg b = B[k * case3_p + j + 2];
          acc[0][2] += a[0] * b;
          acc[1][2] += a[1] * b;
          acc[2][2] += a[2] * b;
          acc[3][2] += a[3] * b;
        }
        if (j + 3 < case3_p)
        {
          reg b = B[k * case3_p + j + 3];
          acc[0][3] += a[0] * b;
          acc[1][3] += a[1] * b;
          acc[2][3] += a[2] * b;
          acc[3][3] += a[3] * b;
        }
        if (j + 4 < case3_p)
        {
          reg b = B[k * case3_p + j + 4];
          acc[0][4] += a[0] * b;
          acc[1][4] += a[1] * b;
          acc[2][4] += a[2] * b;
          acc[3][4] += a[3] * b;
        }
        if (j + 5 < case3_p)
        {
          reg b = B[k * case3_p + j + 5];
          acc[0][5] += a[0] * b;
          acc[1][5] += a[1] * b;
          acc[2][5] += a[2] * b;
          acc[3][5] += a[3] * b;
        }
      }
      for (reg r = 0; r < 4 && i + r < case3_m; ++r)
        for (reg c = 0; c < 6 && j + c < case3_p; ++c)
          C[(i + r) * case3_p + j + c] = acc[r][c];
    }
}
