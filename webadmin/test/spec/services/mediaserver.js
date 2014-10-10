'use strict';

describe('Service: mediaserver', function () {

    // load the service's module
    beforeEach(module('webadminApp'));

    // instantiate service
    var mediaserver;
    beforeEach(inject(function (_mediaserver_) {
        mediaserver = _mediaserver_;
    }));

    it('should do something', function () {
        expect(!!mediaserver).toBe(true);
    });

});
