(function () {

    'use strict';

    angular
        .module('cloudApp')
        .factory('account', AccountService);

    AccountService.$inject = ['cloudApi', '$q', '$location', '$localStorage', '$routeParams',
        '$rootScope', '$base64', 'nxConfigService', 'dialogs', 'languageService'];

    function AccountService(cloudApi, $q, $location, $localStorage, $routeParams,
                            $rootScope, $base64, nxConfigService, dialogs, languageService) {

        $rootScope.session = $localStorage;

        let requestingLogin: any;
        let initialState = $rootScope.session.loginState;
        const CONFIG = nxConfigService.getConfig();
        const lang = languageService.lang;

        $rootScope.$watch('session.loginState', function (value) {  // Catch logout from other tabs
            if (!$routeParams.next && initialState !== value) {
                document.location.reload();
            }
        });

        let service = {
            checkLoginState: function () {
                if ($rootScope.session.loginState) {
                    return $q.resolve(true);
                }
                return $q.reject(false);
            },
            get: function () {
                let self = this;
                if (requestingLogin) {
                    // login is requesting, so we wait
                    return requestingLogin.then(function () {
                        requestingLogin = null; // clean requestingLogin reference
                        return self.get(); // Try again
                    });
                }
                return cloudApi.account().then(function (account) {
                    return account.data;
                });
            },
            authKey: function () {
                return cloudApi.authKey().then(function (result) {
                    return result.data.auth_key;
                });
            },
            checkVisitedKey: function (key) {
                return cloudApi.visitedKey(key).then(function (result) {
                    return result.data.visited;
                });
            },
            checkCode: function (code) {
                return cloudApi.checkCode(code).then(function (result) {
                    return result.data.emailExists;
                });
            },
            requireLogin: function () {
                var res = this.get();
                res.catch(function () {
                    dialogs.login(true).catch(function () {
                        $location.path(CONFIG.redirectUnauthorised);
                    });
                });
                return res;
            },
            redirectAuthorised: function () {
                this.get().then(function () {
                    $location.path(CONFIG.redirectAuthorised);
                });
            },
            redirectToHome: function () {
                this.get().then(function () {
                    $location.path(CONFIG.redirectAuthorised);
                }, function () {
                    $location.path(CONFIG.redirectUnauthorised);
                })
            },

            setEmail: function (email) {
                $rootScope.session.email = email;
            },
            getEmail: function () {
                return $rootScope.session.email;
            },
            login: function (email, password, remember) {
                this.setEmail(email);
                let self = this;

                return cloudApi.login(email, password, remember)
                               .then((result) => {
                                   if (cloudApi.checkResponseHasError(result)) {
                                       return $q.reject(result);
                                   }

                                   if (result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                                       self.setEmail(result.data.email);
                                       $rootScope.session.loginState = result.data.email; //Forcing changing loginState to reload interface
                                   }
                                   return result;
                               });
            },
            logout: function (doNotRedirect) {
                cloudApi.logout().finally(function () {
                    $rootScope.session.$reset(); // Clear session
                    if (!doNotRedirect) {
                        $location.path(CONFIG.redirectUnauthorised);
                    }
                    setTimeout(function () {
                        document.location.reload();
                    });
                });
            },
            logoutAuthorised: function () {
                let self = this;
                this.get().then(function () {
                    // logoutAuthorisedLogoutButton
                    dialogs.confirm(null /*L.dialogs.logoutAuthorisedText*/,
                        lang.dialogs.logoutAuthorisedTitle,
                        lang.dialogs.logoutAuthorisedContinueButton,
                        null,
                        lang.dialogs.logoutAuthorisedLogoutButton
                    ).then(function () {
                        self.redirectAuthorised();
                    }, function () {
                        self.logout(true);
                    });
                });
            },
            checkUnauthorized: function (data) {
                if (data && data.data && data.data.resultCode == 'notAuthorized') {
                    this.logout();
                    return false;
                }
                return true;
            }
        };

        // Check auth parameter in url
        let search = $location.search();
        let auth: any;

        if (search.auth) {
            try {
                auth = $base64.decode(search.auth);
            } catch (exception) {
                auth = false;
                console.error(exception);
            }
            if (auth) {
                let index = auth.indexOf(':');
                let tempLogin = auth.substring(0, index);
                let tempPassword = auth.substring(index + 1);

                requestingLogin = service.login(tempLogin, tempPassword, false).then(function () {
                    $location.search('auth', null);
                }, function () {
                    $location.search('auth', null);
                });
            }
        }

        return service;
    }
})();
