'use strict';


angular.module('cloudApp')
    .controller('DownloadCtrl', ['$scope', '$routeParams', '$location', 'page', 'cloudApi',
    function ($scope, $routeParams, $location, page, cloudApi) {

        $scope.downloads = Config.downloads;

        cloudApi.getDownloads().then(function(data){
            $scope.downloadsData = data.data;

            _.each($scope.downloads.groups,function(platform){
                platform.installers = _.filter(platform.installers,function(installer){
                    var targetInstaller = _.find($scope.downloadsData.installers,function(existingInstaller){

                        console.log("ti",installer.platform,installer.appType,existingInstaller.platform,existingInstaller.appType);
                        return installer.platform == existingInstaller.platform &&
                               installer.appType == existingInstaller.appType;
                    });

                    if(targetInstaller){
                        _.extend(installer, targetInstaller);
                        installer.formatName = L.downloads.platforms[installer.platform] + ' - ' + L.downloads.appTypes[installer.appType];
                    }
                    return !!targetInstaller;
                });
            });


            console.log($scope.downloadsData, $scope.downloads);
        });


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

        var activeOs = $routeParams.platform ||platformMatch[window.jscd.os] || window.jscd.os;
        _.each($scope.downloads.groups,function(platform){
            platform.active = (platform.os || platform.name) === activeOs;
        });


        for(var mobile in Config.downloads.mobile){
            if(Config.downloads.mobile[mobile].os === activeOs){
                window.location.href = L.downloads.mobile[Config.downloads.mobile[mobile].name];
                break;
            }
        }

        $scope.changeHash = function(platform){
            var addHash = (platform.os || platform.name);

            page.title(L.pageTitles.downloadPlatform + platform.name);

            $location.path('/download/' + addHash, false);
        }
    }]);
