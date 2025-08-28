## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import time

import util.nx_integration_interface_dummy as nx_integration_interface

_REQUEST_ID = 0


def call_integration_function(command: str, parameters: dict[str, str]) -> str:
    cmd = nx_integration_interface.Command(command=command, parameters=parameters)
    nx_integration_interface._REQUEST_QUEUE.put(cmd)


def call_integration_function_with_rv(command: str, parameters: dict[str, str]) -> str:
    global _REQUEST_ID
    _REQUEST_ID += 1
    current_request_id = _REQUEST_ID
    cmd = nx_integration_interface.Command(
        comman=command, request_id=current_request_id, parameters=parameters)

    while not (reply := nx_integration_interface._REPLYS.get(current_request_id, None)):
        time.sleep(0.1)
        pass

    del nx_integration_interface._REPLYS[current_request_id]
    return reply
