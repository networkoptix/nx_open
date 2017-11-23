'use strict';


angular.module('cloudApp')
    .controller('DownloadHistoryCtrl', ['$scope', '$routeParams', '$location', 'page', 'cloudApi', 'account',
    function ($scope, $routeParams, $location, page, cloudApi, account) {

        account.requireLogin();
        $scope.downloads = Config.downloads;
        $scope.linkbase = "http://updates.networkoptix.com";

        cloudApi.getDownloadsHistory($routeParams.build).then(function(data){
            $scope.downloadsData = data.data;

            if(!$routeParams.build){ // only one build
                $scope.activeBuilds = data.data.releases;
                $scope.downloadTypes=["releases","patches","betas"];
            }else{
                $scope.activeBuilds = [data.data];
                $scope.downloadTypes = [data.data.type]
            }
        });

        $scope.changeType = function (type){
            $scope.activeBuilds = $scope.downloadsData[type];
        }
    }]);
