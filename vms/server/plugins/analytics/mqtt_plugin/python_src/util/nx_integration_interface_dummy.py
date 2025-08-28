## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

from dataclasses import dataclass, field
from queue import SimpleQueue
import logging

IRRELEVANT_REQUEST_ID = -1
EMPTY_REQUEST_ID = -2

_REQUEST_QUEUE = SimpleQueue()
_REPLYS: dict[int, str] = {}

logger = logging.getLogger(f"mqtt_plugin.{__name__}")


@dataclass
class Command:
    command: str
    request_id: int = IRRELEVANT_REQUEST_ID
    parameters: dict[str, str] = field(default_factory=dict)


def get_host_request() -> Command:
    return _REQUEST_QUEUE.get()


def return_app_reply(request_id: int, reply: str) -> None:
    if request_id == IRRELEVANT_REQUEST_ID:
        logger.warning("Reply for the request that did not want a reply: {reply}")
        return

    _REPLYS[request_id] = reply
