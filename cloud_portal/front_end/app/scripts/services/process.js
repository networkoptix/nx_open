'use strict';


angular.module('cloudApp')
    .factory('process', function ($q) {

        function formatError(error,errorCodes){
            if(!error || !error.resultCode){
                return L.errorCodes.unknownError;
            }
            if(errorCodes && typeof(errorCodes[error.resultCode]) != 'undefined'){
                if($.isFunction(errorCodes[error.resultCode])){
                    var result = errorCodes[error.resultCode](error);
                    if(result !== true){
                        return result;
                    }
                }else {
                    return errorCodes[error.resultCode];
                }
            }
            return L.errorCodes[error.resultCode] || L.errorCodes.unknownError;
        }

        return {
            formatError:formatError,
            init:function(caller, errorCodes){
                return {
                    success:false,
                    error:false,
                    processing: false,
                    finished: false,
                    errorData: null,
                    then:function(successHanlder, errorHandler, processHandler){
                        this.successHanlder = successHanlder;
                        this.errorHandler = errorHandler;
                        this.processHandler = processHandler;
                        return this;
                    },
                    run:function(){
                        this.processing = true;
                        this.error = false;
                        this.success = false;
                        this.finished = false;
                        var self = this;

                        var deferred = $q.defer();
                        deferred.promise.then(
                            this.successHanlder,
                            this.errorHandler,
                            this.processHandler
                        );

                        return caller().then(function(data){
                            self.processing = false;
                            self.finished = true;
                            if(data.data.resultCode && data.data.resultCode != L.errorCodes.ok){
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
                            self.finished = true;
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
