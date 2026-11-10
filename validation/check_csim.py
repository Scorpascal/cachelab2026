"""Additional deterministic tests; never modifies the official test directory.

Run from the repository root after `make csim` on Linux x86-64:
    python3 validation/check_csim.py
"""

from pathlib import Path
from collections import OrderedDict
import random
import subprocess
import tempfile


ROOT = Path.cwd()
CONFIGS = [(0, 1, 0), (0, 4, 3), (1, 2, 0), (3, 8, 5),
           (5, 1, 4), (8, 4, 6), (2, 3, 12), (4, 2, 4)]


def run(binary, work, trace, config, verbose=False):
    summary = work / ".csim_results"
    summary.unlink(missing_ok=True)
    s, ways, b = config
    args = [str(ROOT / binary), "-s", str(s), "-E", str(ways),
            "-b", str(b), "-t", str(trace)]
    if verbose:
        args.append("-v")
    result = subprocess.run(args, cwd=work, capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, (args, result.stdout, result.stderr)
    return tuple(map(int, summary.read_text().split()))


def oracle(addresses, config):
    s, ways, b = config
    sets = [OrderedDict() for _ in range(1 << s)]
    hits = misses = evictions = 0
    for address in addresses:
        block = address >> b
        entries = sets[block & ((1 << s) - 1)]
        tag = block >> s
        if tag in entries:
            hits += 1
            entries.move_to_end(tag)
        else:
            misses += 1
            if len(entries) == ways:
                entries.popitem(last=False)
                evictions += 1
            entries[tag] = None
    return hits, misses, evictions


def main():
    checked = 0
    with tempfile.TemporaryDirectory(prefix="cachelab-csim-check-") as directory:
        work = Path(directory)
        trace = work / "random.trace"
        for config in CONFIGS:
            s, ways, b = config
            for seed in range(8):
                rng = random.Random(20261110 + seed)
                # The supplied binary aliases addresses differing only above
                # bit 31. Compare it within its working address domain; use a
                # separate full-width oracle for the README's 64-bit contract.
                addresses = [0, (1 << 32) - 1, 1 << 31, 0xFFFF0000]
                # Repeated tags in the same set force hit refresh and LRU eviction.
                addresses += [i << (s + b) for i in range(ways + 3)]
                lines = []
                for index in range(256):
                    address = (rng.choice(addresses) if index % 4 else rng.getrandbits(32))
                    text = ("0x" if index % 2 else "") + format(address, "x")
                    lines.append(f" {rng.choice('LS')} {text},{rng.randint(1, 8)} {rng.randint(-1, 35)}\n")
                trace.write_text("".join(lines))
                expected = run("csim-ref", work, trace, config)
                actual = run("csim", work, trace, config)
                assert actual == expected, (config, seed, actual, expected)
                checked += 1

                addresses = [rng.getrandbits(64) for _ in range(20)]
                addresses += [0, 1 << 32, 1 << 63, (1 << 64) - 1]
                sequence = [rng.choice(addresses) for _ in range(256)]
                trace.write_text("".join(f" L 0x{a:x},1 -1\n" for a in sequence))
                actual = run("csim", work, trace, config)
                assert actual == oracle(sequence, config), (config, seed, "64-bit")

        trace.write_text(" L 0,1\n S 0,1\n M 4,1\n I 8,1\n L 0,1\n")
        assert run("csim", work, trace, (0, 1, 2), True) == (2, 3, 2)
        trace.write_text("")
        assert run("csim", work, trace, (0, 1, 0)) == (0, 0, 0)
        sequence = [0, 1 << 63, (1 << 64) - 1, 0, (1 << 63) - 1]
        trace.write_text("".join(f" L {a:x},1\n" for a in sequence))
        for config in [(0, 1, 63), (1, 1, 63)]:
            assert run("csim", work, trace, config) == oracle(sequence, config)
        for extra in [["-s", "-1"], ["-E", "0"], ["-b", "64"], ["-s", "64"],
                      ["-E", "not-a-number"], ["-t", str(work / "absent")]]:
            args = [str(ROOT / "csim"), "-s", "1", "-E", "1", "-b", "1",
                    "-t", str(trace), *extra]
            result = subprocess.run(args, cwd=work, capture_output=True, timeout=10)
            assert result.returncode != 0, args
    print(f"PASS: {checked} randomized reference comparisons, {checked} full-width oracle comparisons; modify/instruction, empty trace, and invalid-input checks.")


if __name__ == "__main__":
    main()
