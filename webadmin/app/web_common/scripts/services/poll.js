'use strict';
/**
 This is a polling factory, which works similar to $interval except it waits for function promise to be resolved

 Check this: https://docs.angularjs.org/api/ng/service/$interval

 fn must return a promise!
 If fn return abortable promise like $http requests - abort function will be called as well

 It uses $interval inside to trick Protractor's waitForAngular
 */

angular.module('nxCommon')
    .factory('$poll', ['$q', '$interval', function ($q, $interval) {

        function poll(fn, delay, initialDelay){
            var deferred = $q.defer();
            var promise = deferred.promise;

            var activeFnPromise = null;
            var activeInterval = null;
            var cancelledPoll = false;
            initialDelay = initialDelay || 0;

            function runPoll(){
                activeInterval = $interval(function(){
                    initialDelay = 0;
                    activeFnPromise = fn(); // Call function
                    activeFnPromise.finally(function(result){
                        activeFnPromise = null;
                        deferred.notify(result);
                        if(!cancelledPoll){
                            runPoll();
                        }
                    });
                }, delay + initialDelay, 1); // Call interval to be executed only once
            }


            function cancelPoll(){
                cancelledPoll = true; // Stop triggering poll again
                $interval.cancel(activeInterval); // Not sure if we need this
                if(activeFnPromise && activeFnPromise.abort){  // fn returned $http-like promise - abort the request
                    activeFnPromise.abort();
                }
                deferred.reject('cancelled');
            }

            promise.abort = cancelPoll;

            //$scope.$on("$destroy", function( event ) { stopTimeout = true; } );

            runPoll();
            return promise;
        }
        poll.cancel = function( promise ){
            if(promise){
                promise.abort();
            }
        };


        return poll;
    }]);