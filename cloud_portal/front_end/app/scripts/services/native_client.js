'use strict';

angular.module('cloudApp')
    .factory('nativeClient', ['$base64','$localStorage',function ($base64,$localStorage) {
        return {
            open:function(systemId){

                var storage = $localStorage;
                var username = encodeURIComponent(storage.email);
                var password = encodeURIComponent(storage.password);
                var system   = systemId?systemId:'localhost:7001';
                var protocol = Config.clientProtocol;
                system = system.replace('{','').replace('}','');
                var url = protocol + system + '/?auth=' + $base64.encode(username + ':' + password);
                window.open(url);
            }
        }
    }]);