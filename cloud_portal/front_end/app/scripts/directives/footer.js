(function () {

    'use strict';
    
    function NxFooter($rootScope, configService) {

        var CONFIG = configService.config;
    
        return {
            restrict   : 'E',
            templateUrl: CONFIG.viewsDir + 'components/footer.html',
            link       : function (scope) {

                scope.viewFooter = CONFIG.showHeaderAndFooter;
                
                $rootScope.$on('nx.layout.footer', function (event, opt) {
                    // An event to control visibility of the header
                    // ... i.e. when in view camera in embed
                    // ... and check if Config.showHeaderAndFooter is false
                    // as view controller resets header and footer on destroy
                    if (CONFIG.showHeaderAndFooter) {
                        scope.viewFooter = !opt.state;
                    }
                });
            }
        };
    }
    
    NxFooter.$inject = ['$rootScope', 'configService'];
    
    angular
        .module('cloudApp.directives')
        .directive('nxFooter', NxFooter);
    
})();
