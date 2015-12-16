'use strict';

var Config = {
    apiBase: '/api',
    clientProtocol: 'vms://',
    errorCodes:{
        ok: 'ok',

        cloudInvalidResponse: 'Cloud DB returned an unexpected response',
        notAuthorized: 'Login or password are incorrect',
        wrongParameters: 'Some parameters on the form are incorrect',

        forbidden: 'You are not authorised for this action',
        accountNotActivated: 'Your account wasn\'t confirmed yet. <a href="#/activate">Send confirmation link again</a>',
        accountBlocked: 'Your account was blocked',

        notFound: 'Not found', // Account not found, activation code not found and so on,
        alreadyExists: 'Already Exists', // Account already exists

        unknownError: 'Some unexpected error has happened'
    }
};
