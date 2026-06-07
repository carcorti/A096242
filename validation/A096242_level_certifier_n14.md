# A096242 Saved-Level Certifier Report through n=14

**Date:** 2026-06-06  
**Command:**

```sh
/usr/bin/time -v ./cert/a096242_level_certifier --levels levels --max-n 14 --threads 16
```

## 1. Executive summary

The independent saved-level certifier passed through `n=14`.

```text
CERTIFIER RESULT: PASS through n=14
```

This certifier verifies the saved binary level files, not a second exhaustive missing-candidate enumeration. For every saved value it checks primality and the existence of at least one valid base-9 digit deletion into the previous level.

## 2. Per-level results

```text
D01: count=4 base_case OK
D02: count=14 prime_bad=0 deletion_bad=0 elapsed=0.000s OK
D03: count=58 prime_bad=0 deletion_bad=0 elapsed=0.000s OK
D04: count=221 prime_bad=0 deletion_bad=0 elapsed=0.000s OK
D05: count=911 prime_bad=0 deletion_bad=0 elapsed=0.000s OK
D06: count=3638 prime_bad=0 deletion_bad=0 elapsed=0.001s OK
D07: count=14687 prime_bad=0 deletion_bad=0 elapsed=0.002s OK
D08: count=61435 prime_bad=0 deletion_bad=0 elapsed=0.006s OK
D09: count=262189 prime_bad=0 deletion_bad=0 elapsed=0.030s OK
D10: count=1140171 prime_bad=0 deletion_bad=0 elapsed=0.136s OK
D11: count=5056361 prime_bad=0 deletion_bad=0 elapsed=0.692s OK
D12: count=22726556 prime_bad=0 deletion_bad=0 elapsed=3.093s OK
D13: count=103351642 prime_bad=0 deletion_bad=0 elapsed=14.563s OK
D14: count=475048816 prime_bad=0 deletion_bad=0 elapsed=72.633s OK
```

## 3. Resource usage

```text
User time: 1331.62 s
System time: 3.35 s
CPU utilization: 1410%
Elapsed wall time: 1:34.64
Maximum resident set size: 4520004 KB
Major page faults: 0
File system inputs: 9494864
File system outputs: 0
Exit status: 0
```

## 4. Paper-use boundary

This report supports the statement that the saved levels `D01.bin` through `D14.bin` passed an independent saved-level validity check:

- base case verified for `D01`;
- sorted/range checks are performed by the certifier before per-level certification;
- every saved value in `D02`--`D14` is prime;
- every saved value in `D02`--`D14` has at least one valid digit deletion into the previous saved level.

It does not support the stronger statement that a second implementation independently enumerated all possible candidates and proved that no candidate is missing.
