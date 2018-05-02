(function () {
    'use strict';
    angular
        .module('cloudApp')
        .factory('authorizationCheckService', AuthorizationCheckService);
    AuthorizationCheckService.$inject = ['$rootScope', '$q', '$localStorage', '$base64',
        'cloudApi', 'configService', 'nxDialogsService', 'languageService', '$location'];
    function AuthorizationCheckService($rootScope, $q, $localStorage, $base64, cloudApi, configService, nxDialogsService, languageService, $location) {
        const CONFIG = configService.config;
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
                    requestingLogin = service.login(tempLogin, tempPassword, false)
                        .then(() => {
                        $location.search('auth', null);
                    }, () => {
                        $location.search('auth', null);
                    });
                }
            }
        }
        function get() {
            if (requestingLogin) {
                // login is requesting, so we wait
                return requestingLogin.then(() => {
                    requestingLogin = null; // clean requestingLogin reference
                    return get(); // Try again
                });
            }
            return cloudApi
                .account()
                .then(function (account) {
                return account.data;
            });
        }
        function login(email, password, remember) {
            this.setEmail(email);
            return cloudApi
                .login(email, password, remember)
                .then((result) => {
                if (cloudApi.checkResponseHasError(result)) {
                    return $q.reject(result);
                }
                if (result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                    this.setEmail(result.data.email);
                    $rootScope.session.loginState = result.data.email; //Forcing changing loginState to reload interface
                }
                return result;
            });
        }
        function requireLogin() {
            return this.get()
                .catch(() => {
                nxDialogsService.login(true);
                $location.path(CONFIG.redirectUnauthorised);
            });
        }
        function redirectAuthorised() {
            this.get()
                .then(() => {
                $location.path(CONFIG.redirectAuthorised);
            });
        }
        function redirectToHome() {
            this.get()
                .then(() => {
                $location.path(CONFIG.redirectAuthorised);
            }, () => {
                $location.path(CONFIG.redirectUnauthorised);
            });
        }
        function logoutAuthorised() {
            this.get()
                .then(() => {
                // logoutAuthorisedLogoutButton
                nxDialogsService.confirm(null /*L.dialogs.logoutAuthorisedText*/, languageService.lang.dialogs.logoutAuthorisedTitle, languageService.lang.dialogs.logoutAuthorisedContinueButton, null, languageService.lang.dialogs.logoutAuthorisedLogoutButton).then(() => {
                    redirectAuthorised();
                }, () => {
                    this.logout(true);
                });
            });
        }
    }
})();
//# sourceMappingURL=authorizationCheckService.js.map