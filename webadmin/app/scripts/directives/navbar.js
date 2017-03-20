'use strict';

angular.module('webadminApp')
    .directive('navbar', function () {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'navbar.html',
            controller: 'NavigationCtrl'
        };
    });
