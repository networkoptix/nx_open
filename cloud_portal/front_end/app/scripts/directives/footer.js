'use strict';

angular.module('cloudApp')
    .directive('footer', function (dialogs, cloudApi, $sessionStorage, account, $location) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/footer.html',
            link: function (scope, element, attrs) {

            }
        }
    });