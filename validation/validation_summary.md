# Validation Summary

**Project:** OEIS A096242  
**Scope:** raw GitHub-ready package for the computation through `n=14`  
**Prepared from workspace:** `/home/carlo/MATH/OEIS/A096242`  

## Main result

The extended b-file is:

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

## Core commands

Build and known-term replay:

```sh
make -C src clean release
make -C src test
make -C src test-segmented
make -C src test-auto
```

Production command used for the final range is represented by:

```sh
./build/a096242 --max-n 14 --threads 16 --mode auto \
  --bfile data/b096242.txt --tmp-dir tmp --save-levels levels
```

Paths of the form `levels/DNN.bin` in this package's reports refer to saved
binary level files generated during the local computational campaign.  The
binary level files are not included in the minimal GitHub package; their
counts, byte sizes, and SHA-256 hashes are documented in
`levels_manifest.tsv`.

Saved-level certification command:

```sh
/usr/bin/time -v ./cert/a096242_level_certifier --levels levels --max-n 14 --threads 16
```

Recorded result:

```text
CERTIFIER RESULT: PASS through n=14
Elapsed wall time: 1:34.64
Maximum resident set size: 4520004 KB
Exit status: 0
```

Additional D11 cross-mode validation:

```text
Single-thread memory rerun: a(11)=5056361, elapsed 0:19.48, peak RSS 1718048 kB, exit 0
16-thread segmented rerun: a(11)=5056361, elapsed 0:10.56, peak RSS 102848 kB, exit 0
Shared SHA-256: 38ffc99bf4836f551e856eced2079d2c841dd1d8e721e600bf4e119af0dd3a59
```

Additional D11 GMP re-enumeration:

```text
Source: cert/a096242_d11_gmp_reenumerate.c
GMP primality call: mpz_probab_prime_p(..., 25)
accepted=5056361 expected=5056361 OK
elapsed 0:18.07, peak RSS 1625204 kB, exit 0
Output SHA-256 matched levels/D11.bin:
38ffc99bf4836f551e856eced2079d2c841dd1d8e721e600bf4e119af0dd3a59
```

Reference mirror:

```text
references/sinclair_miller_rabin_sprp_bases.md
```

This is a local Markdown mirror of the Sinclair Miller-Rabin SPRP bases table
cited by the manuscript. It is included to reduce dependence on the live web
URL; it is not a DOI or a peer-reviewed publication.

## Included checksums

```text
b873a75076e8a011c754db35aefe84f0bb1d42da2f7a6e81b938ca3c4b57a4e2  README.md
7bf69769216464fafa3ad4e52714ce11b504b56800879ae163dc06990340c880  src/a096242.c
d7f2eb30399fea931ffb0d58e61ed1a2d0d031cc565b02a44d9c0e9741545dd4  src/Makefile
2ccfb6ca98937e899a1422ee8153d681239873a66020cc6d12fe0275fb5b5bab  data/b096242.txt
ffe9ed588b200970277661d6b6ba4e14204f5273aa1b2623666e26e6c2fea706  levels_manifest.tsv
5c055d84cf4cd19b7587cd2c76c2742bdefd457d209422f891df0d4498fe474b  cert/README.md
754f4b2399852da5e2eeadf336f88a50000b960dc2535911de97c5b694fd07a1  cert/a096242_level_certifier.c
88c23e510964eba427d2d61e2105e36fb6255803f1406501ed4d5c08af202b41  cert/a096242_d11_gmp_reenumerate.c
87abde54fdbd5b4655d3a9a1046d2a24aa325ab2e97417fe10143f251e1f1069  validation/A096242_n13_run_evaluation.md
a59c411bc46d5ebda87cdd801b48071e7912f971d942144f79a5e6adb5536af8  validation/A096242_n14_run_evaluation.md
c6dfd6ac615aa1714558fe034fff2f33ef6da002ef5778b6c443c0db0cb3c38a  validation/A096242_level_certifier_n14.md
574156c57feadf95c3e08d484857edf0356a2bf6315f548f5676e9b06d270bca  validation/A096242_d11_cross_mode_validation.md
009842bb39e380935527aeb6a8fe522791751771aed9b6b519967c63b3421b96  validation/A096242_d11_gmp_reenumeration.md
dcae834f8a50d72abbcc5700e898d0916329aaf5b6b26605bed95e39db6170ab  validation/validation_notes.md
f67c790c099c8bc611c1f2e7774d12aefadc94383d986e3cb8655a74b538f73c  references/sinclair_miller_rabin_sprp_bases.md
fff780c6c50e7218a8ea0431e7214008d6c55de6c4699f16bef637ba917c8f60  paper/A096242_v10.tex
c1b2c5b27938e28fc082cf7ea33fa9d807efed6ee0954ddc168d658ca8911960  paper/A096242_v10.pdf
870fb5c2c6c95319a353a29db9e6e3e2a98107114efd7347869e3ddcac7066ee  paper/paper_notes.md
```

## Large generated artifacts

The files `levels/D01.bin` through `levels/D14.bin` are not included in this
raw package. Their counts, sizes, and SHA-256 hashes are recorded in
`levels_manifest.tsv`.
