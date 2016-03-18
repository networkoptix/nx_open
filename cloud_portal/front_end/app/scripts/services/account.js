'use strict';

angular.module('cloudApp')
    .factory('account', function (cloudApi, dialogs, $q, $location) {
        function get(){
            var defer = $q.defer();
            cloudApi.account().then(function(account){
                defer.resolve(account.data);
            },function(no_account){
                defer.reject(null);
            });
            return defer.promise;
        }
        return {
            get:get,
            requireLogin:function(){
                var res = get();
                res.catch(function(){
                    dialogs.login(true).catch(function(){
                        $location.path(Config.redirectUnauthorised);
                    });
                });
                return res;
            },
            redirectAuthorised:function(){
                get().then(function(){
                    $location.path(Config.redirectAuthorised);
                });
            },
            logoutAuthorised:function(){
                get().then(function(){
                    cloudApi.logout().then(function(){
                        scope.session.password = ''; // TODO: get rid of password in session storage
                        window.location.reload();
                    });
                });
            }
        }
    });