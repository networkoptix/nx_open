'use strict';

angular.module('webadminApp')
    .factory('cloudAPI', function ($http) {
        return {
            login:function(email, password){
                return $http.post(Config.cloud.apiUrl + '/account/login',{
                    email: email,
                    password: password
                });
            },
            connect:function(systemName, email, password){
                return $http.post(Config.cloud.apiUrl + '/systems/connect',{
                    name: systemName,
                    email: email,
                    password: password
                });
            },
            disconnect:function(system_id, email, password){
                return $http.post(Config.cloud.apiUrl + '/systems/disconnect',{
                    system_id: system_id,
                    email: email,
                    password: password
                });
            },
            ping:function(){
                return $http.get(Config.cloud.apiUrl + '/ping');
            }
        };
    });
