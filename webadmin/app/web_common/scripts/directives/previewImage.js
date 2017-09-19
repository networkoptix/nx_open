'use strict';

/**
 * This is smart video-plugin.
 * 1. It gets list of possible video-sources in different formats (each requires mime-type)
 * (Outer code decides, if some video-format is supported by exact server)
 * 2. Detects browser and operating system
 * 3. Chooses best possible video-source for this browser
 * 4. If there is now such format - he tries to detect possible plugin for rtsp and use it
 *
 * This plugin hides details from outer code either.
 *
 * API:
 * update playing time handler
 * change source
 * play/pause
 * seekPosition - later
 * playingSpeed - later
 *
 */
angular.module('nxCommon')
    .directive('previewImage', ['$interval','$timeout','animateScope', '$sce', '$log', function ($interval,$timeout,animateScope, $sce, $log) {
        return {
            restrict: 'A',
            scope: {
                ngSrc: '@',
                errorLoading: '='
            },

            link: function (scope, element/*, attrs*/) {
                var $element = $(element);
                scope.$watch('ngSrc',function(){
                    element.addClass("hidden");
                });

                element.on('load', function (event) {
                    element.removeClass("hidden");
                });
                element.on('error', function (event) {
                    scope.errorLoading = true;
                    element.addClass("hidden");
                });
            }
        }
    }]);