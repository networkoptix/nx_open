'use strict';

var Config = {
    apiBase: '/api',
    clientProtocol: 'vms://',

    systemStatuses:{
        default: {
            style: 'label-default'
        },
        notActivated: {
            label: 'not activated',
            style: 'label-warning'
        },
        activated:{
            label: 'activated',
            style: 'label-info'
        }
    },
    accessRolesSettings:{
        unshare: 'none',
        default: 'viewer',
        owner:   'owner',
        labels: {
            "owner": "owner",
            "maintenance" : "maintenance",
            "viewer": "viewer",
            "editor": "editor",
            "editorWithSharing": "editor and sharing"
        },
        order:[
            "viewer",
            "editor",
            "editorWithSharing",
            "maintenance",
            "owner"
        ],
        options:[
            {
                "accessRole": "maintenance"
            },
            {
                "accessRole": "owner"
            },
            {
                "accessRole": "viewer"
            },
            {
                "accessRole": "editor"
            },
            {
                "accessRole": "editorWithSharing"
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
        oldPasswordMistmatch: 'Current password doesn\'t match'
    }
};
