'use strict';


angular.module('cloudApp')
    .controller('DownloadCtrl', ['$scope', '$routeParams', function ($scope, $routeParams) {

        $scope.downloads = DownloadsConfig;

    }]);