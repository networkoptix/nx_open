angular.module('cloudApp.services', []);
angular.module('cloudApp.controllers', []);
angular.module('cloudApp.directives', []);
angular.module('cloudApp.components', []);
angular.module('cloudApp.animations', []);
angular.module('cloudApp.filters', []);
angular.module('cloudApp.constants', []);
angular.module('cloudApp.templates', []);

window.Config = {};
window.L = {};

(function () {

    'use strict';
    angular
        .module('cloudApp', [
            'ngCookies',
            'ngResource',
            'ngSanitize',
            'ngAnimate',
            'ngRoute',
            'ui.bootstrap',
            'ngStorage',
            'base64',
            'nxCommon',
            'ngToast',
            'angular-clipboard',

            // cloudApp modules
            'cloudApp.animations',
            'cloudApp.controllers',
            'cloudApp.services',
            'cloudApp.directives',
            'cloudApp.filters',
            'cloudApp.constants',
            'cloudApp.templates'

        ])
        .factory('httpResponseInterceptor', ['$q', '$rootScope', function($q, $rootScope) {
            return {
                responseError: function(error) {
                    if (error.status === 401 || error.status === 403) {
                        // Session expired - try to trigger browser reload
                        $rootScope.session.loginState = false;
                    }
                    return $q.reject(error);
                }
            };
        }])
        .config(['$httpProvider', function ($httpProvider) {
            $httpProvider.defaults.xsrfCookieName = 'csrftoken';
            $httpProvider.defaults.xsrfHeaderName = 'X-CSRFToken';
            $httpProvider.interceptors.push('httpResponseInterceptor');
        }])
        .config(['ngToastProvider', 'CONFIG', function (ngToastProvider, CONFIG) {
            ngToastProvider.configure({
                timeout: CONFIG.alertTimeout,
                animation: 'fade',
                horizontalPosition: 'center',
                maxNumber: CONFIG.alertsMaxCount,
                combineDuplications: true,
                newestOnTop: false
            });
        }])
        .config(['$routeProvider', '$locationProvider', '$compileProvider',
            'languageServiceProvider', 'CONFIG',
            function ($routeProvider, $locationProvider, $compileProvider,
                      languageServiceProvider, CONFIG) {

                $compileProvider.debugInfoEnabled(false); // PROD -> set to false
                $locationProvider.html5Mode(true);
                // .hashPrefix('!');

                var appState = {
                        viewsDir: 'static/views/', //'static/lang_' + lang + '/views/';
                        previewPath: '',
                        viewsDirCommon: 'static/web_common/views/'
                    };

                $.ajax({
                    // url: 'static/views/language.json',
                    url: 'api/utils/language',
                    async: false,
                    dataType: 'json'
                })
                    .done(function (response) {
                        //console.log(response);
                        languageServiceProvider.setLanguage(response);// Set current language

                        // set local variables as providers cannot get values in config phase
                        appState.viewsDir = 'static/lang_' + languageServiceProvider.$get().lang.language + '/views/'; //'static/lang_' + lang + '/views/';
                        appState.viewsDirCommon = 'static/lang_' + languageServiceProvider.$get().lang.language + '/web_common/views/';

                        // detect preview mode
                        var preview = window.location.href.indexOf('preview') >= 0;
                        if (preview) {
                            console.log('preview mode');
                            appState.viewsDir = 'preview/' + appState.viewsDir;
                            appState.previewPath = 'preview';
                        }
                    })
                    .fail(function (error) {
                        //console.log(error);
                        // Fallback to default language
                        //if request to api/utils/language fails then cloud_portal is under maintaince
                        if( error.status > 500) {
                            window.location.href = '/static/503.html';
                        }
                        $.ajax({
                            url: 'static/language.json',
                            async: false,
                            dataType: 'json'
                        })
                            .done(function (response) {
                                languageServiceProvider.setLanguage(response);

                                $.ajax({
                                    url: 'web_common/commonLanguage.json',
                                    async: false,
                                    dataType: 'json'
                                })
                                    .done(function (response) {
                                        languageServiceProvider.setCommonLanguage(response);
                                    })
                                    .fail(function (error) {
                                        console.error('Can\'t get commonLanguage.json -> ' + error);
                                    });
                            });
                    })
                    .always(function () {
                        // For compatibility with legacy modules *****
                        L = languageServiceProvider.$get().lang;
                        Config = CONFIG;

                        angular.extend(Config, appState);
                        // *******************************************

                        $routeProvider
                            .when('/register/success', {
                                title: languageServiceProvider.$get().lang.pageTitles.registerSuccess,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.registerSuccess = true;
                                    }]
                                }
                            })
                            .when('/register/successActivated', {
                                title: languageServiceProvider.$get().lang.pageTitles.registerSuccess,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.registerSuccess = true;
                                        $route.current.params.activated = true;
                                    }]
                                }
                            })
                            .when('/register/:code', {
                                title: languageServiceProvider.$get().lang.pageTitles.register,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl'
                            })
                            .when('/register', {
                                title: languageServiceProvider.$get().lang.pageTitles.register,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl'
                            })
                            .when('/account/password', {
                                title: languageServiceProvider.$get().lang.pageTitles.changePassword,
                                templateUrl: CONFIG.viewsDir + 'account.html',
                                controller: 'AccountCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.passwordMode = true;
                                    }]
                                }
                            })
                            .when('/account', {
                                title: languageServiceProvider.$get().lang.pageTitles.account,
                                templateUrl: CONFIG.viewsDir + 'account.html',
                                controller: 'AccountCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.accountMode = true;
                                    }]
                                }
                            })
                            .when('/systems', {
                                title: languageServiceProvider.$get().lang.pageTitles.systems,
                                templateUrl: CONFIG.viewsDir + 'systems.html',
                                controller: 'SystemsCtrl'
                            })
                            .when('/systems/:systemId', {
                                title: languageServiceProvider.$get().lang.pageTitles.system,
                                templateUrl: CONFIG.viewsDir + 'system.html',
                                controller: 'SystemCtrl'
                            })
                            .when('/systems/:systemId/share', {
                                title: languageServiceProvider.$get().lang.pageTitles.systemShare,
                                templateUrl: CONFIG.viewsDir + 'system.html',
                                controller: 'SystemCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.callShare = true;
                                    }]
                                }
                            })
                            .when('/systems/:systemId/view', {
                                title: languageServiceProvider.$get().lang.pageTitles.view,
                                templateUrl: CONFIG.viewsDir + 'view.html',
                                controller: 'ViewPageCtrl'
                            })
                            .when('/systems/:systemId/view/:cameraId', {
                                title: languageServiceProvider.$get().lang.pageTitles.view,
                                templateUrl: CONFIG.viewsDir + 'view.html',
                                controller: 'ViewPageCtrl'
                            })
                            .when('/activate', {
                                title: languageServiceProvider.$get().lang.pageTitles.activate,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.reactivating = true;
                                    }]
                                }
                            })
                            .when('/activate/success', {
                                title: languageServiceProvider.$get().lang.pageTitles.activateSuccess,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.activationSuccess = true;
                                    }]
                                }
                            })
                            .when('/activate/:activateCode', {
                                title: languageServiceProvider.$get().lang.pageTitles.activateCode,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl'
                            })
                            .when('/restore_password', {
                                title: languageServiceProvider.$get().lang.pageTitles.restorePassword,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.restoring = true;
                                    }]
                                }
                            })
                            .when('/restore_password/sent', {
                                title: languageServiceProvider.$get().lang.pageTitles.restorePassword,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.restoringSuccess = true;
                                    }]
                                }
                            })
                            .when('/restore_password/success', {
                                title: languageServiceProvider.$get().lang.pageTitles.restorePasswordSuccess,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.changeSuccess = true;
                                    }]
                                }
                            })
                            .when('/restore_password/:restoreCode', {
                                title: languageServiceProvider.$get().lang.pageTitles.restorePasswordCode,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl'
                            })
                            .when('/content/:page', {
                                title: '' /*languageServiceProvider.$get().lang.pageTitles.contentPage*/,
                                templateUrl: CONFIG.viewsDir + 'static.html',
                                controller: 'StaticCtrl'
                            })
                            .when('/debug', {
                                title: languageServiceProvider.$get().lang.pageTitles.debug,
                                templateUrl: CONFIG.viewsDir + 'debug.html',
                                controller: 'DebugCtrl'
                            })
                            .when('/login', {
                                title: languageServiceProvider.$get().lang.pageTitles.login,
                                templateUrl: CONFIG.viewsDir + 'startPage.html',
                                controller: 'StartPageCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.callLogin = true;
                                    }]
                                }
                            })
                            .when('/downloads/history', {
                                title: languageServiceProvider.$get().lang.pageTitles.download,
                                templateUrl: CONFIG.viewsDir + 'downloadHistory.html',
                                controller: 'DownloadHistoryCtrl'
                            })
                            .when('/downloads/:build', {
                                title: languageServiceProvider.$get().lang.pageTitles.download,
                                templateUrl: CONFIG.viewsDir + 'downloadHistory.html',
                                controller: 'DownloadHistoryCtrl'
                            })
                            .when('/downloads', {
                                title: languageServiceProvider.$get().lang.pageTitles.download,
                                templateUrl: CONFIG.viewsDir + 'download.html',
                                controller: 'DownloadCtrl'
                            })
                            .when('/download', {
                                title: languageServiceProvider.$get().lang.pageTitles.download,
                                templateUrl: CONFIG.viewsDir + 'download.html',
                                controller: 'DownloadCtrl'
                            })
                            .when('/download/:platform', {
                                title: languageServiceProvider.$get().lang.pageTitles.downloadPlatform,
                                templateUrl: CONFIG.viewsDir + 'download.html',
                                controller: 'DownloadCtrl'
                            })
                            .when('/', {
                                title: ''/*languageServiceProvider.$get().lang.pageTitles.startPage*/,
                                templateUrl: CONFIG.viewsDir + 'startPage.html',
                                controller: 'StartPageCtrl'
                            })
                            .otherwise({
                                title: languageServiceProvider.$get().lang.pageTitles.pageNotFound,
                                templateUrl: CONFIG.viewsDir + '404.html'
                            });
                    });
            }]);
})();
