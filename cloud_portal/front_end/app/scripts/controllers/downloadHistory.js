'use strict';


angular.module('cloudApp')
    .controller('DownloadHistoryCtrl', ['$scope', '$routeParams', '$location', 'page', 'cloudApi',
    function ($scope, $routeParams, $location, page, cloudApi) {

        $scope.downloads = Config.downloads;
        $scope.linkbase = "http://updates.networkoptix.com";
        $scope.downloadTypes=["releases","patches","beta"];

        cloudApi.getDownloadsHistory($routeParams.build).then(function(data){
            $scope.downloadsData = data.data;

            if(!$routeParams.build){ // only one build
                $scope.activeBuilds = data.data.releases;
            }else{
                $scope.activeBuilds = [data.data];
                $scope.downloadTypes = [data.data.type]
            }
            console.log($scope.activeBuilds);
        });
    }]);
