// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"use strict";

function mergeMaps(map1, map2) {
    return { ...map1, ...map2 };
}

function boundingBoxToString(boundingBox) {
    return `${boundingBox.x},${boundingBox.y},${boundingBox.width}x${boundingBox.height}`;
}

function createLogger(scope) {
    const globalConsole = globalThis.console || console;
    const prefix = `[${scope}]`;

    const write = (level, ...args) => {
        const levelPrefix = `[${level}]`;
        if (!args || args.length === 0)
        {
            globalConsole.log(prefix, levelPrefix);
            return;
        }

        globalConsole.log(prefix, levelPrefix, ...args);
    };

    return {
        log: (...args) => write("LOG", ...args),
        info: (...args) => write("INFO", ...args),
        warn: (...args) => write("WARN", ...args),
        error: (...args) => write("ERROR", ...args)
    };
}

module.exports = {
    mergeMaps,
    boundingBoxToString,
    createLogger
};
