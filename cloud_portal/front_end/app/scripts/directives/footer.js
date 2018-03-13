'use strict';

angular.module('cloudApp')
    .directive('nxFooter', function () {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/footer.html',
            link: function (scope, element, attrs) {

            }
        }
    });
