'use strict';

angular.module('cloudApp')
    .directive('footer', function () {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/footer.html',
            link: function (scope, element, attrs) {

            }
        }
    });