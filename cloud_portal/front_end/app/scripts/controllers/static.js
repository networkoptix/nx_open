'use strict';


angular.module('cloudApp')
    .controller('StaticCtrl', ['$scope','cloudApi','$routeParams', function ($scope,cloudApi,$routeParams) {
        var page = $routeParams.page;

        $scope.page_url = Config.viewsDir + 'static/' + encodeURIComponent(page) + '.html';
        $scope.$on('$includeContentError',function(){
            $scope.notFound = true;
        })
    }]);