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

* Host PC hardware: A physical PC with at least Core i5 quad-core or equivalent, 8 GB RAM, 1 GB
    free disk space.
* Host PC OS: Ubuntu 18.04 or Windows 10.
    * The tool may work with other OS versions, but it is not officially supported.
    * On Windows, running VMS Benchmark from a Cygwin shell may or may not work, and is not
        officially supported.
* Host PC should not run any other software at the time of running VMS Benchmark, besides built-in
    OS components. Such software may or may not interfere with VMS Benchmark and may or may not
    affect its ability to run or the accuracy of its report.
* Linux host: `ssh` and `sshpass` tools should be installed on the host.
* Windows host: Firewall should be disabled completely to enable spawning virtual cameras in the
    isolated network.
* VMS Server should be installed on the box and VMS System should be set up via the Setup Wizard
    in the Server web-admin.
    * No other manipulations with the installed VMS Server should be performed before running the
        tool, including adding cameras and configuring the Server from the Client - such activities
        may influence the Server state and thus lead to an incorrect report.
    * NOTE: After running the tool, it is not recommended to use the Server for further
        experiments without re-installation or at least deleting the Server database and restarting
        the Server, because the tool configures the Server specifically to its needs.
* No more than one VMS should be installed on the box.
* If the box Linux user is not root, it should be in sudoers and `sudo` should not ask for
    password.
* The box should be connected to an isolated network together with the host, and this network
    should have the full bandwidth that the box is planned to work with, e.g. 100 Mbps or 1 Gbps.
    There should be no cameras in this network, but it may be connected to the internet.

If there is a need to contact the Support Team regarding an issue with the VMS Benchmark tool
itself, make sure that all the above listed prerequisites are fulfilled.

---------------------------------------------------------------------------------------------------
## Usage

The tool is distributed in the form of a zip package which is a portable installation - no need to
perform any kind of installation procedure, just unpack the zip into a convenient folder. The
tool's zip file can be found among VMS distribution files for the respective platform (Windows or
Linux x64).

All configuration options for the tool are supplied via a configuration file - a name-value text
file called `vms_benchmark.conf` which should reside next to the tool's executable. Each option has
a detailed comment in this file.

Do at least the following in the `vms_benchmark.conf` before running the tool:
- Specify VMS username and password in `vmsUser`/`vmsPassword` fields.
- If `ssh <boxHostnameOrIp>` on the host machine requires to enter credentials, specify them in 
    `boxLogin` and `boxPassword` fields.
- Set the desired number of cameras in `virtualCamerasCount` field.

Then simply run the command `vms_benchmark` without arguments, and watch or capture its output.

Currently, the tool has no command-line options besides `--help` which shows a trivial help.

If the tool encounters an error which prevents further testing (like some of the prerequisites is
not fulfilled, or the connection to the box cannot be established, or some command at the box gives
unexpected result), an ERROR is reported and the tool aborts execution. In this case it is
recommended to investigate and fix the error cause, and run the tool again.

If the assessment fails, an ISSUE is reported and the tool aborts execution. Due to the nature of
the assessment process which depends on many environmental characteristics such as the performance
of the host and the network, sometimes a false-negative assessment may take place - the tool is
trying to be conservative and report an ISSUE when it suspects that the VMS Server performance may
not be optimal on the box. Thus, in case an ISSUE is reported, it is recommended to run the tool
several times using the same configuration, and estimate the percentage of failures. If the
configured load (e.g. number of cameras and number of streams requested from each camera) is on the
edge of the box capabilities, the assessment may have a near-50% rate of reporting an ISSUE.
Generally, it is recommended to consider the assessment successful if the rate of reporting an
ISSUE is up to 5%.

---------------------------------------------------------------------------------------------------
## Example of successful report

Currently, on successful run the tool produces a report similar to this:
```
VMS Benchmark started.
Box IP: 192.168.0.100
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
