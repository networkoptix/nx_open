'use strict';


angular.module('cloudApp')
    .factory('process', function ($q) {
        return {
            init:function(caller){
                var deferred = $q.defer();

                return {
                    success:false,
                    error:false,
                    processing: false,
                    errorData: null,
                    promise: deferred.promise,
                    run:function(){
                        console.log("run process",this);
                        this.processing = true;
                        var self = this;
                        return caller().then(function(data){
                            self.success = true;
                            self.processing = false;
                            deferred.resolve(data);
                        },function(error){
                            console.error("process error",error);
                            self.error = true;
                            self.errorData = error;
                            self.errorMessage = error.data.errorText;
                            self.processing = false;

                            deferred.reject(error);
                        },function(progress){
                            deferred.notify(progress);
                        });
                    }
                }
            }
        }
    });
