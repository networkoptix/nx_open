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
            'ngRoute',
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
                    if (error.status === 401) {
                        // Session expired - try to trigger browser reload
                        $rootScope.session.loginState = undefined;
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
        .config(['ngToastProvider', 'nxConfigServiceProvider', function (ngToastProvider, nxConfigServiceProvider) {
            var CONFIG = nxConfigServiceProvider.$get().getConfig();

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
            'languageServiceProvider', 'nxConfigServiceProvider',
            function ($routeProvider, $locationProvider, $compileProvider,
                      languageServiceProvider, nxConfigServiceProvider) {

                if (!PRODUCTION) {
                    $compileProvider.debugInfoEnabled(true);
                }

                $locationProvider.html5Mode(true);

                var CONFIG = nxConfigServiceProvider.$get().getConfig();

                var appState = {
                        viewsDir: 'static/views/', //'static/lang_' + lang + '/views/';
                        previewPath: '',
                        viewsDirCommon: 'static/web_common/views/',
                        trafficRelayHost: '{host}/gateway/{systemId}',
                        publicDownloads: false,
                        showHeaderAndFooter: true
                    };

                $.ajax({
                    url: CONFIG.apiBase + '/utils/settings',
                    async: false,
                    dataType: 'json'
                }).done(function(response){
                    appState.trafficRelayHost = response.trafficRelayHost;
                    appState.publicDownloads = response.publicDownloads;
                    appState.publicReleases = response.publicReleases;
                    appState.sortSupportedDevices = response.sortSupportedDevices;
                    appState.supportedResolutions = response.supportedResolutions;
                    appState.supportedHardwareTypes = response.supportedHardwareTypes;
                    appState.footerItems = response.footerItems ? JSON.parse(response.footerItems) : {};
                    
                    angular.extend(CONFIG, appState);
                });

                $.ajax({
                    // url: 'static/views/language.json',
                    url: CONFIG.apiBase + '/utils/language',
                    async: false,
                    dataType: 'json'
                })
                    .done(function (response) {
                        languageServiceProvider.setLanguage(response);// Set current language

                        // set local variables as providers cannot get values in config phase
                        appState.viewsDir = 'static/lang_' + response.language + '/views/'; //'static/lang_' + lang + '/views/';
                        appState.viewsDirCommon = 'static/lang_' + response.language + '/web_common/views/';

                        // detect preview mode
                        var preview = window.location.href.indexOf('preview') >= 0;
                        if (preview) {
                            appState.viewsDir = 'preview/' + appState.viewsDir;
                            appState.previewPath = 'preview';
                        }
                    })
                    .fail(function (error) {
                        // Fallback to default language

                        // if request to api/utils/language fails then
                        // cloud_portal is under maintenance
                        // TODO: Causes IOS to not load sometimes but not sure why
                        if (PRODUCTION && error.status > 500) {
                            window.location.href = '/static/503.html';
                        } else if (PRODUCTION) {
                            window.location.href = '/';
                        }

                        $.ajax({
                                url     : 'static/language.json',
                                async   : false,
                                dataType: 'json'
                            })
                            .done(function (response) {
                                languageServiceProvider.setLanguage(response);
                            });
                    })
                    .always(function () {

                        var lang = languageServiceProvider.$get().lang;

                        // For compatibility with legacy modules *****
                        L = lang;
                        Config = CONFIG;

                        angular.extend(Config, appState);
                        // *******************************************

                        $routeProvider
                            .when('/register/success', {
                                title: lang.pageTitles.registerSuccess,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.registerSuccess = true;
                                    }]
                                }
                            })
                            .when('/register/successActivated', {
                                title: lang.pageTitles.registerSuccess,
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
                                title: lang.pageTitles.register,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl'
                            })
                            .when('/register', {
                                title: lang.pageTitles.register,
                                templateUrl: CONFIG.viewsDir + 'regActions.html',
                                controller: 'RegisterCtrl'
                            })
                            .when('/account/password', {
                                title: lang.pageTitles.changePassword,
                                templateUrl: CONFIG.viewsDir + 'account.html',
                                controller: 'AccountCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.passwordMode = true;
                                    }]
                                }
                            })
                            .when('/account', {
                                title: lang.pageTitles.account,
                                templateUrl: CONFIG.viewsDir + 'account.html',
                                controller: 'AccountCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.accountMode = true;
                                    }]
                                }
                            })
                            .when('/systems', {
                                title: lang.pageTitles.systems,
                                templateUrl: CONFIG.viewsDir + 'systems.html',
                                controller: 'SystemsCtrl'
                            })
                            .when('/systems/:systemId', {
                                title: lang.pageTitles.system,
                                templateUrl: CONFIG.viewsDir + 'system.html',
                                controller: 'SystemCtrl'
                            })
                            .when('/systems/:systemId/share', {
                                title: lang.pageTitles.systemShare,
                                templateUrl: CONFIG.viewsDir + 'system.html',
                                controller: 'SystemCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.callShare = true;
                                    }]
                                }
                            })
                            .when('/systems/:systemId/view', {
                                title: lang.pageTitles.view,
                                templateUrl: CONFIG.viewsDir + 'view.html',
                                controller: 'ViewPageCtrl'
                            })
                            .when('/systems/:systemId/view/:cameraId', {
                                title: lang.pageTitles.view,
                                templateUrl: CONFIG.viewsDir + 'view.html',
                                controller: 'ViewPageCtrl'
                            })
                            .when('/embed/:systemId/view/:cameraId', {
                                title      : lang.pageTitles.view,
                                templateUrl: CONFIG.viewsDir + 'view.html',
                                controller : 'ViewPageCtrl',
                                resolve: {
                                    cleanSlate: [function () {
                                        CONFIG.showHeaderAndFooter = false;
                                    }]
                                }
                            })
                            .when('/activate', {
                                title: lang.pageTitles.activate,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.reactivating = true;
                                    }]
                                }
                            })
                            .when('/activate/success', {
                                title: lang.pageTitles.activateSuccess,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.activationSuccess = true;
                                    }]
                                }
                            })
                            .when('/activate/:activateCode', {
                                title: lang.pageTitles.activateCode,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl'
                            })
                            .when('/restore_password', {
                                title: lang.pageTitles.restorePassword,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.restoring = true;
                                    }]
                                }
                            })
                            .when('/restore_password/sent', {
                                title: lang.pageTitles.restorePassword,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.restoringSuccess = true;
                                    }]
                                }
                            })
                            .when('/restore_password/success', {
                                title: lang.pageTitles.restorePasswordSuccess,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.changeSuccess = true;
                                    }]
                                }
                            })
                            .when('/restore_password/:restoreCode', {
                                title: lang.pageTitles.restorePasswordCode,
                                templateUrl: CONFIG.viewsDir + 'activeActions.html',
                                controller: 'ActivateRestoreCtrl'
                            })
                            .when('/content/:page', {
                                title: '' /*lang.pageTitles.contentPage*/,
                                templateUrl: CONFIG.viewsDir + 'static.html',
                                controller: 'StaticCtrl'
                            })
                            .when('/debug', {
                                title: lang.pageTitles.debug,
                                templateUrl: CONFIG.viewsDir + 'debug.html',
                                controller: 'DebugCtrl'
                            })
                            .when('/login', {
                                title: lang.pageTitles.login,
                                templateUrl: CONFIG.viewsDir + 'startPage.html',
                                controller: 'StartPageCtrl',
                                resolve: {
                                    test: ['$route', function ($route) {
                                        $route.current.params.callLogin = true;
                                    }]
                                }
                            })
                            .when('/admin', {
                                resolve: {
                                    test: function(){
                                        window.location = '/admin/';
                                    }
                                }})
                            // for history purpose
                            .when('/downloads/history', {
                                template: '<download-history></download-history>'
                            })
                            .when('/downloads/:param?', {
                                template: '<download-history [route-param]="uriParam"></download-history>',
                                controller: [ '$scope', 'getParam', function ($scope, getParam) {
                                    $scope.uriParam = getParam;
                                }],
                                resolve: {
                                    getParam: [ '$route', function($route){
                                        return $route.current.params.param;
                                    }]
                                }
                            })
                            .when('/download', {
                                template: '<download-component></download-component>'
                            })
                            .when('/download/:platform', {
                                template: '<download-component [route-param-platform]="platform"></download-component>',
                                controller: [ '$scope', 'getPlatform', function ($scope, getPlatform) {
                                    $scope.platform = getPlatform;
                                }],
                                resolve: {
                                    getPlatform: [ '$route', function ($route) {
                                        return $route.current.params.platform
                                    }]
                                }
                            })
                            .when('/browser', {
                                template: '<non-supported-browser></non-supported-browser>'
                            })
                            .when('/campage', {
                                template: ''
                            })
                            .when('/sandbox', {
                                template: ''
                            })
                            .when('/integrations/:plugin?', {
                                template: ''
                            })
                            .when('/new-content', {
                                template: ''
                            })
                            .when('/right', {
                                template: ''
                            })
                            // **** routes for detail views should state full path ****
                            .when('/main/:route', {
                                template: ''
                            })
                            // ********************************************************
                            .when('/main', {
                                template: ''
                            })
                            .when('/', {
                                title: ''/*lang.pageTitles.startPage*/,
                                templateUrl: CONFIG.viewsDir + 'startPage.html',
                                controller: 'StartPageCtrl'
                            })
                            .otherwise({
                                title: lang.pageTitles.pageNotFound,
                                templateUrl: CONFIG.viewsDir + '404.html'
                            });
                    });
            }])
        .run(['nxLanguageService', 'languageService', function (nxLanguageService, languageService) {
            // make sure both language services are synchronized
            // had problem downgrading A6 'nxLanguageService' service to AJS provider so
            // it's set as regular service and running after 'config' phase --TT
            nxLanguageService.translate.use(languageService.lang.language);
        }]);
})();
