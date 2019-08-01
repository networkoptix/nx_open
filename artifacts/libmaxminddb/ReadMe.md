libmaxminddb: https://github.com/maxmind/libmaxminddb

libmaxminddb is licensed under the Apache License Version 2.0

This library is compatible with cmake.

## Usage

To include headers in your project:

```
#include <maxminddb.h>
```

to link against the shared library:

```
nx_add_target(<your-lib> maxminddb)
```
or
```
target_link_libraries(<your-lib> maxminddb)
```

## Current Version: 
### 1.3.2 (update me if you update the sources)

## Updating to a newer version:
1. Generate maxminddb_config.h 
    - using configure (on Linux) 
    - in the ```projects``` directory of the github repository (on Windows)
2. In ```include``` directory replace:
    - ```maxminddb.h```
    - ```maxminddb_config.h``` from step 1.
3. Replace all source files in ```src```
4. In CmakeLists.txt, replace version number (Linux mmdblookup utility won't compile otherwise)
