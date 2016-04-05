'use strict';

angular.module('cloudApp')
    .factory('nativeClient', ['$base64','$localStorage',function ($base64,$localStorage) {
        return {
            open:function(systemId){

                var protocol = Config.clientProtocol;

                var system   = (systemId + '/') || 'open/';

                var params = {};

                var storage = $localStorage;
                var username = storage.email;
                var password = storage.password;
                if (username && password){
                    params.auth = $base64.encode(username + ':' + password);
                }

                var url = protocol + system + '?' + $.param(params);
                window.location.href = url;
                //window.open(url);
            }
        }
    }]);