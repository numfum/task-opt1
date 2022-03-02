# Optimisation Task

The task is optimise the `create_etc1_to_dxt1_6_conversion_table()` table generation function, showing timings before and after.  Example timings for comparison:

| Machine             | OS            | CPU           | Compiler | Time  |
|---------------------|---------------|---------------|----------|-------|
| Mac Pro (Late 2013) | Windows 10    | Intel Xeon E5 | MSVC 19  | 410ms |
| Mac Pro (Late 2013) | macOS Big Sur | Intel Xeon E5 | Clang 13 | 334ms |
| Talos II            | Debian Buster | IBM Power9    | GCC 10   | 302ms |

For macOS/Linux build with:
```
cc -Wall -Wextra -O3 -g0 main.cpp
```
For Windows, in a Visual Studio Command Prompt, build with:
```
cl /wd4576 /W3 /O2 main.cpp
```
Warning C4576 is for the initialiser list. Feel free to tweak the compiler flags, `-flto` or `/GL` for example (but both optimised and unoptimised runs should be compared with the same flags).

The following (artificial) limits are imposed:
1. Use a single thread. Whilst the code can be parallelised with ease, the task is to see what optimisations can be applied to the table generation.
2. Generate the table. The ultimate unbeatable optimisation is to simply include the pre-generated table, but that defeats the task.
3. It must compile with Clang/GCC/MSVC. Again, an easy route would be compiling with [`ispc`](//ispc.github.io) or similar SPMD compilers, but this would also defeat the task. It _would_ be interesting to compare the results though.
4. No using OpenMP's `#pragma omp parallel` or similar libraries or preprocessors.

Any questions, feel free to ask. Any SIMD optimisations can be for an architecture or your choice, the interesting point being the before and after timings. We have implementations for SSE4.1, Neon and Wasm.

This is a snippet of code from [Basis Universal](//github.com/BinomialLLC/basis_universal) (copyright 2019-2021 Binomial LLC, released under an Apache 2.0 license). The [original code](//github.com/BinomialLLC/basis_universal/blob/77b7df8e5df3532a42ef3c76de0c14cc005d0f65/transcoder/basisu_transcoder.cpp#L1178-L1253) is extracted from the ETC1S to DXT transcoder.
