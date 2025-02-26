// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict";

function mergeMaps(map1, map2) {
    return { ...map1, ...map2 };
}

function boundingBoxToString(boundingBox) {
    return `${boundingBox.x},${boundingBox.y},${boundingBox.width}x${boundingBox.height}`;
}

module.exports = {
    mergeMaps,
    boundingBoxToString
};
