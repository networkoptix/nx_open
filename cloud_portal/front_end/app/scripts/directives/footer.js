'use strict';

angular.module('cloudApp')
    .directive('footer', function () {
        return {
            restrict: 'E',
            templateUrl: 'views/components/footer.html',
            link: function (scope, element, attrs) {

            }
        }
    });