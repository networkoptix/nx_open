"use strict";

angular.module("cloudApp")
    .controller("StartPageCtrl", ["$scope", "$routeParams", "dialogs", "account", function ($scope, $routeParams, dialogs, account) {
        account.redirectAuthorised();
        if (window.jscd.browser === "Safari" && window.jscd.browserMajorVersion < 10) {
            alert(L.errorCodes.oldSafariNotSupported);
        }
        $scope.userEmail = account.getEmail();

        if ($routeParams.callLogin) {
            dialogs.login();
        }
    }]);
