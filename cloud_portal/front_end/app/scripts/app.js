'use strict';

angular.module('cloudApp', [
    'ipCookie',
    'ngResource',
    'ngSanitize',
    'ngAnimate',
    'ngRoute',
    'ui.bootstrap',
    'ngStorage',
    'base64',
    'ngToast'
]).config(['$httpProvider', function ($httpProvider) {
    $httpProvider.defaults.xsrfCookieName = 'csrftoken';
    $httpProvider.defaults.xsrfHeaderName = 'X-CSRFToken';
}]).config(['ngToastProvider', function(ngToastProvider) {
    ngToastProvider.configure({
        timeout: Config.alertTimeout,
        animation: 'fade',
        horizontalPosition: 'center',
        maxNumber: Config.alertsMaxCount,
        combineDuplications: true,
        newestOnTop: false
    });
}]).config(['$routeProvider', '$locationProvider', function ($routeProvider, $locationProvider) {
    $locationProvider.html5Mode(true); //Get rid of hash in urls
    $locationProvider.hashPrefix('!');

    $routeProvider
        .when('/register/success', {
            title: L.pageTitles.registerSuccess,
            templateUrl: 'static/views/regActions',
            controller: 'RegisterCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.registerSuccess = true; }]
            }
        })
        .when('/register/successActivated', {
            title: L.pageTitles.registerSuccess,
            templateUrl: 'static/views/regActions',
            controller: 'RegisterCtrl',
            resolve: {
                test: ['$route',function ($route) {
                    $route.current.params.registerSuccess = true;
                    $route.current.params.activated = true;
                }]
            }
        })
        .when('/register/:code', {
            title: L.pageTitles.register,
            templateUrl: 'static/views/regActions',
            controller: 'RegisterCtrl'
        })
        .when('/register', {
            title: L.pageTitles.register,
            templateUrl: 'static/views/regActions',
            controller: 'RegisterCtrl'
        })
        .when('/account/password', {
            title: L.pageTitles.changePassword,
            templateUrl: 'static/views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.passwordMode = true; }]
            }
        })
        .when('/account', {
            title: L.pageTitles.account,
            templateUrl: 'static/views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.accountMode = true; }]
            }
        })


        .when('/systems', {
            title: L.pageTitles.systems,
            templateUrl: 'static/views/systems.html',
            controller: 'SystemsCtrl'
        })
        .when('/systems/default', {
            // Special mode - user will be redirected to default system if default system can be determined (if user has one system)
            title: L.pageTitles.systems,
            templateUrl: 'static/views/systems.html',
            controller: 'SystemsCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.defaultMode = true; }]
            }
        })
        .when('/systems/:systemId', {
            title: L.pageTitles.system,
            templateUrl: 'static/views/system.html',
            controller: 'SystemCtrl'
        })
        .when('/systems/:systemId/share', {
            title: L.pageTitles.systemShare,
            templateUrl: 'static/views/system.html',
            controller: 'SystemCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callShare = true; }]
            }
        })
        .when('/activate', {
            title: L.pageTitles.activate,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.reactivating = true; }]
            }
        })
        .when('/activate/sent', {
            title: L.pageTitles.activateSent,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.reactivatingSuccess = true; }]
            }
        })
        .when('/activate/success',{
            title: L.pageTitles.activateSuccess,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.activationSuccess = true; }]
            }
        })
        .when('/activate/:activateCode', {
            title: L.pageTitles.activateCode,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/restore_password', {
            title: L.pageTitles.restorePassword,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.restoring = true; }]
            }
        })
        .when('/restore_password/sent', {
            title: L.pageTitles.restorePassword,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.restoringSuccess = true; }]
            }
        })
        .when('/restore_password/success', {
            title: L.pageTitles.restorePasswordSuccess,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.changeSuccess = true; }]
            }
        })
        .when('/restore_password/:restoreCode', {
            title: L.pageTitles.restorePasswordCode,
            templateUrl: 'static/views/activeActions.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/content/:page', {
            title: L.pageTitles.contentPage,
            templateUrl: 'static/views/static.html',
            controller: 'StaticCtrl'
        })

        .when('/debug', {
            title: L.pageTitles.debug,
            templateUrl: 'static/views/debug.html',
            controller: 'DebugCtrl'
        })

        .when('/login', {
            title: L.pageTitles.login,
            templateUrl: 'static/views/startPage.html',
            controller: 'StartPageCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callLogin = true; }]
            }
        })

        .when('/download', {
            title: L.pageTitles.download,
            templateUrl: 'static/views/download.html',
            controller: 'DownloadCtrl'
        })
        .when('/download/:platform', {
            title: L.pageTitles.downloadPlatform,
            templateUrl: 'static/views/download.html',
            controller: 'DownloadCtrl'
        })
        .when('/', {
            title: L.pageTitles.startPage,
            templateUrl: 'static/views/startPage.html',
            controller: 'StartPageCtrl'
        })
        .otherwise({
            title: L.pageTitles.pageNotFound,
            templateUrl: 'static/views/404.html'
        });
}]).run(['$route', '$rootScope', '$location', 'page', function ($route, $rootScope, $location, page) {
    var original = $location.path;
    $location.path = function (path, reload) {
        if (reload === false) {
            var lastRoute = $route.current;
            var un = $rootScope.$on('$locationChangeSuccess', function () {
                $route.current = lastRoute;
                un();
            });
        }
        return original.apply($location, [path]);
    };

    $rootScope.$on('$routeChangeSuccess', function (event, current, previous) {
        if(current.$$route){
            page.title(current.$$route.title);
        }else{
            page.title(L.pageTitles.pageNotFound);
        }
    });
}]);
