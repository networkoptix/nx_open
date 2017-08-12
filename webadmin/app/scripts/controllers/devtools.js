'use strict';

angular.module('webadminApp')
    .controller('DevtoolsCtrl', ['$scope', '$sessionStorage',
    function ($scope, $sessionStorage) {

        $scope.Config = Config;
        $scope.session = $sessionStorage;

        if($scope.Config.developersFeedbackForm){
            $scope.developersFeedbackForm =
                $scope.Config.developersFeedbackForm.replace("{{PRODUCT}}",encodeURIComponent(Config.productName));
        }
    }]);
