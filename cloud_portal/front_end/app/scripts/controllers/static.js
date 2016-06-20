'use strict';


angular.module('cloudApp')
    .controller('StaticCtrl', ['$scope','cloudApi','$routeParams', function ($scope,cloudApi,$routeParams) {
        var page = $routeParams.page;

        $scope.page_url = 'static/views/static/' + page + '.html';

    }]);