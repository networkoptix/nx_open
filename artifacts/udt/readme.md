This directory contains in `src/udt/` the modified contents of the `udt4/src/` directory of the UDT
library.

The original UDT library:
- Performs UDP-based Data Transfer.
- Written Yunhong Gu: yunhong.gu@gmail.com
- Licensed under the BSD 3-clause license: https://opensource.org/licenses/BSD-3-Clause
- Hosted at https://udt.sourceforge.io/
- Source code repository: https://git.code.sf.net/p/udt/git
- Contains `md5.h` and `md5.cpp`:
    - Written by L. Peter Deutsch: ghost@aladdin.com
    - Copyright 1999, 2002 Aladdin Enterprises.
    - Licensed under a custom permissive license included in the source code comments.
        - The `md5` module license requires that any modifications are plainly marked as such.

The commit taken from the original UDT SourceForge GIT repo was 354338d5be52, and then the code has
been heavily rewritten here by Network Optix, Inc.
