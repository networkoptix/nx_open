'use strict';


angular.module('cloudApp')
    .controller('DownloadCtrl', ['$scope', '$routeParams','$location', function ($scope, $routeParams, $location) {

        $scope.downloads = DownloadsConfig;

        var platformMatch = {
            'Open BSD': 'Linux',
            'Sun OS': 'Linux',
            'QNX': 'Linux',
            'UNIX': 'Linux',
            'BeOS': 'Linux',
            'OS/2': 'Linux',

            'Mac OS X': 'MacOS',
            'Mac OS': 'MacOS'
        }

        _.each($scope.downloads.platforms,function(platform){
            platform.active = (platform.os || platform.name) === ($routeParams.platform || platformMatch[window.jscd.os] || window.jscd.os);
        });

        $scope.changeHash = function(platform){
            var addHash = (platform.os || platform.name);
            $location.path("/download/" + addHash, false);
        }
    }]);
