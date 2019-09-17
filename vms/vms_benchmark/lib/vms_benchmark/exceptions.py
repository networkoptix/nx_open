class VmsBenchmarkError(Exception):
    def __init__(self, message, original_exception=None):
        super(VmsBenchmarkError, self).__init__(message)
        self.original_exception = original_exception


class DeviceStateError(VmsBenchmarkError):
    pass


class DeviceCommandError(VmsBenchmarkError):
    pass


class UnableToFetchDataFromDevice(VmsBenchmarkError):
    pass


class DeviceFileContentError(VmsBenchmarkError):
    def __init__(self, path):
        super(DeviceFileContentError, self).__init__(f"File '{path}' has unexpected content")


class ServerError(VmsBenchmarkError):
    pass


class ServerApiError(VmsBenchmarkError):
    pass


class TestCameraError(VmsBenchmarkError):
    pass


class VmsBenchmarkIssue(VmsBenchmarkError):
    pass


class StorageFailuresIssue(VmsBenchmarkIssue):
    def __init__(self, failures_count):
        super(StorageFailuresIssue, self).__init__(f"{failures_count} storage failures detected")


class CPUUsageThresholdExceededIssue(VmsBenchmarkIssue):
    def __init__(self, cpu_usage, threshold):
        super(CPUUsageThresholdExceededIssue, self).__init__(
            f"CPU usage {round(cpu_usage*100)}% exceeds maximum ({round(threshold*100)}%)"
        )


class TestCameraStreamingIssue(VmsBenchmarkIssue):
    pass


class HostPrerequisiteFailed(VmsBenchmarkError):
    pass


class DevicePrerequisiteFailed(VmsBenchmarkError):
    pass


class SshHostKeyObtainingFailed(VmsBenchmarkError):
    pass


class InsuficientResourcesError(VmsBenchmarkError):
    pass
