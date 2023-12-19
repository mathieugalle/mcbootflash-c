# mcbootflash-cpp 🚢

**Why?** 🤔

Flash (update) PIC24 bootloader applications from an embedded platform without Python. My company, [Vitirover](https://vitirover.com/), needs to update the embedded code during the lifecycle of the robot 🤖.

Special thanks to [mcbootflash by bessman](https://github.com/bessman/mcbootflash) for the inspiration.

This repository is a C++ rendition of the original mcbootflash. It has no dependencies except the standard library. [bessman](https://github.com/bessman) used the **bincopy** Python library for handling the hex Intel Microchip format. I've transcribed it in C++ (focusing only on the parts necessary for mcbootflash) in **hexfile.h** / **hexfile.cpp**.

## Unit Tests 🧪

For unit testing, I employed [doctest](https://github.com/doctest/doctest). I used the same test hex file as bessman (mcbootflash/tests/testcases/flash/test.hex) for consistent comparison. Additionally, I utilized Jupyter notebooks (/notebooks) to test and translate Python tests to C++.

I also added a hex file compiled with Microchip, _VitiAppDelivery.X.production_, to ensure the generated chunks match those in mcbootflash. This hex file includes some _IHEX_EXTENDED_LINEAR_ADDRESS_ lines, not tested by the test.hex file.

Compilation should be straightforward; the makefile is quite simple.

Note: There is no main function as such. Instead, this code uses the following snippet to run all tests, making it an executable project rather than a compiled library.

```cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
```
(in mcbootflash-cpp.cpp)
As of now, all my tests are passing with flying colors ✅.
