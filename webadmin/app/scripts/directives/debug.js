'use strict';

angular.module('webadminApp')
    .directive('debug', function() {
        return {
            restrict: 'A',
            scope:{
                debug:'='
            },
            link : function(scope, $element) {
                scope.$watch("debug",function(){
                    $element.html(JSON.stringify(scope.debug, null, 2));
                });
            }
        };
    });