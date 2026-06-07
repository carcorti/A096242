# A096242 n=13 Run Evaluation

**Date:** 2026-06-05  
**Evaluator:** OpenAI Codex  
**Operational artifacts:** `src/a096242.c`, `src/Makefile`, `build/a096242`  
**Saved levels:** `levels/D01.bin` through `levels/D13.bin`

## 1. Executive summary

The completed run through `n=13` produced structurally valid level files and extends the known A096242 table from `n=10` to `n=13`.

New terms:

```text
a(11) = 5056361
a(12) = 22726556
a(13) = 103351642
```

Operational verdict: **RUN ACCEPTED** as a valid first production computation, subject to any later independent full recomputation or peer-review confirmation Carlo wants before OEIS-facing publication.

## 2. Counts from binary level files

Counts were derived from file sizes, with each level file interpreted as packed little-endian `uint64_t` values.

| n | file | bytes | count |
|---:|---|---:|---:|
| 1 | `levels/D01.bin` | 32 | 4 |
| 2 | `levels/D02.bin` | 112 | 14 |
| 3 | `levels/D03.bin` | 464 | 58 |
| 4 | `levels/D04.bin` | 1768 | 221 |
| 5 | `levels/D05.bin` | 7288 | 911 |
| 6 | `levels/D06.bin` | 29104 | 3638 |
| 7 | `levels/D07.bin` | 117496 | 14687 |
| 8 | `levels/D08.bin` | 491480 | 61435 |
| 9 | `levels/D09.bin` | 2097512 | 262189 |
| 10 | `levels/D10.bin` | 9121368 | 1140171 |
| 11 | `levels/D11.bin` | 40450888 | 5056361 |
| 12 | `levels/D12.bin` | 181812448 | 22726556 |
| 13 | `levels/D13.bin` | 826813136 | 103351642 |

The first ten counts match `data/b096242.txt`.

## 3. Structural validation

A streaming validator checked every saved level file for:

- file size divisible by 8;
- exact read count matching file-size count;
- strict increasing order;
- no adjacent duplicates;
- values in the correct base-9 digit-length range.

Results:

```text
D01: count=4 first=2 last=7 sorted=True range_ok=True known_OK
D02: count=14 first=11 last=79 sorted=True range_ok=True known_OK
D03: count=58 first=83 last=727 sorted=True range_ok=True known_OK
D04: count=221 first=751 last=6551 sorted=True range_ok=True known_OK
D05: count=911 first=6761 last=58979 sorted=True range_ok=True known_OK
D06: count=3638 first=59557 last=531359 sorted=True range_ok=True known_OK
D07: count=14687 first=532919 last=4782607 sorted=True range_ok=True known_OK
D08: count=61435 first=4787527 last=43043701 sorted=True range_ok=True known_OK
D09: count=262189 first=43062839 last=387415957 sorted=True range_ok=True known_OK
D10: count=1140171 first=387461821 last=3486758053 sorted=True range_ok=True known_OK
D11: count=5056361 first=3486929471 last=31381019437 sorted=True range_ok=True
D12: count=22726556 first=31381313669 last=282429378211 sorted=True range_ok=True
D13: count=103351642 first=282429790541 last=2541865254407 sorted=True range_ok=True
```

## 4. Independent sample validation

A deterministic Python checker sampled 102 values per new level (`D11`, `D12`, `D13`), including first and last values. For every sampled value it verified:

- deterministic Miller-Rabin primality;
- at least one valid base-9 digit deletion lands in the previous saved level.

Results:

```text
D11: sampled=102 prime_ok=True deletion_ok=True
D12: sampled=102 prime_ok=True deletion_ok=True
D13: sampled=102 prime_ok=True deletion_ok=True
```

This is not a full independent recomputation, but it is a useful post-run integrity check independent of the C program's saved output logic.

## 5. SHA-256 hashes

```text
0e37d337015d595e2cb60f8d4519a6d98b6b6fabb1dc3d329966c2297bbc70c7  levels/D01.bin
7740127f65742c12cea4ba09ed31b0efbfe50d51c1563af3191f944398afdcf4  levels/D02.bin
eb7bedc61d0f1b81a47b84c25eb0529c846f56d019c182c53aacd0caaf999227  levels/D03.bin
6053491a1c9f028512521f6a5c03366360a014cbd855e5e1e217f0f91ea9c47c  levels/D04.bin
d462db9ce0e971a66d8797037e12c6edac3b573ba0d7a1b67d05a36f005d1b74  levels/D05.bin
e9d2f977b3769e4ca041db699cc776f899473c683e8e30c1cade875bf3af815f  levels/D06.bin
68394f0ff11c45f28e691330df1304ae86765c6d96665f09f7947b358195f27a  levels/D07.bin
06f82381d35b6cd41d562b5e8729577237728cadf69efc28239908fb6f567c5a  levels/D08.bin
60b21f70dbe5d55c7d91fdfe0e5a160328dc836d37c60b50e6449454db9a191c  levels/D09.bin
cc0bb10dfcd61a01b4193abfd3153c032a876f5a1c32e4c5188d63448b29d63e  levels/D10.bin
38ffc99bf4836f551e856eced2079d2c841dd1d8e721e600bf4e119af0dd3a59  levels/D11.bin
42c916e29431c05cb431d9ca4750b117342cfcc149fc8ce5d490c6c207c40a7c  levels/D12.bin
69ff460d9198fbe42be26943e530262aae6385874eee8007807a4e4e95126595  levels/D13.bin
```

## 6. Candidate b-file extension

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
```

## 7. Residual risk

The run result has passed post-run structural validation and sample mathematical validation. The remaining publication-grade confidence step would be an independent recomputation, ideally with a different implementation or at least a forced segmented/memory-mode comparison for `n=11..13` where feasible.
