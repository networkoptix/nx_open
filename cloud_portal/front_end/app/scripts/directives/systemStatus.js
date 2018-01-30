'use strict';

angular.module('cloudApp')
    .directive('systemStatus', function () {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/system-status.html',
            scope:{
                system:'='
            }
        };
    });