'use strict';

angular.module('cloudApp')
    .factory('account', function (cloudApi, dialogs, $q, $location, $sessionStorage) {
        return {
            get:function(){
                var defer = $q.defer();
                cloudApi.account().then(function(account){
                    defer.resolve(account.data);
                },function(no_account){
                    defer.reject(null);
                });
                return defer.promise;
            },
            requireLogin:function(){
                var res = this.get();
                res.catch(function(){
                    dialogs.login(true).catch(function(){
                        $location.path(Config.redirectUnauthorised);
                    });
                });
                return res;
            },
            redirectAuthorised:function(){
                this.get().then(function(){
                    $location.path(Config.redirectAuthorised);
                });
            },
            logout:function(){
                cloudApi.logout().then(function(){
                    $sessionStorage.$reset(); // Clear session
                    window.location.reload();
                });
            },
            logoutAuthorised:function(){
                var self = this;
                this.get().then(function(){
                    console.log("logoutAuthorised");
                    self.logout();
                });
            }
        }
    });