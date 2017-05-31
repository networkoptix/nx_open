
'use strict';

angular.module('nxCommon')
    .directive('icon', function () {
        return {
            restrict: 'E',
            scope: {
                'for': '='
            },
            // replace: true,
            template: '<span ng-if=\'for || onFalse\' class=\'glyphicon glyphicon-{{for?onTrue:onFalse}} {{class}}\' title=\'{{for?titleTrue:titleFalse}}\'></span>',
            link: function (scope, element, attrs) {
                scope.onTrue = attrs.onTrue || 'ok';
                scope.onFalse = attrs.onFalse;
                scope['class'] = attrs['class'];
                scope.titleTrue = attrs.titleTrue;
                scope.titleFalse = attrs.titleFalse;
            }
        };
    });
