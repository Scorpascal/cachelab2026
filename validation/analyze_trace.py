"""Attribute direct-mapped cache misses by matrix and by the standard 3C model.

Usage: python3 validation/analyze_trace.py CASE TRACE
Uses this lab's 32 sets, one way, 16-byte lines and contiguous A/B/C layout.
This is an independent analysis utility, not part of the official evaluator.
"""
from collections import Counter, OrderedDict
from pathlib import Path
import json
import sys


def analyze(case, path):
    m, n, p = [(2, 2, 2), (16, 16, 16), (32, 32, 32), (27, 41, 27)][case]
    bounds = [m * n, m * n + n * p, m * n + n * p + m * p]
    tags = [None] * 32
    seen = set()
    fully_associative = OrderedDict()
    rows = {name: Counter() for name in ["A", "B", "C", "buffer"]}
    for text in Path(path).read_text().splitlines():
        operation, address, *_ = text.split()
        word = (int(address.split(",")[0], 16) - 0x30000000) // 4
        owner = next((name for name, end in zip(["A", "B", "C"], bounds) if word < end), "buffer")
        block = word // 4
        index = block % 32
        row = rows[owner]
        row["accesses"] += 1
        if tags[index] != block:
            row["misses"] += 1
            category = "compulsory" if block not in seen else "conflict" if block in fully_associative else "capacity"
            row[category] += 1
            tags[index] = block
        seen.add(block)
        fully_associative[block] = None
        fully_associative.move_to_end(block)
        if len(fully_associative) > 32:
            fully_associative.popitem(last=False)
    return rows


if __name__ == "__main__":
    print(json.dumps(analyze(int(sys.argv[1]), sys.argv[2]), indent=2))
