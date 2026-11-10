# Independent validation

These files are additional checks and do not change the official evaluator.
Run from the repository root. The official commands remain:

```sh
python3 test/overall_test.py
```

Additional Linux x86-64 simulator comparisons, including a separate 64-bit oracle:

```sh
make csim
python3 validation/check_csim.py
```

The supplied reference binary aliases some high 32-bit address differences.
`check_csim.py` therefore compares low 32-bit traces to that binary and full-width
traces to an independent LRU model, as documented in the report.

Random/edge matrices with ASan and UBSan, using the original register API:

```sh
g++ -std=c++23 -Wall -O1 -g -fsanitize=address,undefined -I. validation/check_gemm.cpp gemm.cpp -o /tmp/check-gemm
/tmp/check-gemm
```

Per-matrix access/miss attribution and 3C classification by an independent cache model:

```sh
make printTrace
./printTrace case3 > /tmp/case3.trace
python3 validation/analyze_trace.py 3 /tmp/case3.trace
```

`report/evidence/` contains the actual QEMU final results and source hashes.
The report PDF uses embedded Songti, Heiti and Menlo fonts. To rebuild on macOS
with Pandoc and XeLaTeX installed, run `python3 report/build_report.py`.
Personal submission files and VM artifacts belong in ignored `workspaces/`.
The course submission consists of exactly `csim.c`, `gemm.cpp`, and the report PDF;
these validation utilities are not additional course submission files.
