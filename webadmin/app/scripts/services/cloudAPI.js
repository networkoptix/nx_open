'use strict';

angular.module('webadminApp')
    .factory('cloudAPI', ['$http', '$q', function ($http, $q) {
        return {
            login:function(email, password){
                return $http.post(Config.cloud.portalUrl + Config.cloud.apiUrl + '/account/login',{
                    email: email,
                    password: password
                });
            },
            connect:function(systemName, email, password){
                return $http.post(Config.cloud.portalUrl + Config.cloud.apiUrl + '/systems/connect',{
                    name: systemName,
                    email: email,
                    password: password
                });
            },
            disconnect:function(system_id, email, password){
                return $http.post(Config.cloud.portalUrl + Config.cloud.apiUrl + '/systems/disconnect',{
                    system_id: system_id,
                    email: email,
                    password: password
                });
            },
            ping:function(){
                return $http.get(Config.cloud.portalUrl + Config.cloud.apiUrl + '/ping');
            },
            checkConnection:function(){
                var deferred = $q.defer();
                this.ping().then(function(message){
                    if(message.data.resultCode && message.data.resultCode !== 'ok'){
                        deferred.reject(message);
                        return;
                    }
                    deferred.resolve(true);
                },function(error){
                    deferred.reject(error);
                });

                return deferred.promise;
            }
        };
    }]);
