class NxBoxToolError(Exception):
    pass


class TestCameraStreamingError(NxBoxToolError):
    pass


class DeviceStateError(NxBoxToolError):
    pass


class ServerError(NxBoxToolError):
    pass


class ServerApiError(NxBoxToolError):
    pass


class TestCameraError(NxBoxToolError):
    pass


class InsuficientResourcesError(NxBoxToolError):
    pass
