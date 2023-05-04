# FetchBench

Code artifact for the submission "FetchBench: Systematic Identification and Characterization of Proprietary Prefetchers" to CCS2023.

This repository contains:

- `fetchbench/`: Our prefetcher identification and characterization framework (sections 3 and 4)
- `aes-attack/`:  Our first case study (section 5.1): leaking parts of an AES key through the Cortex-A72 SMS prefetcher
- `covert-channel/`: Our second case study (section 5.2): a covert channel between normal world and secure world based on the Cortex-A72 SMS prefetcher