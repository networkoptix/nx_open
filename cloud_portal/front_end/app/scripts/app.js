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
]).config(['ngToastProvider', function(ngToastProvider) {
    ngToastProvider.configure({
        timeout: Config.alertTimeout,
        animation: 'fade',
        horizontalPosition: 'center',
        maxNumber: Config.alertsMaxCount,
        combineDuplications: true,
        newestOnTop: false
    });
}]).config(['$routeProvider', '$locationProvider',function ($routeProvider, $locationProvider) {
    $locationProvider.html5Mode(true); //Get rid of hash in urls
    $locationProvider.hashPrefix('!');

    $routeProvider
        .when('/register/success', {
            templateUrl: 'static/views/register.html',
            controller: 'RegisterCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.registerSuccess = true; }]
            }
        })
        .when('/register/:email', {
            templateUrl: 'static/views/register.html',
            controller: 'RegisterCtrl'
        })
        .when('/register', {
            templateUrl: 'static/views/register.html',
            controller: 'RegisterCtrl'
        })
        .when('/account/password', {
            templateUrl: 'static/views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.passwordMode = true; }]
            }
        })
        .when('/account', {
            templateUrl: 'static/views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.accountMode = true; }]
            }
        })


        .when('/systems', {
            templateUrl: 'static/views/systems.html',
            controller: 'SystemsCtrl'
        })
        .when('/systems/:systemId', {
            templateUrl: 'static/views/system.html',
            controller: 'SystemCtrl'
        })
        .when('/systems/:systemId/share', {
            templateUrl: 'static/views/system.html',
            controller: 'SystemCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callShare = true; }]
            }
        })
        .when('/systems/:systemId/share/:shareEmail', {
            templateUrl: 'static/views/system.html',
            controller: 'SystemCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callShare = true; }]
            }
        })


        .when('/activate', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.reactivating = true; }]
            }
        })
        .when('/activate/sent', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.reactivatingSuccess = true; }]
            }
        })
        .when('/activate/success',{
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.activationSuccess = true; }]
            }
        })
        .when('/activate/:activateCode', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/restore_password', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.restoring = true; }]
            }
        })
        .when('/restore_password/sent', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.restoringSuccess = true; }]
            }
        })
        .when('/restore_password/success', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.changeSuccess = true; }]
            }
        })
        .when('/restore_password/:restoreCode', {
            templateUrl: 'static/views/activate_restore.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/content/:page', {
            templateUrl: 'static/views/static.html',
            controller: 'StaticCtrl'
        })

        .when('/debug', {
            templateUrl: 'static/views/debug.html',
            controller: 'DebugCtrl'
        })

        .when('/login', {
            templateUrl: 'static/views/startPage.html',
            controller: 'StartPageCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callLogin = true; }]
            }
        })
        .otherwise({
            templateUrl: 'static/views/startPage.html',
            controller: 'StartPageCtrl'
        });
}]).run(['$route', '$rootScope', '$location', function ($route, $rootScope, $location) {
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
}]);
