# OEIS A096242 — Base-9 Deletable Primes

Repository supporting the paper **“Computational Extension of OEIS Sequence A096242: Four New Terms for Base-9 Deletable Primes”** by Carlo Corti.

OEIS sequence: A096242 — Number of n-digit base-9 deletable primes.

## Main Results

Starting from the first ten values recorded in the OEIS entry, this project extends the sequence through n = 14:

| n | a(n) |
|---|---:|
| 11 | 5,056,361 |
| 12 | 22,726,556 |
| 13 | 103,351,642 |
| 14 | 475,048,816 |

The computation is based on a deterministic bottom-up enumeration of base-9 deletable primes using insertion generation, global deduplication, trial division, and deterministic Miller–Rabin testing for uint64_t values.

## Repository Structure

```text
cert/          Independent validation tools
data/          Extended OEIS b-file
paper/         Paper source and PDF
references/    External technical references
src/           Production C17 implementation
validation/    Selected validation reports
```

Important files:

- `data/b096242.txt`
- `src/a096242.c`
- `paper/A096242.tex`
- `paper/A096242.pdf`
- `levels_manifest.tsv`
- `validation/validation_summary.md`
- `cert/a096242_level_certifier.c`
- `cert/a096242_d11_gmp_reenumerate.c`

## Build

```bash
make -C src release
```

## Validation

The repository includes representative validation artifacts:

- replay of known OEIS values;
- cross-mode validation of D11;
- independent GMP-based D11 re-enumeration;
- saved-level certification through n = 14;
- SHA-256 manifest of generated levels.

The repository contains selected validation reports and supporting documentation. Large generated level files are intentionally excluded and can be regenerated from the published source code.

## Reproducibility

The implementation supports:

```text
--threads N
--mode auto|memory|segmented
--save-levels DIR
--validate
```

Generated level files may be compared against `levels_manifest.tsv`.

## Paper

Current repository files:

- `paper/A096242.tex`
- `paper/A096242.pdf`

The manuscript clarifies that
`levels/*.bin` paths in the paper and validation reports refer to generated
local campaign artifacts, not files included in the public repository.

## Hardware and Software

The paper reports execution on a MinisForum UM790 Pro with AMD Ryzen 9 7940HS, 64 GB RAM, using a C17 implementation with POSIX threads.

## License

See `LICENSE`.

## Citation

Citation metadata are provided in `CITATION.cff`.

## Zenodo

Archived release: https://doi.org/10.5281/zenodo.20581301
