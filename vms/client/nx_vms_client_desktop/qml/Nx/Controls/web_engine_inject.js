// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

'use strict';

window.channel = new QWebChannel(qt.webChannelTransport, function(channel)
{
    Object.keys(channel.objects).forEach(function(name)
    {
        let parts = name.split(".")
        const last = parts.pop()
        let target = parts.reduce(
            (targetObject, part) =>
            {
                if (!targetObject[part])
                    targetObject[part] = {}
                return targetObject[part]
            }, window)
        target[last] = channel.objects[name]
    });

    if (window.vmsApiInit)
        window.vmsApiInit()
});
