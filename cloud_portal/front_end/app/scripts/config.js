'use strict';

var Config = {
    googleAnalyticsCode: 'UA-51046510-4',
    googleTagsCode: 'GTM-5MRNWP',
    apiBase: '/api',
    clientProtocol: 'nx-vms:',
    nativeDomain: 'cloud-demo.hdw.mx',
    cacheTimeout: 30 * 1000, // Cache lives for 30 seconds
    updateInterval:  30 * 1000, // Update content on pages every 30 seconds

    alertTimeout: 3 * 1000,  // Alerts are shown for 3 seconds
    alertsMaxCount: 5,

    redirectAuthorised:'/systems', // Page for redirecting all authorised users
    redirectUnauthorised:'/', // Page for redirecting all unauthorised users by default

    systemStatuses: {
        onlineStatus: 'online',
        sortOrder: [
            'online',
            'offline',
            'activated',
            'unavailable',
            'notActivated'
        ],
        default: {
            style: 'label-default'
        },
        notActivated: {
            label: L.systemStatuses.notActivated,
            style: 'label-danger'
        },
        activated: {
            label: L.systemStatuses.activated,
            style: 'label-info'
        },
        online: {
            label: L.systemStatuses.online,
            style: 'label-success'
        },
        offline: {
            label: L.systemStatuses.offline,
            style: 'label-default'
        },
        unavailable: {
            label: L.systemStatuses.unavailable,
            style: 'label-default'
        }
    },
    accessRoles: {
        unshare: 'none',
        default: 'viewer',
        disabled: 'disabled',
        custom: 'custom',
        owner:   'owner',
        editUserPermissionFlag: 'GlobalAdminPermission',
        editUserAccessRoleFlag: 'Admin', // TODO: remove it later when cloud permissions
        order: [
            'liveViewer',
            'viewer',
            'advancedViewer',
            'cloudAdmin',
            'owner'
        ],
        options: [
            {
                accessRole: 'owner',
                readOnly: true,
                isAdmin: true
            },
            {
                accessRole: 'liveViewer',
                permissions: 'GlobalAccessAllCamerasPermission'
            },
            {
                accessRole: 'viewer',
                permissions:'GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalAccessAllCamerasPermission'
            },
            {
                accessRole: 'advancedViewer',
                permissions: 'GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllCamerasPermission'
            },
            {
                accessRole: 'cloudAdmin',
                readOnly: true,
                permissions: 'GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllCamerasPermission'
            }
        ]
    },

    emailRegex:"^[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\.?$", // Check only @ and . in the email

    passwordRequirements: {
        minLength: 8,
        minLengthMessage:L.passwordRequirements.minLengthMessage,
        maxLength: 255,
        requiredRegex: '^[\x21-\x7E]$|^[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$',
        requiredMessage: L.passwordRequirements.requiredMessage,
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
                if(new RegExp(classRegex).test(password)) {
                    classesCount ++;
                }
            }
            return classesCount >= 3;
        },
        weakMessage: L.passwordRequirements.weakMessage,
        strongMessage: L.passwordRequirements.strongMessage,
        commonMessage: L.passwordRequirements.commonMessage
    }
};
