'use strict';

var L = {
    productName: 'PRODUCT_NAME',
    clientProtocol: 'CLIENT_PROTOCOL:',
    clientDomain: '{{portalDomain}}',

    dialogs:{
        okButton: 'Ok',
        loginTitle: 'Login to PRODUCT_NAME'
    },
    pageTitles:{
        template: '{{title}} PRODUCT_NAME',
        default: 'PRODUCT_NAME',
        registerSuccess: 'Welcome to',
        register: 'Register in',
        changePassword: 'Change password -',
        account: 'Account settings -',
        systems: 'Systems -',
        system: 'System -',
        systemShare: 'Share the system -',
        activate: 'Activate account -',
        activateSent: 'Activate account -',
        activateSuccess: 'Account activated -',
        activateCode: 'Activate account -',
        restorePassword: 'Reset password -',
        restorePasswordSuccess: 'Password saved -',
        restorePasswordCode: 'Reset password -',
        contentPage: '',
        debug: 'Debug',
        login: 'Login -',
        download: 'Download VMS_NAME',
        downloadPlatform: 'Download VMS_NAME for ',
        startPage: '',
        pageNotFount: 'Page not found'
    },
    systemStatuses:{
        notActivated: 'not activated',
        activated: 'activated',
        online: 'online',
        offline: 'offline',
        unavailable: 'unavailable'
    },
    accessRoles: {
        disabled:{
            label: 'Disabled',
            description: 'User is disabled and cannot log in to the system.'
        },
        'Owner': {
            label: 'Owner',
            description: 'Unrestricted access including the ability to share and connect/disconnect system from cloud.'
        },
        'Viewer': {
            label: 'Viewer',
            description: 'Can view live video and browse the archive.'
        },
        'Live Viewer': {
            label: 'Live Viewer',
            description: 'Can only view live video.'
        },
        'Advanced Viewer': {
            label: 'Advanced Viewer',
            description: 'Can view live video, browse the archive, configure cameras, control PTZ etc.'
        },
        'Administrator':{
            label: 'Administrator',
            description: 'Unrestricted access including the ability to share.'
        },
        'Custom':{
            label: 'Custom',
            description: 'Use the desktop client application to set up custom permissions.'
        },
        cloudAdmin: {
            label: 'Administrator',
            description: 'Can configure system and share it.'
        },
        customRole:{
            label: 'Custom role',
            description: 'Custom user role specified in the system.'
        }
    },
    errorCodes:{
        ok: 'ok',

        cloudInvalidResponse: 'Cloud DB returned an unexpected response.',
        notAuthorized: 'Login or password are incorrect.',
        wrongParameters: 'Some parameters on the form are incorrect.',
        wrongCode: 'Wrong confirmation code.',

        forbidden: 'You do not have permissions to perform this action.',
        accountNotActivated: 'This account hasn\'t been activated yet. <a href="/activate">Send activation link again</a>',
        accountBlocked: 'This account has been blocked.',

        notFound: 'Not found', // Account not found, activation code not found and so on,
        alreadyExists: 'Already exists', // Account already exists

        unknownError: 'Unexpected error occured.',

        // Internal error code for interface
        accountAlreadyActivated: 'This account has been already activated.',
        emailNotFound: 'This email has not been registered in portal.',
        emailAlreadyExists: 'This email address has been already registered.',
        oldPasswordMistmatch: 'Current password is incorrect.',
        passwordMismatch: 'Wrong password',

        systemForbidden: 'You don\'t have access to this system.',
        systemNotFound: 'System not found.',

        cantEditYourself: 'Changing own permissions is not allowed.',
        cantEditAdmin: 'This user already has administrator permissions.'

    },
    passwordRequirements:{
        minLengthMessage:'Password must contain at least 8 characters',
        requiredMessage: 'Use only latin letters, numbers and keyboard symbols, avoid leading and trailing spaces.',
        weakMessage: 'Use numbers, upper and lower case letters and special characters to make your password stronger.',
        strongMessage: 'Strong password!',
        commonMessage: 'This password is in top most popular passwords in the world'
    },
    sharing:{
        confirmOwner: 'You are about to change the owner of the system. You will lose owner permissions. This action is irreversible. Are you sure?',

        shareTitle: 'Share',
        shareConfirmButton: 'Share',
        editShareTitle: 'Edit users\' permissions',
        editShareConfirmButton: 'Save'
    },
    system:{
        yourSystem: 'Your system',
        mySystemSearch: 'IMeMyMine',

        unavailable: 'System is unreachable',
        offline: 'System is offline',

        confirmRenameTitle: 'System name',
        confirmRenameAction: 'Save',
        successRename: 'System name was successfully saved',

        confirmDisconnectTitle: "Disconnect system from PRODUCT_NAME?",
        confirmDisconnectAction: "Disconnect",

        confirmUnshareFromMe: "You are about to disconnect this system from your account. You will not be able to access this system via cloud anymore. Are you sure?",
        confirmUnshareFromMeTitle: "Delete system?",
        confirmUnshareFromMeAction: "Delete",

        confirmUnshare: "You are going to restrict the access to this user. Are you sure?",
        confirmUnshareTitle: "Delete user?",
        confirmUnshareAction: "Delete",

        successDisconnected: 'System was successfully disconnected from PRODUCT_NAME',
        successDeleted: 'System {systemName} was successfully deleted from your account',

        permissionsRemoved: 'Permissions were removed from {accountEmail}'
    }
};