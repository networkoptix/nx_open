'use strict';

angular.module('webadminApp')
    .controller('JoinCtrl', function ($scope, $modalInstance, $interval, mediaserver) {
        $scope.url = '';
        $scope.password = '';

        $scope.testProgressBarVisible = false;
        $scope.testProgress = 0;

        $scope.test = function () {
            $scope.testProgress = 0;

            mediaserver.ping.get();
            var progress = $interval(function () {
                if ($scope.testProgress >= 100) {
                    $scope.testProgressBarVisible = false;
                    $interval.cancel(progress);
                    console.log('done');
                    return;
                }

                $scope.testProgressBarVisible = true;
                $scope.testProgress++;
            }, 100);
        };

        $scope.join = function () {
            $modalInstance.close({url: $scope.url, password: $scope.password});
        };

        $scope.cancel = function () {
            $modalInstance.dismiss('cancel');
        };
    });
