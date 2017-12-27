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
                var self = this;
                if(requestingLogin){
                    // login is requesting, so we wait
                    return requestingLogin.then(function(){
                        requestingLogin = null; // clean requestingLogin reference
                        return self.get(); // Try again
                    });
                }
                return cloudApi.account().then(function(account){
                    return account.data;
                });
            },
            authKey:function(){
                return cloudApi.authKey().then(function(result){
                    return result.data.auth_key;
                });
            },
            checkVisitedKey:function(key){
                return cloudApi.visitedKey(key).then(function(result){
                    return result.data.visited;
                });
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
                    if(cloudApi.checkResponseHasError(result)){
                        return $q.reject(result);
                    }

                    if(result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                        self.setEmail(result.data.email);
                        $rootScope.session.loginState = result.data.email; //Forcing changing loginState to reload interface
                    }
                    return result;
                });
            },
            logout:function(doNotRedirect){
                cloudApi.logout().finally(function(){
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
                    // logoutAuthorisedLogoutButton
                    dialogs.confirm(null /*L.dialogs.logoutAuthorisedText*/,
                        L.dialogs.logoutAuthorisedTitle,
                        L.dialogs.logoutAuthorisedContinueButton,
                        null,
                        L.dialogs.logoutAuthorisedLogoutButton
                        ).then(function(){
                        self.redirectAuthorised();
                    },function(){
                        self.logout(true);
                    });
                });
            },
            checkUnauthorized:function(data){
                if(data && data.data && data.data.resultCode == 'notAuthorized'){
                    this.logout();
                    return false;
                }
                return true;
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
                var requestingLogin = service.login(tempLogin, tempPassword, false).then(function(){
                    $location.search('auth', null);
                },function(){
                    $location.search('auth', null);
                });
            }
        }

        return service;
    }]);
