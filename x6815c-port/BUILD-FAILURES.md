# Build failure inventory

## Attempt 1 — invocation

Target objects were sent to host GCC because this old MediaTek tree overrides environment-only `CC`. Connectivity module Makefiles also lacked the variables normally exported only for built-in WLAN. Fixed by passing LLVM and MT6877/Connac variables directly on every make command.

## Attempt 2 — host tools and missing X6815C camera source

The corrected compile reached preparation and driver traversal.

Host dependencies missing from the workspace image:

- `bc`
- OpenSSL development headers needed by `scripts/extract-cert`

Local persistent builds were started for both.

The baseline lacks the four camera sensor source directories selected by stock `CONFIG_CUSTOM_KERNEL_IMGSENSOR`:

- `s5kjn1_mipi_raw`
- `hi1631q_mipi_raw`
- `gc02m1_mipi_raw`
- `gc02m1macro_mipi_raw`

These are real missing vendor implementations and will not be replaced with fabricated stubs. `configs/build-probe.fragment` disables camera integration only in a separate probe output so compilation can inventory additional gaps. The extracted stock config remains unchanged.
