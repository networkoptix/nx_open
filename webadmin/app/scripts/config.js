'use strict';
/*exported Config */

var Config = {
    viewsDir: 'views/', //'lang_' + lang + '/views/';

    defaultLanguage: 'en_US',
    supportedLanguages:[
        'en_US', 'ru'
    ],

    defaultLogin: 'admin',
    defaultPassword: 'admin',
    newServerFlag: 'SF_NewSystem',
    publicIpFlag: 'SF_HasPublicIP',
    iflistFlag: 'SF_IfListCtrl',
    timeCtrlFlag: 'SF_timeCtrl',

    globalEditServersPermissions: 'GlobalAdminPermission',
    globalViewArchivePermission: 'GlobalViewArchivePermission',
    globalAccessAllMediaPermission: 'GlobalAccessAllMediaPermission',

    productName: 'VMS NAME',
    cloud: {
        productName: 'CLOUD NAME',
        webadminSetupContext: '?from=webadmin&context=setup',
        clientSetupContext: '?from=client&context=setup',
        webadminContext: '?from=webadmin&context=settings',

        apiUrl: '/api',
        portalRegisterUrl: '/register',
        portalSystemUrl: '/systems/{systemId}'
    },

    dateSettingsFormat:'dd MMMM yyyy',

    visualLog: false,
    allowDebugMode: false, // Allow debugging at all. Set to false in production
    debug: {
        video: true, // videowindow.js - disable loader, allow rightclick
        videoFormat: false,//'flashls', // videowindow.js - force video player
        chunksOnTimeline: false, // timeline.js - draw debug events
        jshlsHideError: true, //components\jshls.js - Hide errors used in local env
        jshlsDebug: false //components\jshls.js - Create hls player in debug mode
    },
    helpLinks: [
        // Additional Links to show in help
        /*{
         url: '#/support/',
         title: 'Support',
         target: '' // new|frame
         }*/
    ],
    emailRegex:"^[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\.?$", // Check only @ and . in the email

    passwordRequirements: {
        minLength: 8,
        maxLength: 255,
        requiredRegex: '^[\x21-\x7E]$|^[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$',
        minClassesCount: 2,
        strongClassesCount: 3
    },

    undefinedValue:'__qn_undefined_value__',
    settingsConfig: {
        auditTrailEnabled: {label: 'Audit trail enabled', type: 'checkbox'},
        cameraSettingsOptimization: {label: 'Allow device setting optimization', type: 'checkbox', setupWizard: true},
        disabledVendors: {label: 'Disabled vendors', type: 'text'},
        ec2AliveUpdateIntervalSec: {label: 'System alive update interval (seconds)', type: 'number', alert: 'Warning! It is highly recommended to keep this value at least 10% greater than "Connection keep alive timeout" x "Connection keep probes"'},
        ec2ConnectionKeepAliveTimeoutSec: {label: 'Connection keep alive timeout (seconds)', type: 'number'},
        ec2KeepAliveProbeCount: {label: 'Connection keep alive probes (seconds)', type: 'number'},
        emailFrom: {label: 'Email from', type: 'text'},
        emailSignature: {label: 'Email signature', type: 'text'},
        emailSupportEmail: {label: 'Support Email', type: 'text'},
        ldapAdminDn: {label: 'LDAP admin DN', type: 'text'},
        ldapAdminPassword: {label: 'LDAP admin password', type: 'password'},
        ldapSearchBase: {label: 'LDAP search base', type: 'text'},
        ldapSearchFilter: {label: 'LDAP search filter', type: 'text'},
        ldapUri: {label: 'LDAP URI', type: 'text'},
        autoDiscoveryEnabled: {label: 'Enable device auto discovery', type: 'checkbox', setupWizard: true},
        smtpConnectionType: {label: 'SMTP connection type', type: 'text'},
        smtpHost: {label: 'SMTP host', type: 'text'},
        smtpPort: {label: 'SMTP port', type: 'number'},
        smtpSimple: {label: 'SMTP simple', type: 'checkbox'},
        smtpTimeout: {label: 'SMTP timeout', type: 'number'},
        smptPassword: {label: 'SMTP password', type: 'password'},
        smtpUser: {label: 'SMTP user', type: 'text'},
        updateNotificationsEnabled: {label: 'Update notifications enabled', type: 'checkbox'},
        arecontRtspEnabled: {label: 'Arecont RTSP Enabled', type: 'checkbox'},
        backupNewCamerasByDefault: {label: 'Backup new cameras by default', type: 'checkbox'},
        statisticsAllowed: {
            label: 'Send anonymous usage statistics and crash reports',
            type: 'checkbox',
            setupWizard: true
        },
        backupQualities: {label: 'Backup qualities', type: 'text'},
        serverDiscoveryPingTimeoutSec: {label: 'Server discovery timeout', type: 'number'},

        cloudAccountName: {label: 'Cloud owner account', type: 'static'},
        cloudHost: {label: 'Cloud host', type: 'static'},
        cloudAuthKey: {label: 'Cloud auth key', type: 'static'},
        cloudSystemID: {label: 'Cloud system id', type: 'static'},

        systemName: {label: 'System name', type: 'text'},

        newSystem: {label: 'Server is "New"', type: 'static'},
        proxyConnectTimeoutSec: {label: 'Proxy connection timeout (seconds)', type: 'number'},
        crossdomainEnabled: {label: 'Enable web client', type: 'checkbox'},

        statisticsReportLastNumber: {label: 'Statistics report - last number', type: 'static'},
        statisticsReportLastTime: {label: 'Statistics report - last time', type: 'static'},
        statisticsReportServerApi: {label: 'Statistics server api', type: 'text'},
        statisticsReportTimeCycle: {label: 'Statistics report interval', type: 'number'},
        systemId: {label: 'System ID', type: 'text'},
        systemNameForId: {label: 'System name', type: 'text'},
        takeCameraOwnershipWithoutLock: {label: 'Take cameras ownership without lock', type: 'checkbox'},
        timeSynchronizationEnabled: {label: 'Time synchronization enabled', type: 'checkbox'},
        upnpPortMappingEnabled: {label: 'UPNP port mapping enabled', type: 'checkbox'},

        useServerTime: true
    },
    debugEvents: {
        events: [
            {event: 'CameraMotionEvent', label: 'Camera motion'},
            {event: 'CameraInputEvent', label: 'Camera input'},
            {event: 'CameraDisconnectEvent', label: 'Camera disconnect'},
            {event: 'StorageFailureEvent', label: 'Storage failure'},
            {event: 'NetworkIssueEvent', label: 'Network issue'},
            {event: 'CameraIpConflictEvent', label: "Camera IP conflict"},
            {event: 'ServerFailureEvent', label: 'Server failure'},
            {event: 'ServerConflictEvent', label: 'Server conflict'},
            {event: 'ServerStartEvent', label: 'Server start'},
            {event: 'LicenseIssueEvent', label: 'License issue'},
            {event: 'BackupFinishedEvent', label: 'Backup finished'},
            {event: 'UserDefinedEvent', label: 'Generic event'}
        ],
        reasons: [
            'NoReason',
            'NetworkNoFrameReason',
            'NetworkConnectionClosedReason',
            'NetworkRtpPacketLossReason',
            'ServerTerminatedReason',
            'ServerStartedReason',
            'StorageIoErrorReason',
            'StorageTooSlowReason',
            'StorageFullReason',
            'LicenseRemoved',
            'BackupFailedNoBackupStorageError',
            'BackupFailedSourceStorageError',
            'BackupFailedSourceFileError',
            'BackupFailedTargetFileError',
            'BackupFailedChunkError',
            'BackupEndOfPeriod',
            'BackupDone',
            'BackupCancelled'
        ],
        states: [
            '',
            'Inactive',
            'Active'
        ]
    },

    setup:{
        firstPollingRequest: 5000,
        slowPollingTimeout: 5000,
        pollingTimeout: 1000,
        retriesForMergeCredentialsToApply: 15
    }
};
