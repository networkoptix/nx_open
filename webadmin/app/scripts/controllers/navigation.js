'use strict';

angular.module('webadminApp')
    .controller('NavigationCtrl', function ($scope, $location) {
        $scope.isActive = function (path) {
            var currentPath = $location.path().split('/')[1];
            return currentPath.split('?')[0] === path.split('/')[1].split('?')[0];
        };
    });
