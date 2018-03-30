(function () {
    'use strict';
    angular
        .module('cloudApp')
        .factory('account', AccountService);
    AccountService.$inject = ['cloudApi', '$q', '$location', '$localStorage',
        '$rootScope', '$base64', 'configService'];
    function AccountService(cloudApi, $q, $location, $localStorage, $rootScope, $base64, configService) {
        $rootScope.session = $localStorage;
        let requestingLogin;
        let initialState = $rootScope.session.loginState;
        const CONFIG = configService.config;
        $rootScope.$watch('session.loginState', function (value) {
            if (initialState !== value) {
                document.location.reload();
            }
        });
        let service = {
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
            setEmail: function (email) {
                $rootScope.session.email = email;
            },
            getEmail: function () {
                return $rootScope.session.email;
            },
            login: function (email, password, remember) {
                this.setEmail(email);
                let self = this;
                return cloudApi.login(email, password, remember).then(function (result) {
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
        let auth;
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
        return service;
    }
})();
//# sourceMappingURL=account.js.map