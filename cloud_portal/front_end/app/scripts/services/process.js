'use strict';


angular.module('cloudApp')
    .factory('process', function ($q) {

        function formatError(error,errorCodes){
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
            return Config.errorCodes[error.resultCode] || Config.errorCodes.unknownError;
        }

        return {
            formatError:formatError,
            init:function(caller, errorCodes){
                return {
                    success:false,
                    error:false,
                    processing: false,
                    errorData: null,
                    then:function(successHanlder, errorHandler, processHandler){
                        this.successHanlder = successHanlder;
                        this.errorHandler = errorHandler;
                        this.processHandler = processHandler;
                        return this;
                    },
                    run:function(){
                        this.processing = true;
                        var self = this;

                        var deferred = $q.defer();
                        deferred.promise.then(
                            this.successHanlder,
                            this.errorHandler,
                            this.processHandler
                        );

                        return caller().then(function(data){
                            self.processing = false;
                            if(data.data.resultCode && data.data.resultCode != Config.errorCodes.ok){
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
