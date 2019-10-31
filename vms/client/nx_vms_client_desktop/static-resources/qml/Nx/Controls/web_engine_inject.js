'use strict';

window.channel = new QWebChannel(qt.webChannelTransport, function(channel)
{
    Object.keys(channel.objects).forEach(function(name)
    {
        window[name] = channel.objects[name];
    });
});
