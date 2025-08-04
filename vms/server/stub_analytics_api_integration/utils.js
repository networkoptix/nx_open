// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict";

function mergeMaps(map1, map2) {
    return { ...map1, ...map2 };
}

function boundingBoxToString(boundingBox) {
    return `${boundingBox.x},${boundingBox.y},${boundingBox.width}x${boundingBox.height}`;
}

function generateRandomVector() {
    const kVectorSize = 768;
    const data = new Float32Array(kVectorSize);

    // Fill with random values (uniform distribution between 0.0 and 1.0)
    for (let i = 0; i < kVectorSize; ++i)
    {
        data[i] = Math.random();
    }

    // Normalize to unit vector (L2 norm = 1)
    let norm = Math.sqrt(data.reduce((sum, val) => sum + val * val, 0));
    if (norm > 0.0)
    {
        for (let i = 0; i < kVectorSize; ++i)
            data[i] /= norm;
    }

    // Convert to byte array in little-endian format
    const output = new Uint8Array(kVectorSize * 4);
    const view = new DataView(output.buffer);

    for (let i = 0; i < kVectorSize; ++i)
    {
        view.setFloat32(i * 4, data[i], true); // true = little endian
    }

    return Array.from(output);
}

module.exports = {
    mergeMaps,
    boundingBoxToString,
    generateRandomVector
};
