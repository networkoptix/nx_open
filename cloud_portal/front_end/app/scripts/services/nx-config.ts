(function () {

    'use strict';

    angular
        .module('cloudApp.services')
        .provider('configService', [

            function () {
                // These properties will be injected on config *******************
                // viewsDir: 'static/views/', //'static/lang_' + lang + '/views/';
                // previewPath: '',
                // viewsDirCommon: 'static/web_common/views/',
                // ***************************************************************

                const config = {
                    gatewayUrl: '/gateway',
                    googleTagsCode: 'GTM-5MRNWP',
                    apiBase: '/api',
                    realm: 'VMS',

                    cacheTimeout: 20 * 1000, // Cache lives for 30 seconds
                    updateInterval: 30 * 1000, // Update content on pages every 30 seconds
                    openClientTimeout: 20 * 1000, // 20 seconds we wait for client to open

                    openMobileClientTimeout: 300, // 300ms for mobile browsers
                    timelineMouseEventTimeout: 300, // milliseconds

                    alertTimeout: 3 * 1000,  // Alerts are shown for 3 seconds
                    alertsMaxCount: 5,
                    minSystemsToSearch: 9, // We need at least 9 system to enable search
                    maxSystemsForHeader: 6, // Dropdown at the top is limited in terms of number of cameras to display
                    maxServers: 100, // The maximum amount of server that can be in a system

                    redirectAuthorised: '/systems', // Page for redirecting all authorised users
                    redirectUnauthorised: '/', // Page for redirecting all unauthorised users by default


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
                            style: 'badge-default'
                        },
                        notActivated: {
                            style: 'badge-danger'
                        },
                        activated: {
                            style: 'badge-info'
                        },
                        online: {
                            style: 'badge-success'
                        },
                        offline: {
                            style: 'badge-default'
                        },
                        unavailable: {
                            style: 'badge-default'
                        },
                        master: 'master',
                        slave: 'slave'
                    },
                    systemCapabilities: {
                        cloudMerge: 'cloudMerge'
                    },
                    accessRoles: {
                        unshare: 'none',
                        default: 'Viewer',
                        disabled: 'disabled',
                        custom: 'custom',
                        owner: 'Owner',
                        editUserPermissionFlag: 'GlobalAdminPermission',
                        globalAdminPermissionFlag: 'GlobalAdminPermission',
                        customPermission: {
                            name: 'Custom',
                            permissions: 'NoPermission'
                        },
                        editUserAccessRoleFlag: 'Admin',
                        globalAdminAccessRoleFlag: 'Admin',
                        predefinedRoles: [
                            {
                                'isOwner': true,
                                'name': 'Owner',
                                'permissions': 'GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission'
                            },
                            {
                                'name': 'Administrator',
                                'permissions': 'GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission'
                            },
                            {
                                'name': 'Advanced Viewer',
                                'permissions': 'GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission'
                            },
                            {
                                'name': 'Viewer',
                                'permissions': 'GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalAccessAllMediaPermission'
                            },
                            {
                                'name': 'Live Viewer',
                                'permissions': 'GlobalAccessAllMediaPermission'
                            },
                            {
                                'name': 'Custom',
                                'permissions': 'NoPermission'
                            }
                        ],
                        order: [
                            'Live Viewer',
                            'liveViewer',
                            'Viewer',
                            'viewer',
                            'Advanced Viewer',
                            'advancedViewer',
                            'Cloud Administrator',
                            'cloudAdmin',
                            'Administrator',
                            'admin',
                            'Owner',
                            'owner'
                        ]
                    },
                    emailRegex: '^[-!#$%&\'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&\'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\.?$',
                    passwordRequirements: {
                        minLength: 8,
                        maxLength: 255,
                        requiredRegex: '^[\x21-\x7E]$|^[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$',
                        minClassesCount: 2,
                        strongClassesCount: 3
                    },
                    downloads: {
                        mobile: [
                            {
                                name: 'ios',
                                os: 'iOS'
                            },
                            {
                                name: 'android',
                                os: 'Android'
                            }
                        ],
                        groups: [
                            {
                                name: 'windows',
                                os: 'Windows',
                                installers: [
                                    {
                                        platform: 'win64',
                                        appType: 'client'
                                    },
                                    {
                                        platform: 'win64',
                                        appType: 'server'
                                    },
                                    {
                                        platform: 'win64',
                                        appType: 'bundle'
                                    },
                                    {
                                        platform: 'win86',
                                        appType: 'client'
                                    },
                                    {
                                        platform: 'win86',
                                        appType: 'server'
                                    },
                                    {
                                        platform: 'win86',
                                        appType: 'bundle'
                                    }
                                ]
                            },
                            {
                                name: 'linux',
                                os: 'Linux',
                                installers: [
                                    {
                                        platform: 'linux64',
                                        appType: 'client'
                                    },
                                    {
                                        platform: 'linux64',
                                        appType: 'server'
                                    },
                                    {
                                        platform: 'linux86',
                                        appType: 'client'
                                    },
                                    {
                                        platform: 'linux86',
                                        appType: 'server'
                                    }
                                ]
                            },
                            {
                                name: 'mac',
                                os: 'MacOS',
                                installers: [
                                    {
                                        platform: 'mac',
                                        appType: 'client'
                                    }
                                ]
                            }
                        ]
                    },
                    webclient: {
                        useServerTime: true,
                        useSystemTime: true,
                        disableVolume: true,
                        reloadInterval: 5 * 1000,
                        leftPanelPreviewHeight: 128,
                        resetDisplayedTextTimer: 3 * 1000,
                        hlsLoadingTimeout: 90 * 1000,
                        // One minute timeout for manifest:
                        // * 30 seconds for gateway to open connection
                        // * 30 seconds for server to init camera
                        // * 20 seconds for chunks
                        // * 10 seconds extra
                        updateArchiveStateTimeout: 60 * 1000,
                        updateArchiveRecordsTimeout: 2 * 1000,
                        flashChromelessPath: 'components/flashlsChromeless.swf',
                        flashChromelessDebugPath: 'components/flashlsChromeless_debug.swf',
                        staticResources: 'static/web_common/',
                        maxCrashCount: 2,
                        nativeTimeout: 60 * 1000,
                        playerReadyTimeout: 100,
                        endOfArchiveTime: 30 * 1000,
                        chunksToCheckFatal: 30, // This is used in short cache when requesting chunks for jumpToPosition in timeline directive
                        skipFramesRenderingTimeline: true
                    },
                    permissions: {
                        canViewRelease: 'can_view_release'
                    },
                    globalEditServersPermissions: 'GlobalAdminPermission',
                    globalViewArchivePermission: 'GlobalViewArchivePermission',
                    globalAccessAllMediaPermission: 'GlobalAccessAllMediaPermission',
                    allowBetaMode: false,
                    allowDebugMode: false,
                    debug: {
                        chunksOnTimeline: false // timeline.js - draw debug events
                    },
                    responseOk: 'ok'
                };

                this.$get = function () {
                    return {
                        config: config
                    };
                };
            } ]);
})();
