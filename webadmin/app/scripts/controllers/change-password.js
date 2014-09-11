'use strict';

angular.module('webadminApp')
    .controller('ChangePasswordCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.password = '';
        $scope.confirmPassword = '';

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };



        $scope.save = function () {
            $modalInstance.close({password: $scope.password});
        };

    });
