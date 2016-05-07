'use strict';


angular.module('cloudApp')
    .factory('process', ['$q', 'dialogs', function ($q, dialogs) {

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
                var errorCodes = null;
                var errorPrefix = '';
                var holdAlerts = false;
                var successMessage = null;
                if(settings){
                    errorCodes = settings.errorCodes;
                    holdAlerts = settings.holdAlerts;
                    successMessage = settings.successMessage;

                    errorPrefix = settings.errorPrefix? (settings.errorPrefix + ' ') : '';
                }
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
                            var formatted = formatError(data.data, errorCodes);
                            if(formatted !== false){
                                self.errorMessage = formatted;
                                // Error handler here
                                dialogs.notify(errorPrefix + self.errorMessage, 'danger', holdAlerts);
                            }
                            deferred.reject(data);
                        }
                        return caller().then(function(data){
                            self.processing = false;
                            self.finished = true;
                            if(data.data.resultCode && data.data.resultCode != L.errorCodes.ok){
                                handleError(data);
                            }else {
                                self.success = true;

                                if(successMessage){
                                    dialogs.notify(successMessage, 'success', holdAlerts);
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
    }]);
