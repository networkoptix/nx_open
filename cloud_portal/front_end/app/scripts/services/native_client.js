'use strict';

angular.module('cloudApp')
    .factory('nativeClient', ['$localStorage',function ($localStorage) {
        return {
            open:function(systemId){

                var storage = $localStorage;
                var username = encodeURIComponent(storage.email);
                var password = encodeURIComponent(storage.password);
                var system   = systemId?systemId:'localhost:7001';
                var protocol = Config.clientProtocol;
                system = system.replace('{','').replace('}','');
                var url = protocol + username + ':' + password + '@' + system + '/';

                window.open(url);
            }
        }
    }]);