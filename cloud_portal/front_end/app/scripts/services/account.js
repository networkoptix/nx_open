'use strict';

angular.module('cloudApp')
    .factory('account', ['cloudApi', 'dialogs', '$q', '$location', '$localStorage', '$rootScope','$base64',
    function (cloudApi, dialogs, $q, $location, $localStorage, $rootScope,$base64) {
        $rootScope.session = $localStorage;

        var initialState = $rootScope.session.loginState;
        $rootScope.$watch('session.loginState',function(value){  // Catch logout from other tabs
            if(initialState !== value){
                document.location.reload();
            }
        });

        var service = {
            get:function(){
                var defer = $q.defer();
                cloudApi.account().then(function(account){
                    defer.resolve(account.data);
                },function(no_account){
                    defer.reject(null);
                });
                return defer.promise;
            },
            authKey:function(){
                var defer = $q.defer();
                cloudApi.authKey().then(function(result){
                    defer.resolve(result.data.auth_key);
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

            login:function (email, password, remember){
                this.setEmail(email);
                var self = this;
                return cloudApi.login(email, password, remember).then(function(result){
                    if(result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                        self.setEmail(result.data.email);
                        $rootScope.session.loginState = result.data.email; //Forcing changing loginState to reload interface
                    }
                });
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
                    dialogs.confirm(L.dialogs.logoutConfirmText,
                        L.dialogs.logoutConfirmTitle,
                        L.dialogs.logoutConfirmButton, "danger").then(function(){
                        self.logout(true);
                    },function(){
                        self.redirectAuthorised();
                    });
                });
            }
        }

        // Check auth parameter in url
        var search = $location.search();
        if(search.auth){
            try{
                var auth = $base64.decode(search.auth);
            }catch(exception){
                auth = false;
                console.error(exception);
            }
            if(auth){
                var index = auth.indexOf(':');

                var tempLogin = auth.substring(0,index);
                var tempPassword = auth.substring(index+1);

                service.login(tempLogin, tempPassword, false).then(function(){
                    $location.search('auth', null);
                });
            }
        }

        return service;
    }]);
