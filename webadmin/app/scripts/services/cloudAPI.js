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
            }
        };
    });
