angular
    .module('nxCommon')
    .directive('cameraViewInformer', CameraViewInformer);

function CameraViewInformer() {

    'use strict';

    function updateTpl(scope) {
        //offline and unauthorized are special cases. All others can be set without editing
        if (scope.alertType === 'status') {
            scope.placeholderTitle = L.common.cameraStates[(scope.flags[scope.alertType]).toLowerCase()];
            scope.message = '';
            scope.iconClass = (scope.flags.status === 'Offline') ? 'camera-view-offline' : 'camera-view-unauthorized';
        } else {
            scope.iconClass = 'camera-view-error';
            scope.placeholderTitle = L.common.cameraStates.error;
            scope.message = scope.flags.errorDescription;
        }
    }

    function linkFunction(scope) {

        scope.$watch('flags', function (newFlags, old) {
            scope.placeholderTitle = '';
            scope.message = '';
            scope.iconClass = '';
            scope.condition = false;
            scope.alertType = null;

            //check for flag
            for (var key in newFlags) {
                if (key !== 'status' && newFlags[key]) {
                    scope.alertType = key;
                    break;
                }
            }
            //If position has been selected then there is an archive no message is required
            if (typeof(newFlags.positionSelected) !== 'undefined' &&
                newFlags.positionSelected &&
                scope.alertType === 'positionSelected') {

                return;
            }

            //if status is online dont show any message
            if (!scope.alertType &&
                typeof(newFlags.status) !== 'undefined' &&
                newFlags.status !== '2' && newFlags.status !== '3' &&               // camera.status changed to code in API
                newFlags.status !== 'Online' && newFlags.status !== 'Recording') {  // keeping status strings just in case

                scope.alertType = 'status';
            }
            //if non flag break
            if (!scope.alertType) {
                return;
            }

            scope.condition = true;

            updateTpl(scope);

        }, true);
    }

    return {
        restrict: 'E',
        scope: {
            flags: '=',
            preloader: '=',
            preview: '='
        },
        templateUrl: Config.viewsDirCommon + 'components/placeholder.html',
        link: linkFunction
    };
}
