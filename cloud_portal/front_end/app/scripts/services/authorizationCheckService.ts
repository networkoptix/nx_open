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

                        console.log('INIT ->');
                        account.requestingLogin = login(tempLogin, tempPassword, false)
                            .then(() => {
                                console.log('INIT login(true) ->');
                                $location.search('auth', undefined);
                            })
                            .catch((error) => {
                                console.log('INIT login(error) ->', error);
                                $location.search('auth', undefined);
                            });
                    }
                }
            }

            function get() {
                if (account.requestingLogin) {
                    // login is requesting, so we wait
                    console.log('requestLogin ->');
                    return account.requestingLogin.then(() => {
                        console.log('requestLogin GET() ->');
                        account.requestingLogin = undefined; // clean requestingLogin reference
                        return get(); // Try again
                    });
                }

                return cloudApi
                    .account()
                    .then((account) => {
                        console.log('GET() data ->');
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

                            if ($rootScope.session.loginState) {
                                // If the user that logged in matches the current session there's no need to show
                                // the logout dialog.
                                if (result.data.email !== $rootScope.session.loginState) {
                                    logoutAuthorised();
                                }
                                console.log('LOGOUT ->');
                                return $q.resolve(true);
                            }

                            if (result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                                setEmail(result.data.email);
                                $rootScope.session.loginState = result.data.email; // Forcing changing loginState to reload interface
                            }

                            console.log('LOGIN ->');
                            return $q.reject(false);

                        }, () => {
                            NxDialogsService.notify(languageService.lang.errorCodes.wrongAuthCode, 'danger');
                        });
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
                        .confirm('', languageService.lang.dialogs.logoutAuthorisedTitle,
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
