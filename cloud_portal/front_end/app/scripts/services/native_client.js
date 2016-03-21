'use strict';

angular.module('cloudApp')
    .factory('nativeClient', function () {
        return {
            open:function(systemId){

                var username = encodeURIComponent($sessionStorage.email);
                var password = encodeURIComponent($sessionStorage.password);
                var system   = systemId?systemId:'localhost:7000';
                var protocol = Config.clientProtocol;
                system = system.replace('{','').replace('}','');
                var url = protocol + username + ':' + password + '@' + system + '/';
                window.open(url);
            }
        }
    });