(function () {
    
    'use strict';
    
    angular.module('nxCommon')
        .directive('placeholder', ['nxConfigService', function (nxConfigService) {
    
            const CONFIG = nxConfigService.getConfig();
            
            return {
                restrict: 'E',
                scope: {
                    iconClass: '=',
                    placeholderTitle: '=',
                    message: '=',
                    preloader: '=',
                    condition: '=ngIf'
                },
                templateUrl: CONFIG.viewsDirCommon + 'components/placeholder.html'
            };
        }]);
})();
