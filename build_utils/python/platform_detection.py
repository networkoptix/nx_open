import platform
import sys

supported_targets = [
    "windows-x64",
    "windows-x86",
    "linux-x64",
    "linux-x86",
    "macos-x64",
    "rpi",
    "bpi",
    "isd",
    "isd_s2",
    "android-arm",
    "ios"
]

def detect_platform():
    platform = sys.platform
    if platform.startswith("win32") or platform.startswith("cygwin"):
        return "windows"
    elif platform.startswith("linux"):
        return "linux"
    elif platform.startswith("darwin"):
        return "macos"
    else:
        return "unknown"

def detect_arch():
    arch = platform.machine()
    if arch == "x86_64" or arch == "AMD64":
        return "x64"

    return arch

def detect_target():
    return detect_platform() + "-" + detect_arch()
