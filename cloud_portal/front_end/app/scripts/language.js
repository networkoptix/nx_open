'use strict';

var L = {
    dialogs:{
        okButton: 'Ok',
        loginTitle: 'Login to Nx Cloud'
    },
    systemStatuses:{
        notActivated: 'not activated',
        activated: 'activated',
        online: 'online',
        offline: 'offline',
        unavailable: 'unavailable'
    },
    accessRoles: {
        owner: {
            label: 'owner',
            description: 'Can do pretty much everything with his system'
        },
        viewer: {
            label: 'viewer',
            description: 'Can view live video from cameras and archive'
        },
        liveViewer: {
            label: 'live viewer',
            description: 'Can view only live video from cameras'
        },
        advancedViewer: {
            label: 'advanced viewer',
            description: 'Can view live video from cameras and archive, configure cameras'
        },
        cloudAdmin: {
            label: 'admin',
            description: 'Can configure system and share it'
        }
    },
    errorCodes:{
        ok: 'ok',

        cloudInvalidResponse: 'Cloud DB returned an unexpected response',
        notAuthorized: 'Login or password are incorrect',
        wrongParameters: 'Some parameters on the form are incorrect',
        wrongCode: 'Wrong confirmation code',

        forbidden: 'You are not authorised for this action',
        accountNotActivated: 'Your account wasn\'t confirmed yet. <a href="#/activate">Send confirmation link again</a>',
        accountBlocked: 'Your account was blocked',

        notFound: 'Not found', // Account not found, activation code not found and so on,
        alreadyExists: 'Already Exists', // Account already exists

        unknownError: 'Some unexpected error has happened',

        // Internal error code for interface
        accountAlreadyActivated: 'Your account was already activated',
        emailNotFound: 'Email isn\'t registered in portal',
        emailAlreadyExists: 'Email is already registered in portal',
        oldPasswordMistmatch: 'Current password doesn\'t match',

        systemForbidden: 'You have no access to this system',
        systemNotFound: 'This system wasn\'t found'

    },
    passwordRequirements:
    {
        minLengthMessage:'Password must contain at least 6 characters',
        requiredMessage: 'Use only latin letters, numbers and keyboard symbols',
        weakMessage: 'Use numbers, symbols in different case and special symbols to make your password stronger',
        strongMessage: 'Strong password!',
        commonMessage: 'This password is in top most popular passwords in the world'
    },
    sharing:{
        cantEditYourself: 'Can\'t edit yourself',
        confirmOwner: 'You are going to change the owner of your system. You will not be able to return this power back!'
    },
    system:{
        confirmDisconnect: "You are going to completely disconnect your system from the cloud. Are you sure?",
        confirmDisconnectTitle: "Disconnect system?",
        confirmDisconnectAction: "Disconnect",

        confirmUnshareFromMe: "You are going to disconnect this system from your account. You will lose an access for this system. Are you sure?",
        confirmUnshareFromMeTitle: "Delete system?",
        confirmUnshareFromMeAction: "Delete",

        confirmUnshare: "You are going to debar user from the system. Are you sure?",
        confirmUnshareTitle: "Delete User?",
        confirmUnshareAction: "Delete",

        permissionsRemoved: 'Permissions were removed from {accountEmail}'
    }
};
