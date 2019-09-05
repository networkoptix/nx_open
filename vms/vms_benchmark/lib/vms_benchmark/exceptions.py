class VmsBenchmarkError(Exception):
    pass


class TestCameraStreamingError(VmsBenchmarkError):
    pass


class DeviceStateError(VmsBenchmarkError):
    pass


class ServerError(VmsBenchmarkError):
    pass


class ServerApiError(VmsBenchmarkError):
    pass


class TestCameraError(VmsBenchmarkError):
    pass


class InsuficientResourcesError(VmsBenchmarkError):
    pass
