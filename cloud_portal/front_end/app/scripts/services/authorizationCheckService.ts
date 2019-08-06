(function () {

        'use strict';

        angular
            .module('cloudApp')
            .factory('authorizationCheckService', AuthorizationCheckService);

        AuthorizationCheckService.$inject = ['$rootScope', '$q', '$localStorage', '$base64',
            'cloudApi', 'account', 'nxConfigService', 'NxDialogsService', 'languageService', '$location'];

        function AuthorizationCheckService($rootScope, $q, $localStorage, $base64,
                                           cloudApi, account, nxConfigService, NxDialogsService, languageService, $location) {

            const CONFIG = nxConfigService.getConfig();

            const search = $location.search();
            let auth: any;

            $rootScope.session = $localStorage;

            const service = {
                get,
                login,
                logout,
                requireLogin,
                redirectAuthorised,
                redirectToHome,
                logoutAuthorised,
            };

            init();

            return service;

            //////////////////////////////////////////////////

            function init() {
                if (search.auth) {
                    try {
                        auth = $base64.decode(search.auth);
                    } catch (exception) {
                        auth = false;
                        console.error(exception);
                    }
                    if (auth) {
                        const index = auth.indexOf(':');
                        const tempLogin = auth.substring(0, index);
                        const tempPassword = auth.substring(index + 1);

                        account.requestingLogin = login(tempLogin, tempPassword, false)
                            .then(() => {
                                $location.search('auth', undefined);
                            }, () => {
                                $location.search('auth', undefined);
                            });
                    }
                }
            }

            function get() {
                if (account.requestingLogin) {
                    // login is requesting, so we wait
                    return account.requestingLogin.then(() => {
                        account.requestingLogin = undefined; // clean requestingLogin reference
                        return get(); // Try again
                    });
                }

                return cloudApi
                    .account()
                    .then((account) => {
                        return account.data;
                    });
            }

            function login(email, password, remember) {
                return cloudApi
                        .login(email, password, remember)
                        .then((result) => {
                            if (cloudApi.checkResponseHasError(result)) {
                                return $q.reject(result);
                            }

                            return checkLoginState()
                                    .then(() => {
                                        logoutAuthorised();
                                    })
                                    .catch(() => {
                                        if (result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                                            setEmail(result.data.email);
                                            $rootScope.session.loginState = result.data.email; // Forcing changing loginState to reload interface
                                        }
                                    });
                        });
            }

            function checkLoginState() {
                if ($rootScope.session.loginState) {
                    return $q.resolve(true);
                }
                return $q.reject(false);
            }

            function logout(doNotRedirect) {
                cloudApi
                    .logout()
                    .finally(() => {
                        $rootScope.session.$reset(); // Clear session
                        if (!doNotRedirect) {
                            $location.path(CONFIG.redirectUnauthorised);
                        }
                        setTimeout(() => {
                            document.location.reload();
                        });
                    });
            }

            function setEmail(email) {
                $rootScope.session.email = email;
            }

            function requireLogin() {
                return get().catch(() => {
                    return NxDialogsService.login(true);
                });
            }

            function redirectAuthorised() {
                get().then(() => {
                    $location.path(CONFIG.redirectAuthorised);
                });
            }

            function redirectToHome() {
                get().then(() => {
                    $location.path(CONFIG.redirectAuthorised);
                }, () => {
                    $location.path(CONFIG.redirectUnauthorised);
                });
            }

            function logoutAuthorised() {
                get().then(() => {
                    // logoutAuthorisedLogoutButton
                    NxDialogsService
                        .confirm(languageService.lang.dialogs.logoutAuthorisedText, languageService.lang.dialogs.logoutAuthorisedTitle,
                            languageService.lang.dialogs.logoutAuthorisedContinueButton, 'btn-primary',
                            languageService.lang.dialogs.logoutAuthorisedLogoutButton
                        )
                        .then((result) => {
                            if (result) {
                                redirectAuthorised();
                            } else {
                                logout(true);
                            }
                        });
                });
            }

        }
    }

)();
