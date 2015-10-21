'use strict';

angular.module('cloudApp')
    .factory('cloudApi', function ($http, $q) {

        var apiBase = Config.apiBase;
        var accountCache = null;
        return {
            account:function(){
                var defer = $q.defer();
                if(!accountCache){
                    $http.get(apiBase + '/account').then(function(response){
                        accountCache = response.data;
                        console.log("account",accountCache);
                        defer.resolve(accountCache);
                    },function(error){
                        console.error(error);

                        accountCache = null;
                        defer.resolve(null);
                        return null
                    })
                }else{
                    defer.resolve(accountCache);
                }
                return defer.promise;
            },

            accountPost:function(firstName,lastName,subscribe){
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