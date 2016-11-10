'use strict';

var Config = {
    googleTagsCode: 'GTM-5MRNWP',
    apiBase: '/api',
    realm: 'VMS',

    cacheTimeout: 20 * 1000, // Cache lives for 30 seconds
    updateInterval:  30 * 1000, // Update content on pages every 30 seconds
    openClientTimeout: 5 * 1000, // 30 second we wait for client to open
    openMobileClientTimeout: 300, // 300ms for mobile browsers

    alertTimeout: 3 * 1000,  // Alerts are shown for 3 seconds
    alertsMaxCount: 5,
    minSystemsToSearch: 9, //We need at least 9 system to enable search

    redirectAuthorised:'/systems/default', // Page for redirecting all authorised users
    redirectUnauthorised:'/', // Page for redirecting all unauthorised users by default

    systemStatuses: {
        onlineStatus: 'online', //*/'offline', // - special hack for a bug
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
        default: 'Viewer',
        disabled: 'disabled',
        custom: 'custom',
        owner:   'Owner',
        editUserPermissionFlag: 'GlobalAdminPermission',
        globalAdminPermissionFlag: 'GlobalAdminPermission',
        customPermission:{
            name: 'Custom',
            permissions: 'NoPermission'
        },

        editUserAccessRoleFlag: 'Admin', // TODO: remove it later when cloud permissions are ready
        globalAdminAccessRoleFlag: 'Admin', // TODO: remove it later when cloud permissions are ready
        predefinedRoles:[
            {
                "isOwner": true,
                "name": "Owner",
                "permissions": "GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"
            },
            {
                "name": "Administrator",
                "permissions": "GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"
            },
            {
                "name": "Advanced Viewer",
                "permissions": "GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission"
            },
            {
                "name": "Viewer",
                "permissions": "GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalAccessAllMediaPermission"
            },
            {
                "name": "Live Viewer",
                "permissions": "GlobalAccessAllMediaPermission"
            },
            {
                "name": 'Custom',
                "permissions": 'NoPermission'
            }
        ],
        order: [
            'liveViewer',
            'viewer',
            'advancedViewer',
            'cloudAdmin',
            'owner'
        ]
    },

    emailRegex:"^[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\.?$", // Check only @ and . in the email

    passwordRequirements: {
        minLength: 8,
        minLengthMessage:L.passwordRequirements.minLengthMessage,
        maxLength: 255,
        requiredRegex: '^[\x21-\x7E]$|^[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$',
        requiredMessage: L.passwordRequirements.requiredMessage,
        minClassesCount: 2,
        strongClassesCount: 3,
        weakMessage: L.passwordRequirements.weakMessage,
        strongMessage: L.passwordRequirements.strongMessage,
        commonMessage: L.passwordRequirements.commonMessage
    }
};
