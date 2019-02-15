(function () {

        'use strict';

        angular
            .module('cloudApp')
            .factory('authorizationCheckService', AuthorizationCheckService);

        AuthorizationCheckService.$inject = ['$rootScope', '$q', '$localStorage', '$base64',
            'cloudApi', 'nxConfigService', 'NxDialogsService', 'languageService', '$location'];

        function AuthorizationCheckService($rootScope, $q, $localStorage, $base64,
                                           cloudApi, nxConfigService, NxDialogsService, languageService, $location) {

            const CONFIG = nxConfigService.getConfig();

            let search = $location.search();
            let auth: any;
            let requestingLogin: any;

            $rootScope.session = $localStorage;

            let service = {
                get: get,
                login: login,
                logout: logout,
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
                    } catch (exception) {
                        auth = false;
                        console.error(exception);
                    }
                    if (auth) {
                        let index = auth.indexOf(':');
                        let tempLogin = auth.substring(0, index);
                        let tempPassword = auth.substring(index + 1);

                        requestingLogin = login(tempLogin, tempPassword, false)
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
                setEmail(email);

                return cloudApi
                    .login(email, password, remember)
                    .then((result) => {
                        if (cloudApi.checkResponseHasError(result)) {
                            return $q.reject(result);
                        }

                        if (result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                            setEmail(result.data.email);
                            $rootScope.session.loginState = result.data.email; //Forcing changing loginState to reload interface
                        }
                        return result;
                    });
            }

            function logout(doNotRedirect) {
                cloudApi
                    .logout()
                    .finally(function () {
                        $rootScope.session.$reset(); // Clear session
                        if (!doNotRedirect) {
                            $location.path(CONFIG.redirectUnauthorised);
                        }
                        setTimeout(function () {
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
                })
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
