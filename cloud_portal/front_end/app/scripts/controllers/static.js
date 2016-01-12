'use strict';


angular.module('cloudApp')
    .controller('StaticCtrl', function ($scope,cloudApi,$routeParams) {
        var page = $routeParams.page;

        $scope.page_url = 'views/static/' + page + '.html';

    });