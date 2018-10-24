(function () {

    'use strict';

    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);

    NxFooter.$inject = [ '$rootScope', 'configService' ];

    function NxFooter($rootScope, configService) {

        var CONFIG = configService.config;
    
        function isActive(val) {
            var currentPath = $location.path();
            return currentPath.indexOf(val) >= 0;
        }

        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link       : function (scope) {

                scope.viewFooter = CONFIG.showHeaderAndFooter;
    
                if (!isActive('/embed')) {
                    scope.viewHeader = false;
                }

                $rootScope.$on('nx.layout.footer', function (event, opt) {
                    // An event to control visibility of the footer
                    // ... i.e. when in "View" the server has list of cameras
                    scope.viewFooter = !opt.state;
                });
            }
        }
    }
})();
