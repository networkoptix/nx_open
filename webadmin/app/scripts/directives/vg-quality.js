
'use strict';
angular.module('com.2fdevs.videogular.plugins.controls')
    .directive(
    'vgQuality', function () {
        return {
            restrict: 'E',
            require: '^videogular',
            transclude: true,
            template: '<button class="iconButton enter" ng-click="onClickQuality()" ng-class="qualityIcon" aria-label="Chose quality"></button>',

            scope: {
                current: '=',
                list: '='
            },
            link: function (scope, elem, attr, API) {

            }
        };
    });