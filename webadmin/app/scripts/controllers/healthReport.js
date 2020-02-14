'use strict';

angular.module('webadminApp')
    .controller('HealthReportCtrl', ['$scope', 'mediaserver',
    function ($scope, mediaserver) {
        $scope.Config = Config;
        $scope.ready = false;
        mediaserver.getModuleInformation().then(function (r) {
            $scope.ready = true;
        });
    }]);
