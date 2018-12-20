(function () {
    
    'use strict';
    
    angular.module('nxCommon')
        .directive('placeholder', ['configService', function (configService) {
    
            const CONFIG = configService.config;
            
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
