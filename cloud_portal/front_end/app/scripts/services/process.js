(function () {
    'use strict';
    angular
        .module('cloudApp')
        .factory('process', ['$q', 'ngToast', 'cloudApi', 'account', 'languageService',
        function ($q, ngToast, cloudApi, account, languageService) {
            let lang = languageService.lang;
            function formatError(error, errorCodes) {
                if (!error || !error.resultCode) {
                    return lang.errorCodes.unknownError;
                }
                if (errorCodes && typeof (errorCodes[error.resultCode]) != 'undefined') {
                    if (angular.isFunction(errorCodes[error.resultCode])) {
                        let result = (errorCodes[error.resultCode])(error) || false;
                        if (result !== true) {
                            return result;
                        }
                    }
                    else {
                        return errorCodes[error.resultCode];
                    }
                }
                return lang.errorCodes[error.resultCode] || lang.errorCodes.unknownError;
            }
            return {
                init: function (caller, settings) {
                    /*
                    settings: {
                        errorCodes,

                        holdAlerts
                        successMessage,
                        errorPrefix,
                    }
                    settings.successMessage
                     */
                    let errorCodes = null;
                    let errorPrefix = '';
                    let holdAlerts = false;
                    let successMessage = null;
                    if (settings) {
                        errorCodes = settings.errorCodes;
                        holdAlerts = settings.holdAlerts;
                        successMessage = settings.successMessage;
                        errorPrefix = settings.errorPrefix ? (settings.errorPrefix + ': ') : '';
                    }
                    return {
                        success: false,
                        error: false,
                        processing: false,
                        finished: false,
                        errorData: null,
                        then: function (successHanlder, errorHandler, processHandler) {
                            this.successHanlder = successHanlder;
                            this.errorHandler = errorHandler;
                            this.processHandler = processHandler;
                            return this;
                        },
                        run: function () {
                            let self = this;
                            this.processing = true;
                            this.error = false;
                            this.success = false;
                            this.finished = false;
                            let deferred = $q.defer();
                            deferred.promise.then(this.successHanlder, this.errorHandler, this.processHandler);
                            function handleError(data) {
                                self.processing = false;
                                self.finished = true;
                                self.error = true;
                                self.errorData = data;
                                if (!settings.ignoreUnauthorized && data &&
                                    data.data &&
                                    (data.data.detail ||
                                        // detail appears only when django rest framewrok declines request with
                                        // {"detail":"Authentication credentials were not provided."}
                                        // we need to handle this like user was not authorised
                                        data.data.resultCode == 'notAuthorized' ||
                                        data.data.resultCode == 'forbidden' && settings.logoutForbidden)) {
                                    account.logout();
                                    deferred.reject(data);
                                    return;
                                }
                                let formatted = formatError(data && data.data || data, errorCodes);
                                if (formatted !== false) {
                                    self.errorMessage = formatted;
                                    // Error handler here
                                    // Circular dependencies ... keep ngToast for no -- TT
                                    // nxDialogsService.notify(errorPrefix + self.errorMessage, 'danger', holdAlerts);
                                    ngToast.create({
                                        className: 'danger',
                                        content: errorPrefix + self.errorMessage,
                                        dismissOnTimeout: !holdAlerts,
                                        dismissOnClick: !holdAlerts,
                                        dismissButton: holdAlerts
                                    });
                                }
                                deferred.reject(data);
                            }
                            return caller().then(function (data) {
                                self.processing = false;
                                self.finished = true;
                                const error = cloudApi.checkResponseHasError(data);
                                if (error) {
                                    handleError(error);
                                }
                                else {
                                    self.success = true;
                                    if (successMessage && data !== false) {
                                        // nxDialogsService.notify(successMessage, 'success', holdAlerts);
                                        // Circular dependencies ... keep ngToast for no -- TT
                                        ngToast.create({
                                            className: 'success',
                                            content: successMessage,
                                            dismissOnTimeout: !holdAlerts,
                                            dismissOnClick: !holdAlerts,
                                            dismissButton: holdAlerts
                                        });
                                    }
                                    deferred.resolve(data);
                                }
                            }, function (error) {
                                handleError(error);
                            }, function (progress) {
                                deferred.notify(progress);
                            });
                        }
                    };
                }
            };
        }]);
})();
//# sourceMappingURL=process.js.map