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
                        return installer.platform == existingInstaller.platform &&
                               installer.appType == existingInstaller.appType;
                    });

                    if(targetInstaller){
                        _.extend(installer, targetInstaller);
                        installer.formatName = L.downloads.platforms[installer.platform] + ' - ' + L.downloads.appTypes[installer.appType];
                        installer.url = $scope.downloadsData.releaseUrl + installer.path;
                    }
                    return !!targetInstaller;
                });
            });
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

        var activeOs = $routeParams.platform || platformMatch[window.jscd.os] || window.jscd.os;

        var foundPlatform = false;
        _.each($scope.downloads.groups,function(platform){
            platform.active = (platform.os || platform.name) === activeOs;
            foundPlatform = platform.active || foundPlatform;
        });


        for(var mobile in Config.downloads.mobile){
            if(Config.downloads.mobile[mobile].os === activeOs){
                if(L.downloads.mobile[Config.downloads.mobile[mobile].name].link != 'disabled'){
                    window.location.href = L.downloads.mobile[Config.downloads.mobile[mobile].name].link;
                    return;
                }
                break;
            }
        }

        if($routeParams.platform && !foundPlatform){
            $location.path("404"); // Can't find this page user is looking for
            return;
        }
        if(!foundPlatform){
            $scope.downloads.groups[0] = true;
        }


        $scope.changeHash = function(platform){
            var addHash = (platform.os || platform.name);

            page.title(L.pageTitles.downloadPlatform + platform.name);

            $location.path('/download/' + addHash, false);
        }
    }]);
