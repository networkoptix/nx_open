'use strict';

angular.module('webadminApp')
    .controller('HealthReportCtrl', ['$scope', '$location', 'camerasProvider', 'systemAPI', '$q', '$poll',
    function ($scope, $location, camerasProvider, systemAPI, $q) {
        $scope.Config = Config;

        function requestHealthManifest(){
            return systemAPI.getHealthManifest().then(function (data) {
                return data.data.reply;
            });
        }

        function requestHealthValues(){
            return systemAPI.getHealthValues().then(function (data) {
                return data.data.reply;
            });
        }

        function retriveData() {
            return $q.all([requestHealthManifest(), requestHealthValues()]).then(function (result) {
                $scope.manifest = result[0];
                $scope.values = result[1];
            });
        }

        retriveData();
    }]);
