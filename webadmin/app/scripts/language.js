'use strict';

var L = {
    dialogs:{
        okButton: 'Ok',
        openLink: 'Visit this link',
        openLinkWithTitle: 'To {{title}} visit this link'
    },
    setup:{
        createAccount: 'create account'
    },
    passwordRequirements:
    {
        minLengthMessage:'Password must contain at least 8 characters',
        requiredMessage: 'Use only latin letters, numbers and keyboard \n symbols, avoid leading and trailing spaces.',
        weakMessage: 'Use numbers, symbols in different case\n and special symbols to make your password stronger',
        strongMessage: 'Strong password!',
        commonMessage: 'This password is in top most popular passwords in the world'
    },
    settings:{
        confirmRestart: 'Do you want to restart server now?',
        couldntCheckInternet: 'Couldn\'t check cloud connection',
        confirmRestoreDefault: 'Do you want to clear database and settings? This process will take some time and server will be restarted afterwards.',
        confirmRestoreDefaultTitle: 'Restore factory defaults',
        unexpectedError: 'Can\'t proceed with action: unexpected error has happened',
        connnetionError: 'Connection error',
        wrongPassword: 'Wrong password.',
        error: 'Error: ',
        restartNeeded: 'All changes saved. New settings will be applied after restart. \n Do you want to restart server now?',
        settingsSaved: 'Settings saved',
        confirmHardwareRestart: 'Do you want to restart server\'s operation system?',
        confirmRestoreSettings: 'Do you want to restore all server\'s settings? Archive will be saved, but network settings will be reset.',
        confirmRestoreSettingsNotNetwork: 'Do you want to restart all server\'s settings? Archive and network settings will be saved.',
        unavailable: 'Unavailable',

        confirmDisconnectFromCloud:'Disconnect system from the {{CLOUD_NAME}}',
        confirmDisconnectFromCloudTitle:'Do you want to disconnect your system? It will be unreachable from the internet then.',
        confirmDisconnectFromCloudAction: 'Disconnect',
        disconnectedSuccess: 'System was disconnected successfully',
        connectedSuccess: 'System was connected successfully',

        createLocalOwner:null,
        createLocalOwnerTitle:'Create local administrator',


        confirmChangePassword: 'Change admin\'s password',
        confirmChangePasswordTitle:'Are you sure you want to change system password? It will affect admin user and root password for some servers',
        confirmChangePasswordAction: 'Save new password',

        nx1ControlHint: 'Client application was started. Use a keyboard or mobile application to control it.'

    },
    join:{
        systemIsUnreacheble: 'System is unreachable or doesn\'t exist.',
        incorrectCurrentPassword: 'Incorrect current password',
        incorrectRemotePassword: 'Login or password are incorrect',
        incompatibleVersion: 'System is unreachable, doesn\'t exist or has incompatible version.',
        wrongUrl: 'Unable to connect to specified server.',
        safeMode: 'Can\'t merge systems. Remote system is in safe mode.',
        configError: 'Can\'t merge systems. Maybe one of the systems is in safe mode.',
        cloudError: 'Can\'t merge systems. Dependent system is connected to cloud. You need to disconnect it first.',
        cloudHostConflict: 'Can\'t merge systems, because servers are built with different cloud hosts',
        licenceError: 'Warning: You are about to merge Systems with START licenses. As only 1 START license is allowed per System after your merge you will only have 1 START license remaining. If you understand this and would like to proceed please click Merge to continue.',
        connectionFailed: 'Connection failed: ',
        unknownError: 'Unknown error',
        mergeFailed: 'Merge failed: ',
        mergeSucceed: 'Merge succeed.'
    },
    login:{
        incorrectPassword: 'Login or password is incorrect'
    },
    navigaion:{
        cannotGetUser: 'Server failure: cannot retrieve current user data'
    },
    offlineDialog: {
        serverOffline:'server is offline'
    },
    restartDialog:{
        serverStarting:'server is starting',
        serverRestarting:'server is restarting',
        serverOffline: 'server is offline'
    },
    fileUpload:{
        started: 'Updating successfully started. It will take several minutes',
        upToDate: 'Updating failed. The provided version is already installed.',
        invalidFile: 'Updating failed. Provided file is not a valid update archive.',
        incompatibleSystem: 'Updating failed. Provided file is targeted for another system.',
        extractionError: 'Updating failed. Extraction failed, check available storage.',
        installationError: 'Updating failed. Couldn\'t execute installation script.',
        selectFiles: 'Select files'
    }
};
