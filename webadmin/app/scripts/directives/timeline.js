'use strict';

angular.module('webadminApp')
    .directive('icon', function () {
        return {
            restrict: 'E',
            scope: {
                'for': '='
            },
            template: '<span ng-if=\'for || onFalse\' class=\'glyphicon glyphicon-{{for?onTrue:onFalse}} {{class}}\' title=\'{{for?titleTrue:titleFalse}}\'></span>',
            link: function (scope, element, attrs) {

            }
        };
    });
