'use strict';

angular.module('webadminApp')
    .controller('SettingsCtrl', function ($scope, $modal, $log, mediaserver) {
        $scope.settings = mediaserver.getSettings();

        $scope.settings.then(function (r) {
            $scope.settings = {
                systemName: r.data.reply.systemName,
                port: r.data.reply.port};
        });

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
            mediaserver.saveSettings($scope.settings.systemName);
        };

        $scope.restart = function () {
            mediaserver.restart();
        };
    });
