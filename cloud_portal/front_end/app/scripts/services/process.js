'use strict';


angular.module('cloudApp')
    .factory('process', ['$q', 'dialogs', 'cloudApi', 'account', function ($q, dialogs, cloudApi, account) {

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

                    errorPrefix = settings.errorPrefix? (settings.errorPrefix + ': ') : '';
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

                            if(!settings.ignoreUnauthorized && data &&
                                    data.data &&
                                    (data.data.detail ||
                                        // detail appears only when django rest framewrok declines request with
                                        // {"detail":"Authentication credentials were not provided."}
                                        // we need to handle this like user was not authorised
                                    data.data.resultCode == 'notAuthorized' ||
                                    data.data.resultCode =='forbidden' && settings.logoutForbidden)){
                                account.logout();
                                deferred.reject(data);
                                return;
                            }

                            var formatted = formatError(data ? data.data : data, errorCodes);
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

                            var error = false;
                            if(error = cloudApi.checkResponseHasError(data)){
                                handleError(error);
                            } else {
                                self.success = true;

                                if(successMessage && data !== false){
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
