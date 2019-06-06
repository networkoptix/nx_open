(() => {

        'use strict';

        angular
            .module('cloudApp')
            .factory('authorizationCheckService', AuthorizationCheckService);

        AuthorizationCheckService.$inject = ['$rootScope', '$q', '$localStorage',
            'cloudApi', 'nxConfigService', 'NxDialogsService', 'languageService', '$location'];

        function AuthorizationCheckService($rootScope, $q, $localStorage,
                                           cloudApi, nxConfigService, NxDialogsService, languageService, $location) {

            const CONFIG = nxConfigService.getConfig();

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
            }

            function get() {
                console.log('GET() ->');
                return cloudApi
                    .account()
                    .then((account) => {
                        console.log('GET() (then) ->');
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

                            if ($rootScope.session.loginState) {
                                // If the user that logged in matches the current session there's no need to show
                                // the logout dialog.
                                if (result.data.email !== $rootScope.session.loginState) {
                                    console.log('logoutAuthorised ->');
                                    logoutAuthorised();
                                }

                                return $q.resolve(true);
                            }

                            if (result.data.email) { // (result.data.resultCode === L.errorCodes.ok)
                                setEmail(result.data.email);
                                $rootScope.session.loginState = result.data.email; // Forcing changing loginState to reload interface
                                console.log('setEmail ->');
                            }

                            get().then(() => {
                                     console.log('LOGIN resolved ->');
                                     return $q.resolve(true);
                                 })
                                 .catch(error => {
                                     console.log('LOGIN rejected ->');
                                     return $q.reject(error);
                                 });

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
                console.log('redirectAuthorised (start) ->');
                get().then(() => {
                    console.log('redirectAuthorised (then) ->');
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
