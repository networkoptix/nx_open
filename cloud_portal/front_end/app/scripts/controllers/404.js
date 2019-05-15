(function () {
    'use strict';
    
    angular.module('cloudApp')
        .controller('404Ctrl', ['$scope', 'cloudApi', '$routeParams', 'languageService', 'nxPageService',
            function ($scope, cloudApi, $routeParams, languageService, nxPageService) {
    
                nxPageService.setPageTitle(languageService.lang.pageTitles.pageNotFound);
            }]);
})();
