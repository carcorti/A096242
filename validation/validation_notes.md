# Validation Notes

This public GitHub/Zenodo package intentionally archives reproducibility
material rather than raw terminal logs or large generated binary levels.

Repository: <https://github.com/carcorti/A096242>  
Archive DOI: <https://doi.org/10.5281/zenodo.20581301>

The binary level files `levels/D*.bin` are generated artifacts. They are not
included in this package because the largest files are multi-GB outputs. They
can be regenerated from the published C source and checked against
`levels_manifest.tsv`, which records counts, byte sizes, and SHA-256 hashes.

The validation evidence included here is:

- `A096242_n13_run_evaluation.md`;
- `A096242_n14_run_evaluation.md`;
- `A096242_level_certifier_n14.md`;
- `A096242_d11_cross_mode_validation.md`;
- `A096242_d11_gmp_reenumeration.md`;
- `levels_manifest.tsv`;
- `cert/a096242_level_certifier.c`;
- `cert/a096242_d11_gmp_reenumerate.c`;
- `references/sinclair_miller_rabin_sprp_bases.md`.

The saved-level certifier verifies that every saved value in the regenerated
levels is prime and has at least one valid base-9 digit deletion into the
previous saved level. It is not a second exhaustive missing-candidate
enumerator.

The D11 cross-mode report records byte-identical production output for
single-thread in-memory and 16-thread forced segmented reruns. The D11 GMP
report records a separate-source re-enumeration using GMP's probabilistic
primality routine. These additional checks strengthen the first new level
only; they are not independent re-enumerations of `D12`, `D13`, or `D14`.

The Sinclair Markdown mirror is included to reduce dependence on the live
`https://miller-rabin.appspot.com/` URL for the exact seven-witness record.
It is a local mirror of the cited web table, not a DOI or a peer-reviewed
publication.
