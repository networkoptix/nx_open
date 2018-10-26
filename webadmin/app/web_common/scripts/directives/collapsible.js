(function () {

    'use strict';

    angular
        .module('cloudApp.directives')
        .directive('nxCollapsible', NxCollapsible);

    function NxCollapsible() {
        return {
            restrict: 'A',
            scope   : {
                trigger: '<nxCollapsible'
            },
            link    : function (scope, element, attr) {

                element.addClass('nx-collapsible');
                scope.trigger = scope.trigger || false;

                scope.$watch('trigger', function (newVal) {
                    if (newVal) {
                        element.addClass('show');
                    } else {
                        element.removeClass('show');
                    }
                });
            }
        }
    }
})();
