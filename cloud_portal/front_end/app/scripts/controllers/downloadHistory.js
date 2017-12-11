'use strict';


angular.module('cloudApp')
    .controller('DownloadHistoryCtrl', ['$scope', '$routeParams', '$location', 'page', 'cloudApi', 'account',
    function ($scope, $routeParams, $location, page, cloudApi, account) {

        account.requireLogin();
        $scope.downloads = Config.downloads;

        cloudApi.getDownloadsHistory($routeParams.build).then(function(result){
            $scope.linkbase = result.data.updatesPrefix;

            if(!$routeParams.build){ // only one build
                $scope.activeBuilds = result.data.releases;
                $scope.downloadTypes=["releases", "patches", "betas"];
                $scope.downloadsData = result.data;
            }else{
                $scope.activeBuilds = [result.data];
                $scope.downloadTypes = [result.data.type]
                $scope.downloadsData = {}
                $scope.downloadsData[result.data.type] = $scope.activeBuilds;
            }
        },function(error){
             $location.path("404"); // Can't find downloads.json in specific build
        });

        $scope.changeType = function (type){
            $scope.activeBuilds = $scope.downloadsData[type];
        }
    }]);
