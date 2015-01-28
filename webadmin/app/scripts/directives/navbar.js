'use strict';

angular.module('webadminApp')
    .directive('navbar', function () {
        return {
            restrict: 'E',
            templateUrl: 'views/navbar.html',
            controller: 'NavigationCtrl',
            scope:{
                noPanel:'='
            }
        };
    });
