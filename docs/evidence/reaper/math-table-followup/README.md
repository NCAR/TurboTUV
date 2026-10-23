# Larger-table spectral exponential screening after 501c331

These private prototypes replace the 64-entry, degree-five spectral
exponential with 256/512/1024-entry degree-four or 2048-entry degree-three
variants. Constants come from 100-digit Decimal arithmetic; the generated
preliminary error budgets are not completed independent validation. The
production math helper is unchanged.

Both compilers pass catalogue and primary screening gates for all four
variants. The 256/512-entry variants also pass the original 1,000-spectrum
stress test and improve the short primary screens by roughly 2–8 us per
column. The larger variants fail stress trial 373 at 93.280 epsilon and are
not timed. Every screening sample and failed gate is retained.

The failure exposed a separate interpolation issue: a bin mixing ordinary
segments and zero padding outside the guarded geometry could enter the
ordered interpolation loop without checking cancellation in its ordinary
segments. Expanding the published implementation's stress test also finds a
33.307-epsilon failure at trial 1338 with the existing 64-entry helper.
`diagnostic/selected-stress.log` preserves that failure; the debug log identifies
an unchecked mixed bin in trial 373.

The mixed-bin prototype checks the ordinary segments even when another
segment is zero-valued padding, and replays libm/ordered interpolation if any
unguarded segment is nonzero. All five math variants then pass 100,000 spectra,
7,135,407 values and 5,264 injected optional-allocation failures. Maxima are
9.162 epsilon for the current math helper, 8.560 for 256/512 entries, and 8.858
for 1024/2048 entries. This supports fixing interpolation first; it does not
complete independent proof/oracle validation of the larger tables.

`prototype-source.tar.gz` preserves every math header, spectrum source, stress
and diagnostic harness, and reproduction driver. Run the table sweep from
`501c331` with its selected `r3c` base builds. The mixed-bin diagnostic script
uses its archived private source. The subsequent production correction and
full validation are recorded separately in `../mixed-bin/`.

No larger table is selected at this checkpoint. Remaining work includes the
independent uniform reduction/error argument, all expanded MPFR boundaries,
exception/rounding tests, full scientific suites and new combined timings if
one of these candidates is retained. Source hashes and failed tests must not
be interpreted as approval to deploy a math prototype.
