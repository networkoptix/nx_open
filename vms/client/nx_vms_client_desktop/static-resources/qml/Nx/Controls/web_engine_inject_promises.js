'use strict';

window.channel = new QWebChannel(qt.webChannelTransport, function(channel)
{
    // Wrap each slot call of an object into a function which returns slot result as ES6 promise.
    function wrapObject(name, obj)
    {
        if (typeof(obj.__objectSignals__) !== 'object')
            return;

        var qtFunctions = ['unwrapQObject', 'unwrapProperties', 'propertyUpdate', 'signalEmitted', 'deleteLater'];

        Object.keys(obj).forEach(function(funcName)
        {
            // Skip non-functions, signals and Qt WebChannel functions.
            if (typeof(obj[funcName]) !== 'function')
                return;
            if (typeof(obj.__objectSignals__[funcName]) !== 'undefined')
                return;
            if (qtFunctions.includes(funcName))
                return;

            // WebChannel API expects to pass slot result to a function which is the last
            // argument in a slot call:
            //
            //     slotName(a, b, function(result){ })
            //
            // Rename the slot into _original_slotName and replace slotName with a function
            // which returns a Promise object:
            //
            //     slotName(a, b).then(function(result){ })
            //
            // This way code in server's web admin can handle both old (QWebKit) and new (QWebEngine) calls.
            var savedFuncName = '_original_' + funcName;
            obj[savedFuncName] = obj[funcName];
            obj[funcName] = function()
            {
                var funcThis = this;

                // Convert arguments into an array.
                var funcArgs = new Array(arguments.length);
                for (var i = 0; i < funcArgs.length; ++i)
                    funcArgs[i] = arguments[i];

                // Return a Promise which is resolved when the last argument of original
                // slot function is called.
                return new Promise(function(resolve, reject)
                {
                    funcThis[savedFuncName].apply(funcThis, funcArgs.concat([function(result){ resolve(result); }]));
                });
            };
            console.log('Promise: ' + name + '.' + funcName);
        });

        window[name] = obj;
    };

    Object.keys(channel.objects).forEach(function(name)
    {
        wrapObject(name, channel.objects[name]);
    });
});
