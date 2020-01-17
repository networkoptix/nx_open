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

The tool can run on either Linux or Windows PC called "Host" here, and can connect to any other
device (it may be ARM or x64) with Linux called "Box" here.

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
* Linux Host: `ssh` and `sshpass` tools should be installed on the Host.
* Windows Host: Firewall should be disabled completely to enable spawning virtual cameras in the
    isolated network.
* VMS Server should be installed on the Box and VMS System should be set up via the Setup Wizard
    in the Server web-admin.
    * No other manipulations with the installed VMS Server should be performed before running the
        tool, including adding cameras and changing the Server setting from the Client - such
        activities may influence the Server state and thus lead to an incorrect report.
* No more than one VMS should be installed on the Box.
* If the Box Linux user is not root, it should be in sudoers and `sudo` should not ask for
    password.
* The Box should be connected to an isolated network together with the host, and this network
    should have the full bandwidth that the Box is planned to work with, e.g. 100 Mbps or 1 Gbps.
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

The tool has a few command-line options which do not influence the test procedure but allow to
override some administrative options like where to find the .conf file or where to save the logs.
Run with `--help` for details.

Do at least the following in the `vms_benchmark.conf` before running the tool:
- Specify VMS username and password in `vmsUser`/`vmsPassword` fields.
- If `ssh <boxHostnameOrIp>` on the Host machine requires to enter credentials, specify them in 
    `boxLogin` and `boxPassword` fields, or specify `boxSshKey` field instead.

Then simply run the command `vms_benchmark` without arguments, and watch or capture its output.
In case of a nominal run, the duration of the test is about 4 hours.

If the tool encounters an error which prevents further testing (like some of the prerequisites is
not fulfilled, or the connection to the Box cannot be established, or some command at the Box gives 
unexpected result), an ERROR is reported and the tool aborts execution. In this case it is
recommended to investigate and fix the error cause, and run the tool again.

If the assessment fails, an ISSUE is reported and the tool aborts execution. Due to the nature of
the assessment process which depends on many environmental characteristics such as the performance
of the Host and the network, sometimes a false-negative assessment may take place - the tool is
trying to be conservative and report an ISSUE when it suspects that the VMS Server performance may
not be optimal on the Box. Thus, in case an ISSUE is reported, it is recommended to run the tool
several times using the same configuration (making sure that the Box is in the proper state before
each run; see below), and estimate the percentage of failures. If the configured load (e.g. number
of cameras and number of streams requested from each camera) is on the edge of the Box
capabilities, the assessment may have a near-50% rate of reporting an ISSUE. Generally, it is
recommended to consider the assessment successful if the rate of reporting an ISSUE is up to 5%.

The Box and Server state after running the tool:

* The tool is not intended to be run on a Server working in customer's production installation,
    unless the whole VMS System is considered experimental and thus its operational status,
    configuration and data are considered not valuable.
* ATTENTION: The tool clears all video archives on the Server, thus, never run the tool on a Server
    with valuable data.
* After a successful run, the tool leaves the Server in a state which is good for re-running the
    tool but may be not suitable for using the Server otherwise without rebooting the Box.
* After a failed run, the tool may leave the Box and/or the Server in a state which is not suitable
    for any further testing and assessment (the exact state depends on the failure nature), thus,
    it is recommended to re-initialize the Box file system.

---------------------------------------------------------------------------------------------------
## Report details

The tool produces the so-called Report on stdout, and additionally writes the log to the log file.
The Report is intended for the operator who runs the test, and the log is intended for the support
team though may contain some information useful for the operator in case of a failure.

The Report starts with the general information collected by the tool from the box.

Every minute of the test a progress line is printed with a brief status information.

On a successful assessment, the tool produces a report that ends with the line:
```
SUCCESS: All tests finished.
```

If an ISSUE (assessment failure) or en ERROR (OS or similar failure) occurs, a message is printed
starting with one of these capitalized words, indicating what has happened. The message may also
include an advice to look in the log file for additional details.

---------------------------------------------------------------------------------------------------
## How it works

Here are the approximate steps the tool performs (with some details omitted here for clarity).

1. Connect to the box via ssh.
2. Collect box system information like Linux version, CPU properties, RAM size, and file systems.
3. Check the time at the box - must be the same as the time at the host.
4. Check that the VMS Server is up and running at the box.
5. Connect to the VMS Server via HTTP API.
6. Clear all VMS Server storages (video archives), restarting the Server.
7. Evaluate whether there is enough free RAM at the box.
8. Evaluate whether there is enough free space at the box for the video archive for the test.
9. Run the load tests:
9.1. Start the required number of virtual cameras on the host.
9.2. Wait until the Server discovers all virtual cameras.
9.3. Enable recording of all virtual cameras by the Server.
9.4. Run the streaming test: for the certain time period, pull some live and recorded video streams
    from the Server, periodically doing the following:
9.4.1. Report a brief status every minute.
9.4.2. Check whether the pulled streams arrive uninterrupted.
9.4.3. Check whether the pulled streams have any frame drops or lags.
9.4.4. Check that the box CPU usage is not higher than 50%.
9.4.5. Check that there are no storage failure reports from the Server.
9.4.6. Check whether there are any network errors at the box.
