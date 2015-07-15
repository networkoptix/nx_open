'use strict';

'use strict';

angular.module('webadminApp')
    .directive('videowindow', ['$interval','$timeout','animateScope', function ($interval,$timeout,animateScope) {
        return {
            restrict: 'E',
            scope: {
                vgPlayerReady:"&", // $scope.vgPlayerReady({$API: $scope.API});
                vgUpdateTime:"&",
                vgSrc:"="
            },
            templateUrl: 'views/components/videowindow.html',// ???
            link: function (scope, element/*, attrs*/) {

                /*
                 *
                 *  */

            }
        }
    }]);