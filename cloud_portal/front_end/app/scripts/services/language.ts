import * as angular from 'angular';

(() => {

    'use strict';

    angular
        .module('cloudApp.services')
        .provider('languageService', [

            function () {

                let lang = {
                    language: '',
                    pageTitles: {
                        registerSuccess: '',
                        register: '',
                        account: '',
                        system: '',
                        systemShare: '',
                        systems: '',
                        view: '',
                        changePassword: '',
                        activate: '',
                        activateCode: '',
                        activateSuccess: '',
                        restorePassword: '',
                        restorePasswordCode: '',
                        restorePasswordSuccess: '',
                        debug: '',
                        login: '',
                        download: '',
                        downloadPlatform: '',
                        pageNotFound: ''
                    },
                    dialogs: {
                        okButton: '',
                        cancelButton: '',
                        loginTitle: '',
                        loginButton: '',
                        logoutAuthorisedTitle: '',
                        logoutAuthorisedContinueButton: '',
                        logoutAuthorisedLogoutButton: ''
                    },
                    errorCodes: {
                        ok: '',
                        cantSendActivationPrefix: '',
                        cantDisconnectSystemPrefix: '',
                        cantUnshareWithMeSystemPrefix: '',
                        cantSharePrefix: '',
                        cantGetUsersListPrefix: '',
                        cantGetSystemsListPrefix: '',
                        cantRegisterPrefix: '',
                        cantGetSystemInfoPrefix: '',
                        cantChangeAccountPrefix: '',
                        cantChangePasswordPrefix: '',
                        cantActivatePrefix: '',
                        cantSendConfirmationPrefix: '',
                        cloudInvalidResponse: '',
                        notAuthorized: '',
                        brokenAccount: '',
                        wrongParameters: '',
                        wrongCode: '',
                        wrongCodeRestore: '',
                        forbidden: '',
                        accountNotActivated: '',
                        accountBlocked: '',
                        notFound: '',
                        alreadyExists: '',
                        unknownError: '',
                        accountAlreadyActivated: '',
                        emailNotFound: '',
                        EmailAlreadyExists: '',
                        oldPasswordMistmatch: '',
                        passwordMismatch: '',
                        cantEditYourself: '',
                        cantEditAdmin: '',
                        cantOpenClient: '',
                        lostConnection: '',
                        thisSystem: ''
                    },
                    system: {
                        yourSystem: '',
                        mySystemSearch: '',
                        unavailable: '',
                        offline: '',
                        openClient: '',
                        openClientNoSystem: '',
                        confirmMergeAction: '',
                        mergeSystemTitle: '',
                        confirmRenameTitle: '',
                        confirmRenameAction: '',
                        successRename: '',
                        confirmDisconnectTitle: '',
                        confirmDisconnectAction: '',
                        confirmUnshareFromMe: '',
                        confirmUnshareFromMeTitle: '',
                        confirmUnshareFromMeAction: '',
                        confirmUnshare: '',
                        confirmUnshareTitle: '',
                        confirmUnshareAction: '',
                        successDisconnected: '',
                        successDeleted: '',
                        permissionsRemoved: ''
                    },
                    account: {
                        accountSavedSuccess: '',
                        activationLinkSent: '',
                        passwordChangedSuccess: '',
                        changePassword: '',
                        newPasswordLabel: '',
                        saveChanges: ''
                    },
                    activeActions: {
                        sendConfirm: '',
                        resetPassword: '',
                        setNewPassword: '',
                        setNewPasswordLabel: ''
                    },
                    regActions: {
                        passwordLabel: '',
                        createAccount: ''
                    },
                    header: {
                        allSystems: '',
                        systems: ''
                    },
                    common: {}
                };

                // config phaze accessible functions **************
                this.setLanguage = (language) => {
                    lang = language;
                    // lang.language = lang.language.replace('-', '_');
                    // const nxLanguage = nxLanguageService.$get();
                    // nxLanguage.use(lang);
                };
                // ************************************************

                this.$get = () => {
                    return {
                        // runtime accessible
                        lang
                    };
                };

            }]);
})();
