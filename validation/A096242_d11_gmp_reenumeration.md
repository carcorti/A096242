# A096242 D11 GMP Re-Enumeration

**Date:** June 7, 2026  
**Scope:** separate-source re-enumeration of `D11` from `D10` using GMP primality testing.

## Purpose

Paths such as `levels/D10.bin` and `levels/D11.bin` refer to saved binary
level files generated during the local computational campaign.  These binary
files are not included in the minimal GitHub package; their counts, byte
sizes, and SHA-256 hashes are documented in `levels_manifest.tsv`.

This validation program is intentionally separate from the production source
`src/a096242.c`.  It reads `levels/D10.bin`, enumerates the raw `D11`
insertion attempts, applies the leading-zero and base-9 last-digit filters,
globally deduplicates the surviving candidates, applies small-prime trial
division through 97, and calls GMP's `mpz_probab_prime_p` with repetition
parameter 25.

The check is an independent probabilistic primality cross-check for the first
new level.  It is not a deterministic primality certificate and it does not
cover `D12`, `D13`, or `D14`.

## Source

The source file is:

```text
cert/a096242_d11_gmp_reenumerate.c
```

Build command:

```sh
cc -O3 -march=native -std=c17 -Wall -Wextra -Wpedantic -Wshadow \
  -Wconversion -Wformat=2 cert/a096242_d11_gmp_reenumerate.c \
  -lgmp -o /tmp/a096242_d11_gmp_reenumerate
```

Run command:

```sh
/usr/bin/time -v /tmp/a096242_d11_gmp_reenumerate \
  levels/D10.bin \
  tmp/d11_gmp_20260607_095433/D11_gmp.bin
```

## Run Output

```text
parents=1140171
insertion_trials=112876929
leading_zero_rejects=1140171
last_digit_rejects=3420513
syntax_kept=108316245
global_unique=87478862
small_prime_rejects=73190304
gmp_calls=14288558
accepted=5056361 expected=5056361 OK
timing_read=0.004s
timing_generate=0.346s
timing_sort_unique=6.734s
timing_gmp_primality=10.915s
timing_write=0.069s
timing_total=18.068s
```

`/usr/bin/time -v` reported:

```text
Elapsed wall time: 0:18.07
Maximum resident set size: 1625204 kB
Exit status: 0
```

## Byte-Identity Check

The production `D11.bin` file and the GMP re-enumeration file had the same
SHA-256 hash:

```text
38ffc99bf4836f551e856eced2079d2c841dd1d8e721e600bf4e119af0dd3a59  levels/D11.bin
38ffc99bf4836f551e856eced2079d2c841dd1d8e721e600bf4e119af0dd3a59  tmp/d11_gmp_20260607_095433/D11_gmp.bin
```

Byte count comparison:

```text
40450888 levels/D11.bin
40450888 tmp/d11_gmp_20260607_095433/D11_gmp.bin
```

The binary `D11_gmp.bin` file is not included in the minimal GitHub package.
It is a generated validation artifact.

## Interpretation

This report supports the manuscript statement that `D11` was reproduced by a
separate GMP-based re-enumerator and that the resulting level file was
byte-identical to the saved production `D11.bin`.  The input `D10.bin` is a
saved level file whose count agrees with the local OEIS snapshot through the
known terms.
