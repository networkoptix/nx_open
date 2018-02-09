'use strict';

angular.module('cloudApp')
    .directive('systemStatus', function () {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/system-status.html',
            scope:{
                system:'='
            },
            link:function(scope){
            	scope.cannotMerge = !scope.system.capabilities || scope.system.capabilities.indexOf('cloudMerge') < 0;
            }
        };
    });