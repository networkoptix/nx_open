(function () {
    'use strict';
    angular
        .module('cloudApp')
        .factory('authorizationCheckService', AuthorizationCheckService);
    AuthorizationCheckService.$inject = ['$rootScope', '$q', '$localStorage', '$base64', 'cloudApi', 'CONFIG', 'nxDialogsService', 'languageService', '$location'];
    function AuthorizationCheckService($rootScope, $q, $localStorage, $base64, cloudApi, CONFIG, nxDialogsService, languageService, $location) {
        let search = $location.search();
        let auth;
        let requestingLogin;
        $rootScope.session = $localStorage;
        let service = {
            get: get,
            login: login,
            requireLogin: requireLogin,
            redirectAuthorised: redirectAuthorised,
            redirectToHome: redirectToHome,
            logoutAuthorised: logoutAuthorised,
        };
        init();
        return service;
        //////////////////////////////////////////////////
        function init() {
            if (search.auth) {
                try {
                    auth = $base64.decode(search.auth);
                }
                catch (exception) {
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
        }
        function get() {
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
        }
        function login(email, password, remember) {
            this.setEmail(email);
            let self = this;
            return cloudApi.login(email, password, remember).then(function (result) {
                if (cloudApi.checkResponseHasError(result)) {
                    return $q.reject(result);
                }
                if (result.data.email) {
                    self.setEmail(result.data.email);
                    $rootScope.session.loginState = result.data.email; //Forcing changing loginState to reload interface
                }
                return result;
            });
        }
        function requireLogin() {
            let res = this.get();
            res.catch(function () {
                nxDialogsService.login(true).catch(function () {
                    $location.path(CONFIG.redirectUnauthorised);
                });
            });
            return res;
        }
        function redirectAuthorised() {
            this.get().then(function () {
                $location.path(CONFIG.redirectAuthorised);
            });
        }
        function redirectToHome() {
            this.get().then(function () {
                $location.path(CONFIG.redirectAuthorised);
            }, function () {
                $location.path(CONFIG.redirectUnauthorised);
            });
        }
        function logoutAuthorised() {
            let self = this;
            this.get().then(function () {
                // logoutAuthorisedLogoutButton
                nxDialogsService.confirm(null /*L.dialogs.logoutAuthorisedText*/, languageService.lang.dialogs.logoutAuthorisedTitle, languageService.lang.dialogs.logoutAuthorisedContinueButton, null, languageService.lang.dialogs.logoutAuthorisedLogoutButton).then(function () {
                    self.redirectAuthorised();
                }, function () {
                    self.logout(true);
                });
            });
        }
    }
})();
//# sourceMappingURL=authorizationCheckService.js.map