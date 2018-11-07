(function () {

    'use strict';
    
    function NxFooter($rootScope, nxConfigService) {
    
        var CONFIG = nxConfigService.getConfig();

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
        };
    }
    
    NxFooter.$inject = ['$rootScope', 'nxConfigService'];
    
    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);
    
})();
