'use strict';


angular.module('cloudApp')
    .factory('process', function ($q, dialogs) {

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
            init:function(caller, settings){
                /*
                settings: {
                    errorCodes,

                    holdAlerts
                    successMessage,
                    errorPrefix,
                }
                settings.successMessage
                 */
                var errorCodes = settings.errorCodes;
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

                        function handleError(data){
                            self.processing = false;
                            self.finished = true;
                            self.error = true;
                            self.errorData = data;
                            self.errorMessage = formatError(data.data, errorCodes);
                            // Error handler here
                            var prefix = (settings.errorPrefix + ' ') || '';
                            dialogs.notify(prefix + self.errorMessage, 'danger', settings.holdAlerts);
                            deferred.reject(data);
                        }
                        return caller().then(function(data){
                            self.processing = false;
                            self.finished = true;
                            if(data.data.resultCode && data.data.resultCode != L.errorCodes.ok){
                                handleError(data);
                            }else {
                                self.success = true;

                                if(settings.successMessage){
                                    dialogs.notify(settings.successMessage, 'success', settings.holdAlerts);
                                }
                                deferred.resolve(data);
                            }
                        },function(error){
                            handleError(error);
                        },function(progress){
                            deferred.notify(progress);
                        });
                    }
                }
            }
        }
    });
