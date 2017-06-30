import platform
import sys

supported_targets = [
    "windows",
    "windows-x64",
    "windows-x86",
    "linux",
    "linux-arm",
    "linux-x64",
    "linux-x86",
    "macosx",
    "macosx-x64",
    "rpi",
    "bpi",
    "tx1",
    "isd_s2",
    "edge1",
    "android",
    "android-arm",
    "android-x86",
    "ios",
    "any"
]

additional_targets = {
    "windows-x64": [ "windows" ],
    "windows-x86": [ "windows" ],
    "linux-x64": [ "linux" ],
    "linux-x86": [ "linux" ],
    "macosx-x64": [ "macosx" ],
    "android-arm": [ "android" ],
    "android-x86": [ "android" ],
    "rpi": [ "linux-arm", "linux" ],
    "bpi": [ "linux-arm", "linux" ],
    "edge1": [ "linux-arm", "linux" ],
    "isd_s2": [ "linux-arm", "linux" ],
    "tx1": [ "linux-arm", "linux" ]
}

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

def get_alternative_targets(target):
    alternatives = additional_targets.get(target, [])
    if target != "any":
        alternatives.append("any")
    return alternatives

def get_platform_for_target(target):
    if target.startswith("windows"):
        return "windows"
    elif target.startswith("linux"):
        return "linux"
    elif target.startswith("macosx"):
        return "macosx"
    elif target in [ "rpi", "bpi", "tx1", "isd_s2", "edge1" ]:
        return "linux"
    elif target.startswith("android"):
        return "android"
    elif target == "ios":
        return "ios"
    else:
        return None
