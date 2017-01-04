'use strict';

angular.module('cloudApp')
    .factory('cloudApi', ['$http', '$q', '$localStorage', function ($http, $q, $localStorage) {

        var apiBase = Config.apiBase;

        var cachedResults = {};
        var cacheReceived = {};
        function cacheGet(url, cacheForever){
            cacheReceived[url] = 0;
            return function(command) {

                var fromCache = command == 'fromCache';
                var forceReload = command == 'reload';
                var clearCache = command == 'clearCache';

                var defer = $q.defer();
                var now = (new Date()).getTime();

                //Check for cache availability
                var haveValueInCache = typeof(cachedResults[url]) !== 'undefined';
                var cacheMiss = !haveValueInCache || // clear cache miss
                    forceReload || // ignore cache
                    !cacheForever && ((now - cacheReceived[url]) > Config.cacheTimeout); // outdated cache

                if(clearCache){
                    delete(cachedResults[url]);
                    cacheReceived[url] = 0;
                    defer.resolve(null);
                }else if(fromCache || !cacheMiss){
                    if(haveValueInCache) {
                        defer.resolve(cachedResults[url]);
                    }else{
                        defer.reject(cachedResults[url]);
                    }
                }else {
                    $http.get(url).then(function (response) {
                        cachedResults[url] = response;
                        cacheReceived[url] = now;
                        defer.resolve(cachedResults[url]);
                    }, function (error) {
                        delete(cachedResults[url]);
                        cacheReceived[url] = 0;
                        defer.reject(error);
                    });
                }

                return defer.promise;
            }
        }

        function clearCache(){
            cachedResults = {};
            cacheReceived = {};
        }

        var getSystems = cacheGet(apiBase + '/systems');
        var modules = cacheGet(apiBase + '/modules');
        modules().then(function(result){
            Config.gatewayUrl = window.location.protocol + '//' + result.data.vms_gateway_host +':' + result.data.vms_gateway_port;
        });

        return {
            modules:modules,
            checkResponseHasError:function(data){
                if(data && data.data && data.data.resultCode && data.data.resultCode != L.errorCodes.ok){
                    return data;
                }
                return false;
            },
            account: cacheGet(apiBase + '/account'),
            login:function(email, password, remember){
                clearCache();
                return $http.post(apiBase + '/account/login',{
                    email: email,
                    password: password,
                    remember: remember
                });
            },
            logout:function(){
                clearCache();
                return $http.post(apiBase + '/account/logout');
            },
            register:function(email, password, firstName, lastName, subscribe, code){
                return $http.post(apiBase + '/account/register',{
                    email: email,
                    password: password,
                    first_name: firstName,
                    last_name: lastName,
                    subscribe: subscribe,
                    code: code
                });
            },

            notification_send:function(userEmail,type,message){
                return $http.post(apiBase.replace('/api','/notifications') + '/send',{
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
            authKey:function(){
                return $http.post(apiBase + '/account/authKey');
            },
            systems: getSystems,

            system: function(systemId){
                function requestSystem(){
                    return $http.get(apiBase + '/systems/' + systemId);
                }

                return getSystems().then(function(systemsCache){
                    //Search our system in cache
                    var system = _.find(systemsCache.data,function(system){
                        return system.id == systemId;
                    });

                    if(system) { // Cache success
                        return {data:[system]};
                    }else{ // Cache miss
                        return requestSystem();
                    }
                },requestSystem); // Total cache miss
            },
            renameSystem:function(systemId,systemName){
                var self = this;
                return $http.post(apiBase + '/systems/' + systemId + '/name',{
                    name:systemName
                }).then(function(result){
                    self.systems('clearCache');
                    return result;
                });
            },
            getSystemAuth:function(systemId){
                return $http.get(apiBase + '/systems/' + systemId + '/auth');
            },

            getCommonPasswords:cacheGet('/static/scripts/commonPasswordsList.json',true),
            users:function(systemId){
                return $http.get(apiBase + '/systems/' + systemId + '/users');
            },

            // TODO Remove this method in 3.0.0
            share:function(systemId, userEmail, role){
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role: role
                });
            },
            unshare:function(systemId,userEmail){
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role: Config.accessRoles.unshare
                });
            },

            disconnect:function(systemId, password){
                return $http.post(apiBase + '/systems/disconnect', {
                    system_id: systemId,
                    password: password
                });
            },
            connect:function(name){
                return $http.post(apiBase + '/systems/connect', {
                    name: name
                });
            },
            accessRoles: function(systemId){
                console.error('This method must not be used');
                return $http.get(apiBase + '/systems/' + systemId + '/accessRoles');
            },

            visitedKey:function(key){
                return $http.get(apiBase + '/utils/visitedKey/?key=' + encodeURIComponent(key));
            }
        }

    }]);