'use strict';

angular.module('cloudApp')
    .factory('account', ['cloudApi', 'dialogs', '$q', '$location', '$localStorage', '$rootScope', function (cloudApi, dialogs, $q, $location, $localStorage, $rootScope) {
        $rootScope.session = $localStorage;

        var initialState = $rootScope.session.loginState;
        $rootScope.$watch('session.loginState',function(value){  // Catch logout from other tabs
            if(initialState !== value){
                window.location.reload();
            }
        });

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
            redirectToHome:function(){
                this.get().then(function(){
                    $location.path(Config.redirectAuthorised);
                },function(){
                    $location.path(Config.redirectUnauthorised);
                })
            },

            setEmail:function(email){
                $rootScope.session.email = email;
            },
            getEmail:function(){
                return $rootScope.session.email;
            },

            setPassword:function(password){
                $rootScope.session.password = password; // TODO: This is dirty security hole, but I need this to make "Open in client" work
            },
            getPassword:function(){
                return $rootScope.session.password;
            },

            login:function(email, password, remember){
                this.setEmail(email);
                this.setPassword(password);
                var promise = cloudApi.login(email, password, remember);
                promise.then(function(){
                    $rootScope.session.loginState = true;
                });
                return promise;
            },
            logout:function(doNotRedirect){
                cloudApi.logout().then(function(){
                    $rootScope.session.$reset(); // Clear session
                    if(!doNotRedirect) {
                        $location.path(Config.redirectUnauthorised);
                    }
                    setTimeout(function(){
                        document.location.reload();
                    });
                });
            },
            logoutAuthorised:function(){
                var self = this;
                this.get().then(function(){
                    self.logout(true);
                });
            }
        }
    }]);