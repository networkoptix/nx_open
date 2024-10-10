// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict";

class InvalidTargetError extends Error{
    constructor(message) {
        super(message);
        this.name = "InvalidTargetError"
    }
}

module.exports = {
    InvalidTargetError: InvalidTargetError
};
