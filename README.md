# NTT_accel_generator
Code generator for NTT accelerator

### Prerequisites

AMD Vitis (2023.2) - https://www.amd.com/en/products/software/adaptive-socs-and-fpgas/vitis.html

TAPA (0.1.20250127) - https://tapa.readthedocs.io/en/main/

Rapidstream (2025.1.0109, recommended) - https://docs.rapidstream-da.com/

### Install Python Requirements
```bash
pip install -r requirements.txt 
```

## Generating an NTT project 

Please use generate_code.py to automatically generate an NTT project.

| Argument | Description | Supported Values | Default |
|----------|-------------|-----------------|---------|
| **N** | Transform size (polynomial degree) | 256, 512, 1024 | 1024 |
| **q** | Prime modulus | 3329, 8380417, 12289 | 12289 |
| **bits** | Bit length of coefficients | 16, 32 | 32 |
| **BU** | Number of butterfly units per stage | 1, 2, 4, 8, 16, 32 | 8 |
| **CH** | Number of input HBM channels <br> (Total of 2×CH channels are used for input & output) | 1, 2, 4, 8 | 4 |

Command: (with default values)
```bash
./generate_code.py -N 1024 -q 12289 -bits 32 -BU 8 -CH 4 
```

Terminal output:
```bash
Values used -> N: 1024, q: 12289, bits: 32, BU: 8, CH: 4, veclen: 16
Number of NTT cores: 4
Creating a new folder: N1024_BU8_CH4
```

## How to use MAKEFILE
Actions (TARGET = sw, xo, hw)
1) all: Host compilation [tapa g++] / Synthesis into RTL [tapa compile] / Bitstream generation [v++]
2) run: sw simulation / RTL simulation / onboard


HOST Compilation
```bash
make all TARGET=sw
```
Synthesize and generate xo file
```bash
make all TARGET=xo
```
Generate bitstream from xo file (no Rapidstream)
```bash
make all TARGET=hw
```
Optimize xo file with Rapidstream and generate bitstream (recommended)
```bash
make all TARGET=hw-opt
```
Simulation / Onboard verification (NUM = number of samples)
```bash
make run TARGET={sw | xo | hw | hw-opt} NUM=10000
```

## Updates
### 2025/01/10
#### Modified
- **ntt.cpp**: implemented input_stage and temporal_stage differently so that it does not require further modification
- **host.cpp**: implemented input & output processing that supports B less than 8 (multiple samples in the same channel)
- **generate_code.py**: creates folder containing necessary files (src, Makefile, link_config.ini) for different configurations 

#### Added
- **gen_config.py**: generates floorplan & device configs for Rapidstream-tapaopt (stored under "all_files" folder)

### 2025/01/17 & 2025/01/18
#### Modified
- **Merging folder / generate_code.py**: merged "all_files" with " templates" and modified generate_code.py accordingly
- **gen_config.py**: Fixed port_pre_assignments
- **Makefile**: Rearranged directories for rapidstream-tapaopt

