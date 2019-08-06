(function () {
    
    'use strict';
    
    function CameraViewInformer($rootScope, $timeout, nxConfigService, languageService) {
        
        const CONFIG = nxConfigService.getConfig();
        const LANG = languageService.lang;
        
        function updateTpl(scope) {
            //offline and unauthorized are special cases. All others can be set without editing
            if (scope.alertType === 'status') {
                scope.placeholderTitle = LANG.common.cameraStates[(scope.flags[scope.alertType]).toLowerCase()];
                scope.message = '';
                scope.iconClass = (scope.flags.status === 'Offline') ? 'camera-view-offline' : 'camera-view-unauthorized';
            } else {
                scope.iconClass = 'camera-view-error';
                scope.placeholderTitle = LANG.common.cameraStates.error;
                scope.message = (!scope.flags.errorDescription) ? LANG.common.cameraStates[scope.alertType] : scope.flags.errorDescription;
            }
            
            // Visualise after informer resize
            // ... when error happen - it expands to
            // 100% ignoring timeline height and info shifts -- TT
            $timeout(function () {
                scope.displayInfo = true;
            }, 500);
        }
        
        function linkFunction(scope) {
            
            scope.$watch('flags', function () {
                scope.placeholderTitle = '';
                scope.message = '';
                scope.iconClass = '';
                scope.displayInfo = false;
                scope.condition = false;
                scope.alertType = null;
                
                //check for flag
                
                for (var key in scope.flags) {
                    if (key !== 'status' && scope.flags[key]) {
                        scope.alertType = key;
                        break;
                    }
                }
                //If position has been selected then there is an archive no message is required
                if (typeof(scope.flags.positionSelected) !== 'undefined' &&
                    scope.flags.positionSelected &&
                    scope.alertType === 'positionSelected') {
                    return;
                }
                
                //if status is online dont show any message
                if (scope.flags.status === 'Offline' ||
                    !scope.alertType && typeof(scope.flags.status) !== 'undefined' &&
                    !(scope.flags.status === 'Online' || scope.flags.status === 'Recording')) {
                    
                    scope.alertType = 'status';
                }
                //if non flag break
                if (!scope.alertType) {
                    return;
                }
    
                scope.condition = true;
                
                updateTpl(scope);
                
            }, true);
    
            $rootScope.$on('nx.player.playing', function () {
                // Ensure Safari will hide the informer
                // probable cause: Event misalignment
                scope.condition = false;
            });
        }
        
        return {
            restrict: 'E',
            scope: {
                flags: '<',
                preloader: '<',
                preview: '<'
            },
            templateUrl: CONFIG.viewsDirCommon + 'components/placeholder.html',
            link: linkFunction
        };
    }
    
    CameraViewInformer.$inject = ['$rootScope', '$timeout', 'nxConfigService', 'languageService'];
    
    angular
        .module('nxCommon')
        .directive('cameraViewInformer', CameraViewInformer);
})();
