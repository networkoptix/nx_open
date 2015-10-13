'use strict';

angular.module('cloudApp')
    .factory('cloudApi', function ($http, $q) {

        var apiBase = Config.apiBase;
        return {
            account:function(firstName,lastName,subscribe){
                if(!data) {
                    return $http.get(apiBase + '/account');
                }
                return $http.post(apiBase + '/account',{
                    firstName:firstName,
                    lastName:lastName,
                    subscribe:subscribe
                });
            },
            login:function(email,password){
                return $http.post(apiBase + '/account/login',{
                    email:email,
                    password:password
                });
            },
            logout:function(){
                return $http.post(apiBase + '/account/logout');
            },
            register:function(email,password,firstName,lastName,subscribe){
                return $http.post(apiBase + '/account/register',{
                    email:email,
                    password:password,
                    firstName:firstName,
                    lastName:lastName,
                    subscribe:subscribe
                });
            },
            activate:function(){
                throw "not implemented";
                return $http.get(apiBase + '/account/activate');
            },
            restorePassword:function(){
                throw "not implemented";
                return $http.get(apiBase + '/account/restorePassword');
            },
            changePassword:function(){
                throw "not implemented";
                return $http.get(apiBase + '/account/changePassword');
            }
        }

    });