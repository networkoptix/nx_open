(function () {

    "use strict";

    angular
        .module('cloudApp')
        .controller('StartPageCtrl', StartPage);

    StartPage.$inject = [ '$scope', 'cloudApi', '$location', '$routeParams',
        'dialogs', 'account', 'authorizationCheckService' ];

    function StartPage($scope, cloudApi, $location, $routeParams,
                       dialogs, account, authorizationCheckService) {

        if ($routeParams.callLogin) {
            dialogs.login(false);
        } else {
            authorizationCheckService.redirectAuthorised();
            $scope.userEmail = account.getEmail();
        }
    }
})();
