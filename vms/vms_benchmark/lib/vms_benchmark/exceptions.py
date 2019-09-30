class VmsBenchmarkError(Exception):
    def __init__(self, message, original_exception=None):
        super(VmsBenchmarkError, self).__init__(message)
        self.original_exception = original_exception


class TestCameraStreamingError(VmsBenchmarkError):
    pass


class DeviceStateError(VmsBenchmarkError):
    pass


class DeviceCommandError(VmsBenchmarkError):
    pass


class ServerError(VmsBenchmarkError):
    pass


class ServerApiError(VmsBenchmarkError):
    pass


class TestCameraError(VmsBenchmarkError):
    pass


class HostPrerequisiteFailed(VmsBenchmarkError):
    pass


class DevicePrerequisiteFailed(VmsBenchmarkError):
    pass


class SshHostKeyObtainingFailed(VmsBenchmarkError):
    pass


class InsuficientResourcesError(VmsBenchmarkError):
    pass
