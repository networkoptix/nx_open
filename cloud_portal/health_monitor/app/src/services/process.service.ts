import { Injectable } from '@angular/core';
import { NxLanguageProviderService } from './nx-language-provider';
import { NxToastService } from '../dialogs/toast.service';
import { NxCloudApiService } from './nx-cloud-api';
import { NxConfigService } from './nx-config';
import { NxSessionService } from './session.service';

interface ProcessSettings {
    errorCodes: any;
    errorMessage: string;
    errorPrefix: string;
    holdAlerts: boolean;
    ignoreUnauthorized: boolean;
    logoutForbidden: boolean;
    successMessage: string;
}


class Process {
    CONFIG: any;
    LANG: any;
    cloudApiService: any;
    sessionService: any;
    toastService: any;

    caller: any;
    settings: ProcessSettings;
    deferredPromise: any;

    /* process info */
    success: boolean;
    error: boolean;
    processing: boolean;
    finished: boolean;
    errorData: any;

    /* process handlers */
    successHandler: any;
    errorHandler: any;


    constructor(CONFIG, LANG, sessionService, cloudApiService, toastService, caller, settings) {
        this.CONFIG = CONFIG;
        this.LANG = LANG;
        this.cloudApiService = cloudApiService;
        this.sessionService = sessionService;
        this.toastService = toastService;
        this.init(caller, settings);
        return this;
    }

    init(caller, settings) {
        /*
        settings: {
            errorCodes,

            holdAlerts
            successMessage,
            errorPrefix,
        }
        settings.successMessage
         */
        if (settings) {
            settings.errorPrefix = settings.errorPrefix ? `${settings.errorPrefix}: ` : '';
            this.settings = {... this.settings, ... settings};
        } else {
            this.settings = {
                errorCodes: {},
                errorMessage: '',
                errorPrefix: '',
                holdAlerts: false,
                ignoreUnauthorized: false,
                logoutForbidden: false,
                successMessage: '',
            };
        }
        this.caller = caller;
    }

    run() {
        this.processing = true;
        this.error = false;
        this.success = false;
        this.finished = false;

        this.deferredPromise = this.createDeferredPromise();
        this.deferredPromise.promise.then(this.successHandler, this.errorHandler);

        /* There is a weird issue when executing a process that is passed into a modal.
         * After the first execution then caller function becomes undefined when the run
         * returns this.caller(). Wrapping it in a promise fixes the issue.
         */
        const wrapper = new Promise((resolve) => {
            return resolve(this.caller());
        });
        return wrapper.then((data) => {
            const error = this.cloudApiService.checkResponseHasError(data);
            if (error) {
                this.handleError(error);
            } else {
                this.success = true;
                if (this.settings.successMessage && data !== false) {
                    // nxDialogsService.notify(successMessage, 'success', holdAlerts);
                    // Circular dependencies ... keep ngToast for no -- TT
                    const options = {
                        classname: 'success',
                        autohide: !this.settings.holdAlerts,
                        delay: this.CONFIG.alertTimeout
                    };
                    this.toastService.show(this.settings.successMessage, options);
                }
                this.deferredPromise.resolve(data);
            }
            return;
        }, (error) => {
            if (error && error.error) {
                error = error.error;
            }
            this.handleError(error);
        }).finally(() => {
            this.processing = false;
            this.finished = true;
        });
    }

    then(successHandler, errorHandler?) {
        this.successHandler = successHandler;
        this.errorHandler = errorHandler || (() => {});
        return this;
    }

    private createDeferredPromise() {
        return (() => {
            let resolve;
            let reject;

            const p = new Promise((res, rej) => {
                resolve = res;
                reject = rej;
            });

            return {
                promise: p,
                reject,
                resolve
            };
        })();
    }

    private formatError(error, errorCodes) {
        const errorCode = error && error.data && error.data.resultCode || error && error.resultCode || error;
        if (!errorCode) {
            return this.LANG.errorCodes.unknownError;
        }
        if (errorCodes && typeof (errorCodes[errorCode]) !== 'undefined') {
            if (typeof(errorCodes[errorCode]) === 'function') {
                const result = (errorCodes[errorCode])(error) || false;
                if (result !== true) {
                    return result;
                }
            } else {
                return errorCodes[errorCode];
            }
        }
        return this.LANG.errorCodes[errorCode] || this.LANG.errorCodes.unknownError;
    }

    private handleError(data) {
        this.error = true;
        this.errorData = data;
        if (!this.settings.ignoreUnauthorized && data &&
            (data.detail ||
                // detail appears only when django rest framework declines request with
                // {"detail":"Authentication credentials were not provided."}
                // we need to handle this like user was not authorised
                data.resultCode === 'notAuthorized' ||
                data.resultCode === 'forbidden' && this.settings.logoutForbidden)) {
            this.sessionService.invalidateSession();
            this.deferredPromise.reject(data);
            return;
        }
        const formatted = this.formatError(data, this.settings.errorCodes);
        if (formatted !== false) {
            this.settings.errorMessage = formatted;
            const message = `${this.settings.errorPrefix} ${this.settings.errorMessage}`;
            const options = {
                autohide: !this.settings.holdAlerts,
                classname: 'danger',
                delay: this.CONFIG.alertTimeout
            };
            this.toastService.show(message, options);
        }
        this.deferredPromise.reject(data);
    }
}


@Injectable({
    providedIn: 'root'
})
export class NxProcessService {
    CONFIG: any;
    LANG: any;
    constructor(private configService: NxConfigService,
                private languageService: NxLanguageProviderService,
                private cloudApiService: NxCloudApiService,
                private sessionService: NxSessionService,
                private toastService: NxToastService) {
        this.CONFIG = this.configService.getConfig();
        this.LANG = this.languageService.getTranslations();
    }

    createProcess(caller, settings?) {
        return new Process(this.CONFIG, this.LANG, this.sessionService, this.cloudApiService, this.toastService, caller, settings);
    }
}
