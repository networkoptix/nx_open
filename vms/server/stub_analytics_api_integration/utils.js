// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict";

function mergeMaps(map1, map2) {
    return { ...map1, ...map2 };
}

module.exports = {
    mergeMaps
};
