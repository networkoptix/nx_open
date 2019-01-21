import { Injectable }                from '@angular/core';
import { downgradeInjectable }       from '@angular/upgrade/static';

@Injectable({
    providedIn: 'root'
})
export class NxConfigService {

    config: any;

    constructor() {
        // These properties will be injected on config *******************
        // viewsDir: 'static/views/', //'static/lang_' + lang + '/views/';
        // previewPath: '',
        // viewsDirCommon: 'static/web_common/views/',
        // ***************************************************************

        this.config = {
            gatewayUrl    : '/gateway',
            googleTagsCode: 'GTM-5MRNWP',
            apiBase       : '/api',
            realm         : 'VMS',
            cacamerasUrl  : 'https://cameras.networkoptix.com/api/v1/cacameras/',

            cacheTimeout     : 20 * 1000, // Cache lives for 30 seconds
            updateInterval   : 30 * 1000, // Update content on pages every 30 seconds
            openClientTimeout: 20 * 1000, // 20 seconds we wait for client to open

            openMobileClientTimeout: 300, // 300ms for mobile browsers

            alertTimeout       : 3 * 1000,  // Alerts are shown for 3 seconds
            alertsMaxCount     : 5,
            minSystemsToSearch : 9, // We need at least 9 system to enable search
            maxSystemsForHeader: 6, // Dropdown at the top is limited in terms of number of cameras to display

            redirectAuthorised  : '/systems', // Page for redirecting all authorised users
            redirectUnauthorised: '/', // Page for redirecting all unauthorised users by default

            links: {
                admin: {
                    product: '/admin/cms/product/'
                }
            },

            layout: {
                table: {
                    rows: 10
                },
                tableLarge: {
                    rows: 20
                }
            },

            systemStatuses                : {
                onlineStatus: 'online',
                sortOrder   : [
                    'online',
                    'offline',
                    'activated',
                    'unavailable',
                    'notActivated'
                ],
                default     : {
                    style: 'badge-default'
                },
                notActivated: {
                    style: 'badge-danger'
                },
                activated   : {
                    style: 'badge-info'
                },
                online      : {
                    style: 'badge-success'
                },
                offline     : {
                    style: 'badge-default'
                },
                unavailable : {
                    style: 'badge-default'
                },
                master      : 'master',
                slave       : 'slave'
            },
            systemCapabilities            : {
                cloudMerge: 'cloudMerge'
            },
            accessRoles                   : {
                unshare                  : 'none',
                default                  : 'Viewer',
                disabled                 : 'disabled',
                custom                   : 'custom',
                owner                    : 'Owner',
                editUserPermissionFlag   : 'GlobalAdminPermission',
                globalAdminPermissionFlag: 'GlobalAdminPermission',
                customPermission         : {
                    name       : 'Custom',
                    permissions: 'NoPermission'
                },
                editUserAccessRoleFlag   : 'Admin',
                globalAdminAccessRoleFlag: 'Admin',
                predefinedRoles          : [
                    {
                        isOwner    : true,
                        name       : 'Owner',
                        permissions: 'GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission'
                    },
                    {
                        name       : 'Administrator',
                        permissions: 'GlobalAdminPermission|GlobalEditCamerasPermission|GlobalControlVideoWallPermission|GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission'
                    },
                    {
                        name       : 'Advanced Viewer',
                        permissions: 'GlobalViewLogsPermission|GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalManageBookmarksPermission|GlobalUserInputPermission|GlobalAccessAllMediaPermission'
                    },
                    {
                        name       : 'Viewer',
                        permissions: 'GlobalViewArchivePermission|GlobalExportPermission|GlobalViewBookmarksPermission|GlobalAccessAllMediaPermission'
                    },
                    {
                        name       : 'Live Viewer',
                        permissions: 'GlobalAccessAllMediaPermission'
                    },
                    {
                        name       : 'Custom',
                        permissions: 'NoPermission'
                    }
                ],
                order                    : [
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
            emailRegex                    : '^[-!#$%&\'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&\'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,6}\\.?$',
            passwordRequirements          : {
                minLength         : 8,
                maxLength         : 255,
                requiredRegex     : '^[\x21-\x7E]$|^[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$',
                minClassesCount   : 2,
                strongClassesCount: 3
            },
            downloads                     : {
                mobile: [
                    {
                        name: 'ios',
                        os  : 'iOS'
                    },
                    {
                        name: 'android',
                        os  : 'Android'
                    }
                ],
                groups: [
                    {
                        name      : 'windows',
                        os        : 'Windows',
                        installers: [
                            {
                                platform: 'win64',
                                appType : 'client'
                            },
                            {
                                platform: 'win64',
                                appType : 'server'
                            },
                            {
                                platform: 'win64',
                                appType : 'bundle'
                            },
                            {
                                platform: 'win86',
                                appType : 'client'
                            },
                            {
                                platform: 'win86',
                                appType : 'server'
                            },
                            {
                                platform: 'win86',
                                appType : 'bundle'
                            }
                        ]
                    },
                    {
                        name      : 'linux',
                        os        : 'Linux',
                        installers: [
                            {
                                platform: 'linux64',
                                appType : 'client'
                            },
                            {
                                platform: 'linux64',
                                appType : 'server'
                            },
                            {
                                platform: 'linux86',
                                appType : 'client'
                            },
                            {
                                platform: 'linux86',
                                appType : 'server'
                            }
                        ]
                    },
                    {
                        name      : 'macos',
                        os        : 'MacOS',
                        installers: [
                            {
                                platform: 'mac',
                                appType : 'client'
                            }
                        ]
                    }
                ]
            },
            icons : {
                default : '/static/icons/integration_tile_preview_plugin.svg',
                platforms : [
                    { name: 'arm', src: '/static/icons/integration_tile_os_arm.svg' },
                    { name: 'linux', src: '/static/icons/integration_tile_os_linux.svg' },
                    { name: 'windows', src: '/static/icons/integration_tile_os_windows.svg' }
                ]
            },
            webclient                     : {
                useServerTime            : true,
                useSystemTime            : true,
                disableVolume            : true,
                reloadInterval           : 5 * 1000,
                leftPanelPreviewHeight   : 128,
                resetDisplayedTextTimer  : 3 * 1000,
                hlsLoadingTimeout        : 90 * 1000,
                // One minute timeout for manifest:
                // * 30 seconds for gateway to open connection
                // * 30 seconds for server to init camera
                // * 20 seconds for chunks
                // * 10 seconds extra
                updateArchiveStateTimeout: 60 * 1000,
                flashChromelessPath      : 'components/flashlsChromeless.swf',
                flashChromelessDebugPath : 'components/flashlsChromeless_debug.swf',
                staticResources          : 'static/web_common/',
                maxCrashCount            : 2,
                nativeTimeout            : 60 * 1000,
                playerReadyTimeout       : 100,
                endOfArchiveTime         : 30 * 1000,
                chunksToCheckFatal       : 30 // This is used in short cache when requesting chunks for jumpToPosition in timeline directive
            },
            messageType: {
                ipvd: 'ipvd_feedback',
                inquiry: 'sales_inquiry',
                support: 'request_support',
                unknown: 'unknown'
            },
            permissions                   : {
                canViewRelease: 'can_view_release'
            },
            globalEditServersPermissions  : 'GlobalAdminPermission',
            globalViewArchivePermission   : 'GlobalViewArchivePermission',
            globalAccessAllMediaPermission: 'GlobalAccessAllMediaPermission',
            allowBetaMode                 : false,
            allowDebugMode                : false,
            debug                         : {
                chunksOnTimeline: false // timeline.js - draw debug events
            },
            responseOk                    : 'ok',
            embedInfo                     : {
                vimeo : {
                    link : 'https://player.vimeo.com/video/',
                    regex : '^https?:\\/\\/vimeo\\.com\\/([\\d]+)$'
                },
                youtube : {
                    link: 'https://www.youtube.com/embed/',
                    regex: '^https?:\\/\\/(?:www\\.youtube\\.com\\/(?:embed\\/|watch\\?v=)|youtu\\.be\\/)([\\w]+)$'
                }
            },
            defaultPlatformNames                 : {
                'arm-file'        : 'Arm',
                'linux-x64-file'  : 'Linux x64',
                'linux-x86-file'  : 'Linux x86',
                'macos-file'      : 'Mac OS',
                'rpi-file'        : 'Raspberry Pi',
                'windows-x64-file': 'Windows x64',
                'windows-x86-file': 'Windows x86'
            },
            animation: {
                carouselImageEnter: '0.25s ease-in',
                carouselImageLeave: '0.25s ease-out'
            },
            campage: {
                vendorGroups: 4
            }
        };
    }

    getConfig() {
        return this.config;
    }
}
