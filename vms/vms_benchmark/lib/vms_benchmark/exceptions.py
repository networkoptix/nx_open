class VmsBenchmarkError(Exception):
    def __init__(self, message, original_exception=None):
        super(VmsBenchmarkError, self).__init__(message)
        self.original_exception = original_exception


class BoxStateError(VmsBenchmarkError):
    pass


class BoxCommandError(VmsBenchmarkError):
    pass


class UnableToFetchDataFromBox(VmsBenchmarkError):
    pass


class BoxFileContentError(VmsBenchmarkError):
    def __init__(self, path):
        super(BoxFileContentError, self).__init__(f"File {path!r} has unexpected content")


class ServerError(VmsBenchmarkError):
    pass


class ServerApiError(VmsBenchmarkError):
    pass


class ServerApiResponseError(VmsBenchmarkError):
    pass


class TestCameraError(VmsBenchmarkError):
    pass


class RtspPerfError(VmsBenchmarkError):
    pass


class VmsBenchmarkIssue(VmsBenchmarkError):
    def __init__(self, message, original_exception=None, sub_issues=[]):
        super(VmsBenchmarkIssue, self).__init__(message, original_exception=original_exception)
        self.sub_issues = sub_issues


class StorageFailuresIssue(VmsBenchmarkIssue):
    def __init__(self, failures_count):
        super(StorageFailuresIssue, self).__init__(f"{failures_count} Storage failures detected")


class CpuUsageThresholdExceededIssue(VmsBenchmarkIssue):
    def __init__(self, cpu_usage, threshold):
        super(CpuUsageThresholdExceededIssue, self).__init__(
            f"CPU usage {round(cpu_usage*100)}% exceeds maximum ({round(threshold*100)}%)."
        )


class TestCameraStreamingIssue(VmsBenchmarkIssue):
    pass


class HostPrerequisiteFailed(VmsBenchmarkError):
    pass


class BoxPrerequisiteFailed(VmsBenchmarkError):
    pass


class SshConnectionError(VmsBenchmarkError):
    pass

class SshHostKeyObtainingFailed(VmsBenchmarkError):
    pass


class InsufficientResourcesError(VmsBenchmarkError):
    pass


class HostOperationError(VmsBenchmarkError):
    pass
