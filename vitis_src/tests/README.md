# How to Build and Run Tests

- Navigate to tests directory:
```bash
cd path/to/vitis_src/tests
```
- Create Build Directory (Keep source clean):

```bash
mkdir build
cd build
```
- Generate Makefiles:

```bash
cmake -G "Unix Makefiles" ..
```
(Note: If using MinGW on Windows, you might need -G "MinGW Makefiles" instead).

```bash
make
```
- Run the test binary:
```bash
./test_fixed_point.exe
```