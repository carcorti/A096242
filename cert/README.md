# A096242 Level Certifier

This directory contains validation programs for A096242.

## Scope

`a096242_level_certifier.c` verifies the saved binary levels `D01.bin`,
`D02.bin`, ..., `DNN.bin`.

For each level it checks:

- file content is strictly sorted;
- values are in the correct base-9 digit-length range;
- every saved value is prime;
- every saved value in `D_n`, for `n >= 2`, has at least one valid base-9
  digit deletion landing in `D_(n-1)`;
- `D01` is exactly `{2, 3, 5, 7}`.

This is an independent validity certifier for the saved level files. It does
not perform a second exhaustive missing-candidate enumeration.

The `DNN.bin` files are generated artifacts. They are not intended to be
uploaded to the minimal GitHub repository, especially for large levels. A
regenerated set of level files should instead be compared with
`levels_manifest.tsv`, which records counts, byte sizes, and SHA-256 hashes.

## Build

From the project root:

```sh
cc -std=c17 -O3 -march=native -Wall -Wextra -Wpedantic -Wshadow \
  -Wconversion -Wformat=2 -pthread \
  cert/a096242_level_certifier.c -o cert/a096242_level_certifier
```

## Run

Full check through `n=14`:

```sh
/usr/bin/time -v ./cert/a096242_level_certifier --levels levels --max-n 14 --threads 16
```

Short smoke test:

```sh
./cert/a096242_level_certifier --levels levels --max-n 10 --threads 16
```

## Local calibration

On the current project data:

```text
--max-n 12: PASS, 4.35 s wall, peak RSS about 218 MB
--max-n 13: PASS, 19.99 s wall, peak RSS about 986 MB
--max-n 14: PASS, 1:34.64 wall, peak RSS about 4.52 GB
```

The full `--max-n 14` report is recorded in
`outputs/A096242_level_certifier_n14.md`.

## D11 GMP Re-Enumerator

`a096242_d11_gmp_reenumerate.c` is a separate-source re-enumerator for the
first new level `D11`. It reads `levels/D10.bin`, enumerates the raw `D11`
insertion attempts, applies the leading-zero and base-9 last-digit filters,
globally deduplicates candidates, applies small-prime trial division through
97, and then calls GMP's `mpz_probab_prime_p` with repetition parameter 25.

Build:

```sh
cc -O3 -march=native -std=c17 -Wall -Wextra -Wpedantic -Wshadow \
  -Wconversion -Wformat=2 cert/a096242_d11_gmp_reenumerate.c \
  -lgmp -o /tmp/a096242_d11_gmp_reenumerate
```

Run:

```sh
/tmp/a096242_d11_gmp_reenumerate levels/D10.bin /tmp/D11_gmp.bin
```

The corresponding validation report is
`validation/A096242_d11_gmp_reenumeration.md`.
