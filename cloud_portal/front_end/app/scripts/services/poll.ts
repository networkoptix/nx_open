(function () {

    'use strict';

    /**
     This is a polling factory, which works similar to $interval except it waits for function promise to be resolved

     Check this: https://docs.angularjs.org/api/ng/service/$interval

     fn must return a promise!
     If fn return abortable promise like $http requests - abort function will be called as well

     It uses $interval inside to trick Protractor's waitForAngular
     */

    angular.module('cloudApp')
        .factory('$poll', [ '$q', '$interval', function ($q, $interval) {

            function poll(fn, delay) {

                let deferred = $q.defer();
                let promise = deferred.promise;

                let activeFnPromise = null;
                let activeInterval = null;
                let cancelledPoll = false;

                promise.cancel = function () {
                    this.cancelPoll();
                };

                function runPoll() {
                    activeInterval = $interval(() => {
                        activeFnPromise = fn(); // Call function
                        activeFnPromise.finally((result) => {
                            activeFnPromise = null;
                            deferred.notify(result);
                            if (!cancelledPoll) {
                                runPoll();
                            }
                        });
                    }, delay, 1); // Call interval to be executed only once
                }


                function cancelPoll() {
                    cancelledPoll = true; // Stop triggering poll again
                    $interval.cancel(activeInterval); // Not sure if we need this
                    if (activeFnPromise && activeFnPromise.abort) {  // fn returned $http-like promise - abort the request
                        activeFnPromise.abort();
                    }
                    deferred.reject('cancelled');
                }

                promise.abort = cancelPoll;

                //$scope.$on('$destroy', function( event ) { stopTimeout = true; } );

                runPoll();
                return promise;
            }

            // poll.cancel = function (promise) {
            //     promise.abort();
            // };


            return poll;
        } ]);
})();
