(function () {

    'use strict';

    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);

    NxFooter.$inject = [ 'configService' ];

    function NxFooter(configService) {

        var CONFIG = configService.config;

        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link       : function (scope, element, attrs) {

            }
        }
    }
})();
