'use strict';

describe('Controller: SettingsCtrl', function () {

    // load the controller's module
    beforeEach(module('webadminApp'));

    var SettingsCtrl,
        scope,
        http;

    // Initialize the controller and a mock scope
    beforeEach(inject(function ($controller, $rootScope, $httpBackend) {
        http = $httpBackend;

        var response = {status: 'OK'};
        http.whenGET('/ec2/settings').respond(response);

        scope = $rootScope.$new();
        SettingsCtrl = $controller('SettingsCtrl', {
            $scope: scope
        });
    }));

    afterEach(function () {
        http.verifyNoOutstandingExpectation();
        http.verifyNoOutstandingRequest();
    });

    it('makes request to restart the server', function () {
        var response = {status: 'OK'};
        http.whenPOST('/ec2/restartServer').respond(response);

        http.expectGET('/ec2/settings');
        scope.restart();
        http.expectPOST('/ec2/restartServer');
        http.flush();
    });
});
