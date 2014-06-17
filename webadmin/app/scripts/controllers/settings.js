'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver) {
        $scope.settings = mediaserver.settings.get();
        $scope.confirmPassword = '';

        $scope.openJoinDialog = function () {
            var modalInstance = $modal.open({
                templateUrl: 'views/join.html',
                controller: 'JoinCtrl',
                resolve: {
                    items: function () {
                        return $scope.settings;
                    }
                }
            });

            modalInstance.result.then(function (settings) {
                $log.info(settings);
            }, function () {
                $log.info('Modal dismissed at: ' + new Date());
            });
        };

        $scope.save = function () {
            mediaserver.settings.save($scope.settings);
        };

        $scope.restart = function () {
            mediaserver.restart();
        };
    });
