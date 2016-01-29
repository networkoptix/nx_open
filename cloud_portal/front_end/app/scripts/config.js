'use strict';

var Config = {
    apiBase: '/api',
    clientProtocol: 'vms://',
    cacheTimeout: 30*1000, // Cache lives for 30 seconds

    redirectAuthorised:'/systems', // Page for redirecting all authorised users
    redirectUnauthorised:'/', // Page for redirecting all unauthorised users by default

    systemStatuses:{
        onlineStatus: 'online',
        default: {
            style: 'label-default'
        },
        notActivated: {
            label: 'not activated',
            style: 'label-danger'
        },
        activated:{
            label: 'activated',
            style: 'label-info'
        },
        online:{
            label: 'online',
            style: 'label-success'
        },
        offline:{
            label: 'offline',
            style: 'label-default'
        },
        unavailable:{
            label: 'unavailable',
            style: 'label-default'
        }
    },
    accessRoles:{
        unshare: 'none',
        default: 'viewer',
        owner:   'owner',
        settings:{
            "owner": {
                label: "owner",
                description: "Can do pretty much everything with his system"
            },
            "viewer": {
                label: "viewer",
                description: "Can view live video from cameras and archive"
            },
            "liveViewer": {
                label: "live viewer",
                description: "Can view only live video from cameras"
            },
            "advancedViewer": {
                label: "advanced viewer",
                description: "Can view live video from cameras and archive, configure cameras"
            },
            "cloudAdmin": {
                label: "admin",
                description: "Can configure system and share it"
            }
        },
        order:[
            "liveViewer",
            "viewer",
            "advancedViewer",
            "cloudAdmin",
            "owner"
        ],
        options:[
            {
                "accessRole": "owner"
            },
            {
                "accessRole": "viewer"
            },
            {
                "accessRole": "liveViewer"
            },
            {
                "accessRole": "advancedViewer"
            },
            {
                "accessRole": "cloudAdmin"
            }
        ]
    },

    emailRegex:'.+@.+\\..+', // Check only @ and . in the email
    passwordRequirements:
    {
        minLength: 6,
        minLengthMessage:'Password must contain at least 6 characters',
        maxLength: 255,
        requiredRegex: '[\x21-\x7E]+',
        requiredMessage: 'Use only latin letters, numbers and keyboard symbols',
        strongPasswordCheck: function(password){

            var classes = [
                '[0-9]+',
                '[a-z]+',
                '[A-Z]+',
                '[\\W_]+'
            ];

            var classesCount = 0;

            for (var i = 0; i < classes.length; i++) {
                var classRegex = classes[i];
                if(new RegExp(classRegex).test(password)){
                    classesCount ++;
                }
            }
            return classesCount >= 3;
        },
        weakMessage: 'Use numbers, symbols in different case and special symbols to make your password stronger',
        strongMessage: 'Strong password!',
        commonMessage: 'This password is in top most popular passwords in the world'
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

    }
};
