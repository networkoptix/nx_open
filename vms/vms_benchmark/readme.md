// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# VMS Benchmark

---------------------------------------------------------------------------------------------------
## Introduction

VMS Benchmark is a command-line tool which allows a partner/customer to assess the ability to run
VMS on a Linux-based device. The tool runs on a host PC, connects to the device, collects system
information such as CPU type and RAM size, and then starts the VMS Server, feeds it test video
streams from virtual cameras, and creates a detailed report about potential performance issues.

---------------------------------------------------------------------------------------------------
## Prerequisites

The tool can run on either Linux or Windows PC called "host" here, and can connect to any other
device (it may be ARM or x64) with Linux called "box" here.

The following prerequisites should be assured before running the tool:

* Linux host: `ssh` and `sshpass` tools should be installed on the host in case of Linux.
* VMS Server should be installed on the box and VMS System should be set up.
* The box Linux user should be in sudoers and `sudo -i` should not require typing a password.
* The box should be in "known_hosts" on the host - `ssh <box>` should not ask for a confirmation.

---------------------------------------------------------------------------------------------------
## Usage

The tool is distributed in the form of a zip package which is a portable installation - no need to
perform any kind of installation procedure, just unpack the zip into a convenient folder. The
tool's zip file can be found among VMS distribution files for the respective platform (Windows or
Linux x64).

All configuration options for the tool are supplied via a configuration file - a name-value text
file called `vms_benchmark.conf` which should reside next to the tool's executable.

Do at least the following in the `vms_benchmark.conf` before running the tool:
- Specify VMS username and password in `vmsUser`/`vmsPassword` fields.
- If `ssh <deviceHost>` on the host machine requires to enter credentials, specify them in 
    `deviceLogin` and `devicePassword` fields.

Then simply run the command `vms_benchmark` without arguments, and watch or capture its output.

Currently, the tool has no command-line options besides `--help` which shows a trivial help.

---------------------------------------------------------------------------------------------------
## Example of successful report

Currently, on successful run the tool produces a report similar to this:
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
    Waiting for test cameras discovering... (timeout is 120 seconds)
    All test cameras had been discovered successfully.
    Recording on camera e3e9a385-7fe0-3ba5-5482-a86cde7faf48 enabled.
    RTSP stream opened. Test duration: 300 seconds.
    Serving 1 cameras is OK.
    Frame drops: 0
    CPU usage: 1.50
    Free RAM usage: 389M

All tests are successfully finished.
```
