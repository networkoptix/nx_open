'use strict';

angular.module('cloudApp')
    .factory('cloudApi', function ($http, $q) {

        var apiBase = Config.apiBase;

        function cacheGet(url, cacheForever){
            var cachedResult = null;
            var cacheReceived = 0;
            return function(command) {

                var fromCache = command == 'fromCache';
                var forceReload = command == 'reload';
                var clearCache = command == 'clearCache';

                var defer = $q.defer();
                var now = (new Date()).getTime();

                //Check for cache availability
                var cacheMiss = cachedResult === null || // clear cache miss
                    forceReload || // ignore cache
                    !cacheForever && (now - cacheReceived) > Config.cacheTimeout; // outdated cache


                if(clearCache){
                    cachedResult = null;
                    cacheReceived = 0;
                    defer.resolve(null);
                }else  if(fromCache || !cacheMiss){
                    if(cachedResult !== null) {
                        defer.resolve(cachedResult);
                    }else{
                        defer.reject(cachedResult);
                    }
                }else {
                    $http.get(url).then(function (response) {
                        cachedResult = response;
                        cacheReceived = now;
                        defer.resolve(cachedResult);
                    }, function (error) {
                        cachedResult = null;
                        cacheReceived = 0;
                        defer.reject(error);
                    });
                }

                return defer.promise;
            }
        }

        var getSystems = cacheGet(apiBase + '/systems');

        return {
            account: cacheGet(apiBase + '/account'),
            login:function(email, password, remember){
                return $http.post(apiBase + '/account/login',{
                    email: email,
                    password: password,
                    remember: remember
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

            notification_send:function(userEmail,type,message){
                return $http.post(apiBase.replace("/api","/notifications") + '/send',{
                    user_email:userEmail,
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
                return $http.post(apiBase + '/account/changePassword',{
                    new_password:newPassword,
                    old_password:oldPassword
                });
            },
            restorePasswordRequest:function(userEmail){
                return $http.post(apiBase + '/account/restorePassword',{
                    user_email:userEmail
                });
            },
            restorePassword:function(code, newPassword){
                return $http.post(apiBase + '/account/restorePassword',{
                    code:code,
                    new_password:newPassword
                });
            },
            reactivate:function(userEmail){
                return $http.post(apiBase + '/account/activate',{
                    user_email:userEmail
                });
            },
            activate:function(code){
                return $http.post(apiBase + '/account/activate',{
                    code:code
                });
            },
            systems: getSystems,

            system: function(systemId){


                var defer = $q.defer();

                function requestSystem(){
                    $http.get(apiBase + '/systems/' + systemId).then(function(success){
                        defer.resolve(success);
                    },function(error){
                        defer.reject(error);
                    });
                }

                getSystems('fromCache').then(function(systemsCache){
                    //Search our system in cache
                    var system = _.find(systemsCache.data,function(system){
                        return system.id == systemId;
                    });

                    if(system) { // Cache success
                        defer.resolve({data:[system]});
                    }else{ // Cache miss
                        requestSystem();
                    }
                },requestSystem); // Total cache miss

                return defer.promise;
            },

            getCommonPasswords:cacheGet('/static/scripts/commonPasswordsList.json',true),
            users:function(systemId){
                return $http.get(apiBase + '/systems/' + systemId + '/users');
            },
            share:function(systemId, userEmail, role){
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role:role
                });
            },
            unshare:function(systemId,userEmail){
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role: Config.accessRoles.unshare
                });
            },
            disconnect:function(systemId){
                return $http.post(apiBase + '/systems/delete', {
                    system_id: systemId
                });
            },
            connect:function(name){
                return $http.post(apiBase + '/systems/connect', {
                    name: name
                });
            },
            accessRoles: function(systemId){
                return $http.get(apiBase + '/systems/' + systemId + '/accessRoles');
            }
        }

    });