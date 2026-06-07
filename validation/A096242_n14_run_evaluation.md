# A096242 n=14 Run Evaluation

**Date:** 2026-06-05  
**Evaluator:** OpenAI Codex  
**Operational artifacts:** `src/a096242.c`, `src/Makefile`, `build/a096242`  
**Saved level:** `levels/D14.bin`

## 1. Executive summary

The completed run through `n=14` produced a structurally valid `D14.bin` file.

New term:

```text
a(14) = 475048816
```

Operational verdict: **RUN ACCEPTED** as a valid production computation for `n=14`, subject to later independent recomputation or external peer-review confirmation before OEIS-facing publication.

## 2. Count and file metadata

```text
levels/D14.bin bytes: 3800390528
count = bytes / 8 = 475048816
SHA-256: 13d23872161dcb2730fdb4b20cff363e205889298b29420d56c0abae8ba91aa4
first value: 2541866082389
last value: 22876787289959
```

The first and last values are in the valid 14-digit base-9 range.

## 3. Structural validation

A streaming validator checked the entire `D14.bin` file for:

- file size divisible by 8;
- exact read count matching file-size count;
- strict increasing order;
- values in the correct 14-digit base-9 range.

Result:

```text
D14: bytes=3800390528 count=475048816 read_count=475048816 first=2541866082389 last=22876787289959 sorted=True range_ok=True
```

## 4. Independent sample validation

A deterministic Python checker sampled 102 values from `D14.bin`, including first and last values. For every sampled value it verified:

- deterministic Miller-Rabin primality;
- at least one valid base-9 digit deletion lands in `D13.bin`.

Result:

```text
D14: sampled=102 prime_ok=True deletion_ok=True
```

## 5. Candidate b-file extension

```text
1 4
2 14
3 58
4 221
5 911
6 3638
7 14687
8 61435
9 262189
10 1140171
11 5056361
12 22726556
13 103351642
14 475048816
```

## 6. n=15 scale estimate from the measured a(14)

Using `a(14) = 475048816`:

```text
candidate_bound n=15 = a(14) * 15 * 9 = 64131590160
raw candidate bytes = 513.1 GB decimal = 477.8 GiB
```

Estimated output size, assuming the next ratio remains around 4.55--4.70:

```text
a(15) estimate: 2.16e9 to 2.23e9
D15.bin estimate: 17.3 GB to 17.9 GB decimal
```

`n=15` is feasible only as a segmented I/O-heavy campaign. It should not be run in memory mode.
