// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Nx Box tool

---------------------------------------------------------------------------------------------------
## License

The whole contents of this package, including all Python source code, is licensed as Open Source
under the terms of Mozilla Public License 2.0: www.mozilla.org/MPL/2.0/, see the license text in
`license_mpl2.md` file in the root directory of this package.

---------------------------------------------------------------------------------------------------
## CLI help

```
./nx_box_tool [OPTIONS]

    --help Show help message and exit.
```

---------------------------------------------------------------------------------------------------
## Sample: run tests

Prerequisites:
* `sshpass` tool should be installed on the host.
* VMS should be installed on the box and the license should be activated.

Modify options in `nx_box_tool.conf` config file:

* Specify box hostname or IP-address in the `deviceHost` option.
* If `ssh <deviceHost>` on the host machine requires type the credentials, write they in 
`deviceLogin` and `devicePassword` options.

Then simply run command:
```
./nx_box_tool
```

Example of successful report:

```
Device IP: 192.168.0.100
Arch: armv7l
Number of CPUs: 12
CPU features: NEON
RAM: 783978496 (140042240 free)
Volumes:
    ext4 on /: free 9392750592 of 30450716672
    vfat on /boot: free 22053376 of 45281280
Detected VMS installations:
    networkoptix in /opt/networkoptix/mediaserver (port 7001, PID 22033)
Restarting server...
Server restarted successfully.
Free space of RAM: 211M of 748M
API test is OK.
Try to serve 1 cameras.

    Spawned 1 test cameras.
    Waiting for test cameras descovering... (timeout is 120 seconds)
    All test cameras had been discovered successfully.
    Recording on camera e3e9a385-7fe0-3ba5-5482-a86cde7faf48 enabled.
    RTSP stream opened. Test duration: 300 seconds.
    Serving 1 cameras is OK.
    Frame drops: 0
    CPU usage: 1.50
    Free RAM usage: 389M

All tests are successfully finished.
```