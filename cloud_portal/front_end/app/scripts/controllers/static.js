'use strict';

angular.module('cloudApp')
    .controller('StaticCtrl', ['$scope', 'cloudApi', '$routeParams', 'nxTitle',
        function($scope, cloudApi, $routeParams, nxTitle) {
            
            var page = $routeParams.page;
    
            nxTitle.setTitle(page.charAt(0).toUpperCase() + page.slice(1));
            
            $scope.page_url = Config.viewsDir + 'static/' + encodeURIComponent(page) + '.html';
            $scope.$on('$includeContentError', function () {
                $scope.notFound = true;
            })
        }]);
