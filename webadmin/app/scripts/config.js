'use strict';
/*exported Config */

var Config = {
    viewsDir: 'views/', //'lang_' + lang + '/views/';
    viewsDirCommon: 'web_common/views/',
    developersFeedbackForm: 'https://docs.google.com/forms/d/e/1FAIpQLSdN0Woxlo2UgHFF243604gsae9ol9J_N24CtPdRk8EgMFyG4A/viewform?usp=pp_url&entry.1099959647={{PRODUCT}}',
    webadminSystemApiCompatibility: true,
    defaultLanguage: 'en_US',
    supportedLanguages:[
        'en_US', 'ru_RU'
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
    dateInternalFormat:'yyyy-MM-ddThh:mm:ss',

    visualLog: false,
    allowDebugMode: false, // Allow debugging at all. Set to false in production
    debug: {
        chunksOnTimeline: false, // timeline.js - draw debug events
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
        auditTrailEnabled: {type: 'checkbox'},
        cameraSettingsOptimization: {type: 'checkbox', setupWizard: true},
        disabledVendors: {type: 'text'},
        ec2AliveUpdateIntervalSec: {type: 'number', alert: 'Warning! It is highly recommended to keep this value at least 10% greater than "Connection keep alive timeout" x "Connection keep probes"'},
        ec2ConnectionKeepAliveTimeoutSec: {type: 'number'},
        ec2KeepAliveProbeCount: {type: 'number'},
        emailFrom: {type: 'text'},
        emailSignature: {type: 'text'},
        emailSupportEmail: {type: 'text'},
        ldapAdminDn: {type: 'text'},
        ldapAdminPassword: {type: 'password'},
        ldapSearchBase: {type: 'text'},
        ldapSearchFilter: {type: 'text'},
        ldapUri: {type: 'text'},
        autoDiscoveryEnabled: {type: 'checkbox', setupWizard: true},
        smtpConnectionType: {type: 'text'},
        smtpHost: {type: 'text'},
        smtpPort: {type: 'number'},
        smtpSimple: {type: 'checkbox'},
        smtpTimeout: {type: 'number'},
        smptPassword: {type: 'password'},
        smtpUser: {type: 'text'},
        updateNotificationsEnabled: {type: 'checkbox'},
        arecontRtspEnabled: {type: 'checkbox'},
        backupNewCamerasByDefault: {type: 'checkbox'},
        statisticsAllowed: { type: 'checkbox', setupWizard: true},
        backupQualities: {type: 'text'},
        serverDiscoveryPingTimeoutSec: {type: 'number'},

        cloudAccountName: {type: 'static'},
        cloudHost: {type: 'static'},
        cloudAuthKey: {type: 'static'},
        cloudSystemID: {type: 'static'},

        systemName: {type: 'text'},

        newSystem: {type: 'static'},
        proxyConnectTimeoutSec: {type: 'number'},
        crossdomainEnabled: {type: 'checkbox'},

        statisticsReportLastNumber: {type: 'static'},
        statisticsReportLastTime: {type: 'static'},
        statisticsReportServerApi: {type: 'text'},
        statisticsReportTimeCycle: {type: 'number'},
        systemId: {type: 'text'},
        systemNameForId: {type: 'text'},
        takeCameraOwnershipWithoutLock: {type: 'checkbox'},
        timeSynchronizationEnabled: {type: 'checkbox'},
        upnpPortMappingEnabled: {type: 'checkbox'},
    },
    webclient:{
        useServerTime: true,
        disableVolume: true,
        reloadInterval: 5*1000,
        leftPanelPreviewHeight: 38, // 38px is the height for previews in the left panel
        resetDisplayedTextTimer: 3 * 1000,
        hlsLoadingTimeout: 60 * 1000,
            // One minute timeout for manifest:
            // * 30 seconds for server to init camera
            // * 20 seconds for chunks
            // * 10 seconds extra
        updateArchiveStateTimeout: 60*1000, // If camera hs no archive - try to update it every minute
        flashChromelessPath: "components/flashlsChromeless.swf",
        flashChromelessDebugPath: "components/flashlsChromeless_debug.swf",
        staticResources: "web_common/",
        maxCrashCount: 2,
        nativeTimeout: 60 * 1000, //60s
        playerReadyTimeout: 100,
        endOfArchiveTime: 30 * 1000, //30s
        chunksToCheckFatal: 30 //This is used in short cache when requesting chunks for jumpToPosition in timeline directive
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
            {label: 'Instant', value: null},
            {label: 'Inactive', value: 'Inactive'},
            {label: 'Active', value: 'Active'}
        ]
    },

    setup:{
        firstPollingRequest: 5000,
        slowPollingTimeout: 5000,
        pollingTimeout: 1000,
        retriesForMergeCredentialsToApply: 15
    }
};
