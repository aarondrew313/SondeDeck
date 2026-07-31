# rs1729 RS41 FEC wrapper

`bch_ecc_upstream.inc` is the upstream `bch_ecc.c` source from `rs1729/RS`.

It must not be compiled directly as its own source file. It is included from
`rs1729_fec.c` after the required upstream typedefs are defined:

```c
typedef unsigned char ui8_t;
typedef unsigned int ui32_t;
```

This mirrors the structure used by upstream `rs41ecc.c`, where the typedefs
are defined before including `bch_ecc.c`.
