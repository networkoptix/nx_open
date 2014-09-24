'use strict';

describe('Controller: JoinCtrl', function () {

    // load the controller's module
    beforeEach(module('webadminApp'));

    var JoinCtrl,
        http,
        scope,
        modalInstance;

    // Initialize the controller and a mock scope
    beforeEach(inject(function ($controller, $rootScope, $httpBackend) {
        http = $httpBackend;
        var response = {status: 'OK'};
        http.whenGET('/ec2/ping').respond(response);
        scope = $rootScope.$new();

        modalInstance = {                    // Create a mock object using spies
            close: jasmine.createSpy('modalInstance.close'),
            dismiss: jasmine.createSpy('modalInstance.dismiss'),
            result: {
                then: jasmine.createSpy('modalInstance.result.then')
            }
        };
        JoinCtrl = $controller('JoinCtrl', {
            $scope: scope,
            $modalInstance: modalInstance
        });
    }));

    afterEach(function () {
        http.verifyNoOutstandingExpectation();
        http.verifyNoOutstandingRequest();
    });

    describe('Initial state', function () {
        it('should instantiate the controller properly', function () {
            expect(JoinCtrl).not.toBeUndefined();
        });

//        it('should close the modal with result "true" when accepted', function () {
//            scope.accept();
//            expect(modalInstance.close).toHaveBeenCalledWith(true);
//        });
//
//        it('should close the modal with result "false" when rejected', function () {
//            scope.reject();
//            expect(modalInstance.close).toHaveBeenCalledWith(false);
//        });
    });

    it('makes request to api to validate credentials', function () {
        scope.test();
        http.expectGET('/ec2/ping');
        http.flush();
    });
});
