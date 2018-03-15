(function () {

    'use strict';

    angular
    .module('cloudApp.directives')
    .directive('nxFooter', NxFooter);

    NxFooter.$inject = ['CONFIG'];

    function NxFooter (CONFIG) {
        return {
            restrict: 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link: function (scope, element, attrs) {

            }
        }
    }
})();
