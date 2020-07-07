# VMS Benchmark

// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introduction

VMS Benchmark (called the "Tool" here) is a command-line tool which allows a partner/customer to
assess the ability to run VMS on a Linux-based device. The Tool runs on a host PC, connects to the
device, collects system information such as CPU type and RAM size, and then starts the VMS Server,
feeds it test video streams from virtual cameras, and creates a detailed report about potential
performance issues.

### Disclaimer

This tool is intended to run a preliminary compatibility check and burn-in test on ARM-based
hardware. The tool is in BETA, the results are preliminary and can be used for estimation only.
Additionally, please see the following article:
[ARM Support Policy](https://support.networkoptix.com/hc/en-us/articles/360035919314-ARM-Support-Policy)

---------------------------------------------------------------------------------------------------
## Prerequisites

The Tool can run on either Linux or Windows PC called the "Host" here, and can connect to any other
device (it may be ARM or x64) with Linux called the "Box" here, via either SSH or Telnet.

The following prerequisites must be assured before running the Tool:

- Host PC hardware: A physical PC with at least Core i5 quad-core or equivalent, 8 GB RAM, 1 GB
    free disk space.
- Host PC OS: Ubuntu 18.04 or Windows 10.
    - The Tool may work with other OS versions, but it is not officially supported.
    - On Windows, running VMS Benchmark from a Cygwin shell may or may not work, and is not
        officially supported.
- Host PC must not run any other software at the time of running VMS Benchmark, besides built-in
    OS components, and all OS background activities like auto-updating, search indexing, antivirus
    and other scheduled tasks like storage optimization must be turned off. Also, in case an SSD
    storage is used on the Host, it must not be too worn-out or have less than 10% free space,
    because otherwise an SSD may introduce system-wide freezes for fractions of a second which is
    long enough compared to the video stream frame rate.
    Such software/activities may or may not interfere with VMS Benchmark and may or may not
    affect its ability to run or the accuracy of its report.
- Both the Host and the Box should have the system clock set to the same date/time and time zone.
    Network time synchronization (e.g. via NTP protocol) or other means of automatically adjusting
    the system date/time must be disabled on both the Host and the Box - otherwise the Tool may
    falsely report issues like frame drops and stream lags.
- Linux Host: If SSH is going to be used to connect to the Box, `ssh` and `sshpass` tools must be
  installed on the Host.
    * To install these tools, run the command: `sudo apt install openssh-client sshpass`
- Windows Host: Firewall must be disabled completely to enable spawning virtual cameras in the
    isolated network.
- VMS Server must be installed on the Box and VMS System must be set up via the Setup Wizard
    in the Server web-admin.
    - ATTENTION: VMS Server must be installed and configured exactly as planned for production use,
        including the proper video archive storage configuration and choosing a suitable storage
        type for the Server logs. For detailed recommendations about configuring
        VMS Server on a device, please refer to the following article:
        [Building Linux-based NVRs](https://support.networkoptix.com/hc/en-us/articles/360035982154)
        If the recommendations from this article are not followed carefully, the Server performance
        and reliability may be seriously compromised, which may or may not be detected by the Tool.
    - For details about installing VMS Server on an ARM-based device, refer to the following
        article:
        [ARM Single Board Computer (SBC) Support & Installation Instructions](https://support.networkoptix.com/hc/en-us/articles/360033842053)
    - No other manipulations with the installed VMS Server should be performed before running the
        tool, including adding cameras and changing the Server settings from the Client - such
        activities may influence the Server state and thus lead to an incorrect report.
- No more than one VMS must be installed on the Box.
- If the Box Linux user is not root, it must be in sudoers and `sudo` must not ask for password.
    - On a Debian-based Box, the following commands can be used to achieve these goals:
        - To add the user `<username>` to sudoers: `sudo usermod -aG sudo <username>`
            - To check, run a command like `sudo ls`, it will ask for the user's password.
        - To allow `sudo` to work without password: `sudo visudo`, then add the following line to
            the end of the file, save it and exit the editor: `<username> ALL=(ALL) NOPASSWD:ALL`
            - To check: run a command like `sudo ls`, it will work without asking for the password.
- The Box must be connected to an isolated network together with the host, and this network
    must have the full bandwidth that the Box is planned to work with, e.g. 100 Mbps or 1 Gbps.
    There must be no cameras in this network, but it may be connected to the internet.

If there is a need to contact the Support Team regarding an issue with the VMS Benchmark tool
itself, make sure that all the above listed prerequisites are fulfilled.

---------------------------------------------------------------------------------------------------
## Usage

The Tool is distributed in the form of a zip package which is a portable installation - no need to
perform any kind of installation procedure, just unpack the zip into a convenient folder. The
tool's zip file can be found among VMS distribution files for the respective platform (Windows or
Linux x64).

All configuration options for the Tool are supplied via a configuration file - a name-value text
file called `vms_benchmark.conf` which must reside next to the Tool's executable. Each option has
a detailed comment in this file.

The Tool has a few command-line options which do not influence the test procedure but allow to
override some administrative options like where to find the .conf file or where to save the logs.
Run with `--help` for details.

Do at least the following in the `vms_benchmark.conf` before running the Tool:
- Specify VMS username and password in `vmsUser`/`vmsPassword` fields.
- If using Telnet to connect to the box:
    - Specify `boxConnectByTelnet=True` option.
    - Specify the credentials in `boxLogin` and `boxPassword` fields.
    - NOTE: In certain cases, a telnet connection can be very slow, requiring up to a few seconds.
        This is known to be caused by telnet daemon on the Box trying to resolve the host name of
        the Host. The Tool creates a new connection for each command it executes on the Box, thus,
        in this case the assessment may fail due to slow command execution. If the Tool suspects a
        slow Telnet connection, it prints a warning and still attempts to run.
        - To fix it, adding the Host IP address to `/etc/hosts` of the Box may help.
- If using SSH to connect to the box:
    - If `ssh <boxHostnameOrIp>` on the Host machine requires to enter credentials, specify them in
        `boxLogin` and `boxPassword` fields, or specify `boxSshKey` field instead.
- ATTENTION: The provided `vms_benchmark.conf` file has all options listed with their default
    values, but such lines are commented out via putting a `#` sign at the beginning of the line.
    When changing a value, do not forget to remove this sign to uncomment the line.

Then simply run the command `vms_benchmark` without arguments, and watch or capture its output.
In case of a nominal run, the duration of the test is about 4 hours.

If the Tool encounters an error which prevents further testing (like some of the prerequisites is
not fulfilled, or the connection to the Box cannot be established, or some command at the Box gives
unexpected result), an ERROR is reported and the Tool aborts execution. In this case it is
recommended to investigate and fix the error cause, and run the Tool again.

If the assessment fails, an ISSUE is reported and the Tool aborts execution. Due to the nature of
the assessment process which depends on many environmental characteristics such as the performance
of the Host and the network, sometimes a false-negative assessment may take place - the Tool is
trying to be conservative and report an ISSUE when it suspects that the VMS Server performance may
not be optimal on the Box. Thus, in case an ISSUE is reported, it is recommended to run the Tool
several times using the same configuration (making sure that the Box is in the proper state before
each run; see below), and estimate the percentage of failures. If the configured load (e.g. number
of cameras and number of streams requested from each camera) is on the edge of the Box
capabilities, the assessment may have a near-50% rate of reporting an ISSUE. Generally, it is
recommended to consider the assessment successful if the rate of reporting an ISSUE is up to 5%.

The Box and Server state after running the Tool:

* The Tool is not intended to be run on a Server working in customer's production installation,
    unless the whole VMS System is considered experimental and thus its operational status,
    configuration and data are considered not valuable.
* ATTENTION: The Tool clears all video archives on the Server, thus, never run the Tool on a Server
    with valuable data.
* After a successful run, the Tool leaves the Server in a state which is good for re-running the
    tool but may be not suitable for using the Server otherwise without rebooting the Box.
* After a failed run, the Tool may leave the Box and/or the Server in a state which is not suitable
    for any further testing and assessment (the exact state depends on the failure nature), thus,
    it is recommended to re-initialize the Box file system.

---------------------------------------------------------------------------------------------------
## Report details

The Tool produces the so-called Report on stdout, and additionally writes the log to the log file.
The Report is intended for the operator who runs the test, and the log is intended for the support
team though may contain some information useful for the operator in case of a failure.

The Report starts with the general information collected by the Tool from the box.

Every minute of the test a progress line is printed with a brief status information.

On a successful assessment, the Tool produces a report that ends with the line:
```
SUCCESS: All tests finished.
```

If an ISSUE (assessment failure) or en ERROR (OS or similar failure) occurs, a message is printed
starting with one of these capitalized words, indicating what has happened. The message may also
include an advice to look in the log file for additional details.

---------------------------------------------------------------------------------------------------
## How it works

Here are the approximate steps the Tool performs (with some details omitted here for clarity).

1. Connect to the box via SSH or Telnet.
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
