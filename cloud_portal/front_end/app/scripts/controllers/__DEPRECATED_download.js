(function () {

    'use strict';

    angular
        .module('cloudApp.controllers')
        .controller('DownloadCtrl', [ '$scope', '$routeParams', '$location',
            'page', 'cloudApi', 'configService', 'languageService',

            function ($scope, $routeParams, $location, page, cloudApi, configService, languageService) {

                $scope.downloads = configService.config.downloads;

                cloudApi.getDownloads().then(function (data) {
                    $scope.downloadsData = data.data;

                    angular.forEach($scope.downloads.groups, function (platform) {
                        platform.installers = _.filter(platform.installers, function (installer) {
                            var targetInstaller = _.find($scope.downloadsData.installers, function (existingInstaller) {
                                return installer.platform === existingInstaller.platform &&
                                    installer.appType === existingInstaller.appType;
                            });

                            if (targetInstaller) {
                                _.extend(installer, targetInstaller);
                                installer.formatName = languageService.lang.downloads.platforms[ installer.platform ] + ' - ' + languageService.lang.downloads.appTypes[ installer.appType ];
                                installer.url = $scope.downloadsData.releaseUrl + installer.path;
                            }
                            return !!targetInstaller;
                        });
                    });
                });


                var platformMatch = {
                    'Open BSD': 'Linux',
                    'Sun OS'  : 'Linux',
                    'QNX'     : 'Linux',
                    'UNIX'    : 'Linux',
                    'BeOS'    : 'Linux',
                    'OS/2'    : 'Linux',

                    'Mac OS X': 'MacOS',
                    'Mac OS'  : 'MacOS'
                };

                var activeOs = $routeParams.platform || platformMatch[ window.jscd.os ] || window.jscd.os;

                var foundPlatform = false;
                angular.forEach($scope.downloads.groups, function (platform) {
                    platform.active = (platform.os || platform.name) === activeOs;
                    foundPlatform = platform.active || foundPlatform;
                });


                for (var mobile in $scope.downloads.mobile) {
                    if ($scope.downloads.mobile[ mobile ].os === activeOs) {
                        if (languageService.lang.downloads.mobile[ $scope.downloads.mobile[ mobile ].name ].link !== 'disabled') {
                            window.location.href = languageService.lang.downloads.mobile[ $scope.downloads.mobile[ mobile ].name ].link;
                            return;
                        }
                        break;
                    }
                }

                if ($routeParams.platform && !foundPlatform) {
                    $location.path("404"); // Can't find this page user is looking for
                    return;
                }
                if (!foundPlatform) {
                    $scope.downloads.groups[ 0 ].active = true;
                }


                $scope.changeHash = function (platform) {
                    var addHash = (platform.os || platform.name);

                    page.title(languageService.lang.pageTitles.downloadPlatform + platform.name);

                    $location.path('/download/' + addHash, false);
                }
            } ]);
})();
