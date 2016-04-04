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
}]).config(function ($routeProvider) {
    $routeProvider
        .when('/register/success', {
            templateUrl: 'views/register.html',
            controller: 'RegisterCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.registerSuccess = true; }]
            }
        })
        .when('/register/:email', {
            templateUrl: 'views/register.html',
            controller: 'RegisterCtrl'
        })
        .when('/register', {
            templateUrl: 'views/register.html',
            controller: 'RegisterCtrl'
        })
        .when('/account/password', {
            templateUrl: 'views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.passwordMode = true; }]
            }
        })
        .when('/account', {
            templateUrl: 'views/account.html',
            controller: 'AccountCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.accountMode = true; }]
            }
        })


        .when('/systems', {
            templateUrl: 'views/systems.html',
            controller: 'SystemsCtrl'
        })
        .when('/systems/:systemId', {
            templateUrl: 'views/system.html',
            controller: 'SystemCtrl'
        })
        .when('/systems/:systemId/share', {
            templateUrl: 'views/system.html',
            controller: 'SystemCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callShare = true; }]
            }
        })
        .when('/systems/:systemId/share/:shareEmail', {
            templateUrl: 'views/system.html',
            controller: 'SystemCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callShare = true; }]
            }
        })


        .when('/activate', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.reactivating = true; }]
            }
        })
        .when('/activate/success',{
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.activationSuccess = true; }]
            }
        })
        .when('/activate/:activateCode', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/restore_password', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.restoring = true; }]
            }
        })
        .when('/restore_password/success', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.changeSuccess = true; }]
            }
        })
        .when('/restore_password/:restoreCode', {
            templateUrl: 'views/activate_restore.html',
            controller: 'ActivateRestoreCtrl'
        })
        .when('/static/:page', {
            templateUrl: 'views/static.html',
            controller: 'StaticCtrl'
        })

        .when('/debug', {
            templateUrl: 'views/debug.html',
            controller: 'DebugCtrl'
        })

        .when('/login', {
            templateUrl: 'views/startPage.html',
            controller: 'StartPageCtrl',
            resolve: {
                test: ['$route',function ($route) { $route.current.params.callLogin = true; }]
            }
        })
        .otherwise({
            templateUrl: 'views/startPage.html',
            controller: 'StartPageCtrl'
        });
});



angular.module('cloudApp').run(['$route', '$rootScope', '$location', function ($route, $rootScope, $location) {
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
