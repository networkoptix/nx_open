import platform
import sys

supported_targets = [
    "windows-x64",
    "windows-x86",
    "linux-x64",
    "linux-x86",
    "macosx-x64",
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
        return "macosx"
    else:
        return "unknown"

def detect_arch():
    arch = platform.machine()
    if arch == "x86_64" or arch == "AMD64":
        return "x64"

    return arch

def detect_target():
    return detect_platform() + "-" + detect_arch()

def get_platform_for_target(target):
    if target.startswith("windows"):
        return "windows"
    elif target.startswith("linux"):
        return "linux"
    elif target.startswith("macosx"):
        return "macosx"
    elif target in [ "rpi", "bpi", "isd", "isd_s2" ]:
        return "linux"
    elif target.startswith("android"):
        return "android"
    elif target == "ios":
        return "ios"
    else:
        return None
