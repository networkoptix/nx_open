'use strict';

angular.module('cloudApp').directive(
         'setTitle',
         ['$rootScope', '$timeout', 'page', function ($rootScope, $timeout, page) {
             return {
                 restrict: 'EA',
                 link: function (scope, iElement, iAttrs, controller, transcludeFn) {
                     // If we've been inserted as an element then we detach from the DOM because the caller
                     // doesn't want us to have any visual impact in the document.
                     // Otherwise, we're piggy-backing on an existing element so we'll just leave it alone.
                     var tagName = iElement[0].tagName.toLowerCase();
                     if (tagName === 'view-title' || tagName === 'viewtitle') {
                         iElement.remove();
                     }

                     scope.$watch(
                         function () {
                             return iElement.text();
                         },
                         function (newTitle) {
                            page.title(newTitle);
                         }
                     );
                 }
             };
         }]
     );