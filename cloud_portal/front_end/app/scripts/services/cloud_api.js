'use strict';

angular.module('cloudApp')
    .factory('cloudApi', function ($http, $q) {

        var apiBase = Config.apiBase;
        var accountCache = null;
        return {
            account:function(){
                var defer = $q.defer();
                if(!accountCache){

                    console.log("account request");
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
                    first_name:firstName,
                    last_name:lastName,
                    subscribe:subscribe
                });
            },

            notification_send:function(user_email,type,message){
                return $http.post(apiBase.replace("/api","/notifications") + '/send',{
                    user_email:user_email,
                    type:type,
                    message:message
                });
            },

            accountPost:function(firstName,lastName,subscribe){
                return $http.post(apiBase + '/account',{
                    first_name:firstName,
                    last_name:lastName,
                    subscribe:subscribe
                });
            },
            changePassword:function(newPassword,oldPassword){
                return $http.get(apiBase + '/account/changePassword',{
                    new_password:newPassword,
                    old_password:oldPassword
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


            systems:function(){
                return $http.get(apiBase + '/systems');
            }
        }

    });