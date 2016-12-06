angular.module('webadminApp')
    .config(['$provide', function($provide) {
        $provide.decorator('$log', ['$delegate', 'Logging', function($delegate, Logging) {
            Logging.enabled = Config.visualLog;
            if(Logging.enabled){
                console.log("inititate visual log");
                $('body').prepend('<pre id="debug"></pre>');
            }
            var methods = {
                error: function() {
                    if (Logging.enabled) {
                        $delegate.error.apply($delegate, arguments);
                        Logging.error.apply(null, arguments);
                    }
                },
                log: function() {
                    if (Logging.enabled) {
                        $delegate.log.apply($delegate, arguments);
                        Logging.log.apply(null, arguments);
                    }
                },
                info: function() {
                    if (Logging.enabled) {
                        $delegate.info.apply($delegate, arguments);
                        Logging.info.apply(null, arguments);
                    }
                },
                warn: function() {
                    if (Logging.enabled) {
                        $delegate.warn.apply($delegate, arguments);
                        Logging.warn.apply(null, arguments);
                    }
                }
            };
            return methods;
        }]);
    }])
    .service('Logging', function() {
        var service = {
            error: function() {
                self.type = 'error';
                log.apply(self, arguments);
            },
            warn: function() {
                self.type = 'warn';
                log.apply(self, arguments);
            },
            info: function() {
                self.type = 'info';
                log.apply(self, arguments);
            },
            log: function() {
                self.type = 'log';
                log.apply(self, arguments);
            },
            enabled: false,
            logs: []
        };

        var log = function() {
            var data = '';
            for(var key in arguments){
                data += JSON.stringify(arguments[key],null,2) + '<br>';
            }
            var message = '<span>' + this.type + ': ' + data + '</span>';
            $("#debug").append(message);
        };

        return service;
    });