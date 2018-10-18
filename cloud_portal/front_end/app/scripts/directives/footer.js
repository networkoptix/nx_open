(function () {

    'use strict';

    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);

    NxFooter.$inject = [ '$rootScope', 'nxConfigService', 'account' ];

    function NxFooter($rootScope, nxConfigService, account) {

        var CONFIG = nxConfigService.getConfig(),
            user   = {};

        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link       : function (scope) {

                scope.viewFooter = CONFIG.showHeaderAndFooter;

                $rootScope.$on('nx.layout.footer', function (event, opt) {
                    // An event to control visibility of the footer
                    // ... i.e. when in "View" the server has list of cameras
                    scope.viewFooter = !opt.state;
                });
            }
        }
    }
})();
