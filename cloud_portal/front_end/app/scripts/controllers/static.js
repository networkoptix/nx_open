'use strict';

angular.module('cloudApp')
    .controller('StaticCtrl', ['$scope', 'cloudApi', '$routeParams', 'nxPageService',
        function($scope, cloudApi, $routeParams, nxPageService) {
            
            var page = $routeParams.page;
    
            nxPageService.setPageTitle(page.charAt(0).toUpperCase() + page.slice(1));
            
            $scope.page_url = Config.viewsDir + 'static/' + encodeURIComponent(page) + '.html';
            $scope.$on('$includeContentError', function () {
                $scope.notFound = true;
            })
        }]);
