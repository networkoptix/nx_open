'use strict';

var L = {
    productName: 'PRODUCT_NAME',
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
        restorePassword: 'Restore password -',
        restorePasswordSuccess: 'Password saved -',
        restorePasswordCode: 'Restore password -',
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
            description: 'User is disabled in the system'
        },
        owner: {
            label: 'Owner',
            description: 'Can do pretty much everything with his system'
        },
        viewer: {
            label: 'Viewer',
            description: 'Can view live video from cameras and archive'
        },
        liveViewer: {
            label: 'Live viewer',
            description: 'Can view only live video from cameras'
        },
        advancedViewer: {
            label: 'Advanced viewer',
            description: 'Can view live video from cameras and archive, configure cameras'
        },
        cloudAdmin: {
            label: 'Administrator',
            description: 'Can configure system and share it'
        },
        custom:{
            label: 'Custom',
            description: 'Access to specified resources with specified rights'
        },
        customRole:{
            label: 'Custom role',
            description: 'Custom user role specified in the system'
        },
    },
    errorCodes:{
        ok: 'ok',

        cloudInvalidResponse: 'Cloud DB returned an unexpected response',
        notAuthorized: 'Login or password are incorrect',
        wrongParameters: 'Some parameters on the form are incorrect',
        wrongCode: 'Wrong confirmation code',

        forbidden: 'You are not authorised for this action',
        accountNotActivated: 'Your account wasn\'t confirmed yet. <a href="/activate">Send confirmation link again</a>',
        accountBlocked: 'Your account was blocked',

        notFound: 'Not found', // Account not found, activation code not found and so on,
        alreadyExists: 'Already Exists', // Account already exists

        unknownError: 'Some unexpected error has happened',

        // Internal error code for interface
        accountAlreadyActivated: 'Your account was already activated',
        emailNotFound: 'Email isn\'t registered in portal',
        emailAlreadyExists: 'Email is already registered in portal',
        oldPasswordMistmatch: 'Current password doesn\'t match',
        passwordMismatch: 'Wrong password',

        systemForbidden: 'You have no access to this system',
        systemNotFound: 'This system wasn\'t found',

        cantEditYourself: 'You can\'t change your own permissions',
        cantEditAdmin: 'This user already has administrator privileges'

    },
    passwordRequirements:{
        minLengthMessage:'Password must contain at least 8 characters',
        requiredMessage: 'Use only latin letters, numbers and keyboard symbols, avoid leading and trailing spaces.',
        weakMessage: 'Use numbers, symbols in different case and special symbols to make your password stronger',
        strongMessage: 'Strong password!',
        commonMessage: 'This password is in top most popular passwords in the world'
    },
    sharing:{
        confirmOwner: 'You are going to change the owner of your system. You will not be able to return this power back!',

        shareTitle: 'Share',
        shareConfirmButton: 'Share',
        editShareTitle: 'Edit users permissions',
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

        confirmUnshareFromMe: "You are going to disconnect this system from your account. You will lose an access for this system. Are you sure?",
        confirmUnshareFromMeTitle: "Delete system?",
        confirmUnshareFromMeAction: "Delete",

        confirmUnshare: "You are going to debar user from the system. Are you sure?",
        confirmUnshareTitle: "Delete User?",
        confirmUnshareAction: "Delete",

        successDisconnected: 'System was successfully disconnected from PRODUCT_NAME',
        successDeleted: 'System {systemName} was successfully deleted from your account',

        permissionsRemoved: 'Permissions were removed from {accountEmail}'
    }
};