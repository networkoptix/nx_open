'use strict';


angular.module('cloudApp')
    .factory('process', function ($q) {

        function formatError(error,errorCodes){
            return (errorCodes && errorCodes[error.resultCode]) || Config.errorCodes[error.resultCode] || Config.errorCodes.unknownError;
        }

        return {
            formatError:formatError,
            init:function(caller, errorCodes){
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

                            self.processing = false;

                            console.log("process success", data);

                            if(data.data.resultCode && data.data.resultCode != Config.errorCodes.ok){

                                console.log("process error", data);

                                self.error = true;
                                self.errorData = data;
                                self.errorMessage = formatError(data.data, errorCodes);

                                deferred.reject(data);

                            }else {
                                self.success = true;

                                deferred.resolve(data);
                            }
                        },function(error){

                            self.processing = false;
                            console.error("process error",error);
                            self.error = true;
                            self.errorData = error;
                            self.errorMessage = formatError(error.data, errorCodes);

                            deferred.reject(error);
                        },function(progress){
                            deferred.notify(progress);
                        });
                    }
                }
            }
        }
    });
