'use strict';
/*exported Config */

(function() {
    window.Config = {
        viewsDir: 'views/', //'lang_' + lang + '/views/';
        viewsDirCommon: 'web_common/views/',
        developersFeedbackForm: 'https://docs.google.com/forms/d/e/1FAIpQLSdN0Woxlo2UgHFF243604gsae9ol9J_N24CtPdRk8EgMFyG4A/viewform?usp=pp_url&entry.1099959647={{PRODUCT}}',
        webadminSystemApiCompatibility: true,
        defaultLanguage: 'en_US',
        supportedLanguages: [
            'en_US', 'ru_RU'
        ],

        defaultLogin: 'admin',
        defaultPassword: 'admin',
        defaultPort: 7001,
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

        dateSettingsFormat: 'dd MMMM yyyy',
        dateInternalFormat: 'yyyy-MM-ddThh:mm:ss',

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
        emailRegex: "^[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\.?$", // Check only @ and . in the email

        passwordRequirements: {
            minLength: 8,
            maxLength: 255,
            requiredRegex: '^[\x21-\x7E]$|^[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$',
            minClassesCount: 2,
            strongClassesCount: 3
        },

        undefinedValue: '__qn_undefined_value__',
        settingsConfig: {
            auditTrailEnabled: {type: 'checkbox'},
            cameraSettingsOptimization: {type: 'checkbox', setupWizard: true},
            disabledVendors: {type: 'text'},
            ec2AliveUpdateIntervalSec: {
                type: 'number',
                alert: 'Warning! It is highly recommended to keep this value at least 10% greater than "Connection keep alive timeout" x "Connection keep probes"'
            },
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
            smtpPassword: {type: 'password'},
            smtpUser: {type: 'text'},
            updateNotificationsEnabled: {type: 'checkbox'},
            arecontRtspEnabled: {type: 'checkbox'},
            backupNewCamerasByDefault: {type: 'checkbox'},
            statisticsAllowed: {type: 'checkbox', setupWizard: true},
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
            maxRtspConnectDurationSec: {label: 'Maximum duration for RTSP connection (seconds)', type: 'number'},

            statisticsReportLastNumber: {type: 'static'},
            statisticsReportLastTime: {type: 'static'},
            statisticsReportServerApi: {type: 'text'},
            statisticsReportTimeCycle: {type: 'number'},
            localSystemId: {type: 'static'},
            systemId: {type: 'static'},
            systemNameForId: {type: 'text'},
            takeCameraOwnershipWithoutLock: {type: 'checkbox'},
            upnpPortMappingEnabled: {type: 'checkbox'},

            trafficEncryptionForced:  {type: 'checkbox'},
            videoTrafficEncryptionForced: {type: 'checkbox'},
            updateStatus: {type: 'static'},
            watermarkSettings: {type: 'static'},

            timeSynchronizationEnabled: {type: 'checkbox'},
            primaryTimeServer: {type: 'static'},
            osTimeChangeCheckPeriodMs: {type: 'number'},
            syncTimeExchangePeriod: {type: 'number'},

            maxWearableArchiveSynchronizationThreads: {type: 'number'},

            hanwhaDeleteProfilesOnInitIfNeeded: {type: 'checkbox'},
            hanwhaChunkReaderMessageBodyTimeoutSeconds: {type: 'number'},
            hanwhaChunkReaderResponseTimeoutSeconds: {type: 'number'},
            showHanwhaAlternativePtzControlsOnTile: {type: 'checkbox'},

            maxEventLogRecords: {type: 'number'}
        },
        webclient: {
            useServerTime: true,
            useSystemTime: true,
            disableVolume: true,
            reloadInterval: 5 * 1000,
            leftPanelPreviewHeight: 38, // 38px is the height for previews in the left panel
            resetDisplayedTextTimer: 3 * 1000,
            hlsLoadingTimeout: 60 * 1000,
            // One minute timeout for manifest:
            // * 30 seconds for server to init camera
            // * 20 seconds for chunks
            // * 10 seconds extra
            updateArchiveStateTimeout: 60 * 1000,
            flashChromelessPath: "components/flashlsChromeless.swf",
            flashChromelessDebugPath: "components/flashlsChromeless_debug.swf",
            staticResources: "web_common/",
            maxCrashCount: 2, //Number of retries to get video
            nativeTimeout: 60 * 1000 //60s
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

        setup: {
            firstPollingRequest: 5000,
            slowPollingTimeout: 5000,
            pollingTimeout: 1000,
            retriesForMergeCredentialsToApply: 15
        },

        metrics:{
            hide:{
                p2pCounters: true,
                transactions: true,
                tcpConnections: true,
                offlineStatus: true
            },
            statusOrder:{
                Offline: 1,
                Unauthorized: 2,
                Recording: 8,
                Online: 9
            },
            percentValues:{
                danger: 70, // Danger level of anything - more that 70% usage
                warning: 50 // Warning level - more that 50% usage
            },
            liveMetricsUpdate: 10000 // Every 10 seconds
        },
        errors: {
            login: {
                wrongPassword: 'Wrong username or password.',
                accountLockout: 'The user is locked out due to several failed attempts. Please, try again later.'
            }
        }
    };
})();
