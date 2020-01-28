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
            apiBase       : '/api',
            realm         : 'VMS',
            cacamerasUrl  : 'https://cameras.networkoptix.com/api/v1/cacameras/',

            cacheTimeout     : 20 * 1000, // Cache lives for 30 seconds
            updateInterval   : 30 * 1000, // Update content on pages every 30 seconds
            openClientTimeout: 20 * 1000, // 20 seconds we wait for client to open
            openClientError: 'notVisited',

            openMobileClientTimeout  : 300, // 300ms for mobile browsers
            timelineMouseEventTimeout: 300, // milliseconds

            alertTimeout       : 3 * 1000,  // Alerts are shown for 3 seconds
            alertsMaxCount     : 5,
            minSystemsToSearch : 9, // We need at least 9 system to enable search
            maxSystemsForHeader: 6, // Dropdown at the top is limited in terms of number of cameras to display
            maxServers         : 100, // The maximum amount of server that can be in a system

            redirectAuthorised  : '/systems', // Page for redirecting all authorised users
            redirectUnauthorised: '/', // Page for redirecting all unauthorised users by default
            redirect404: '/404',
            redirectPaths: ['/register', '/restore_password', '/activate', '/404'],

            links: {
                admin: {
                    asset: '/admin/cms/asset/%ID%/pages/'
                }
            },

            layout: {
                table     : {
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
                    style: 'default'
                },
                notActivated: {
                    style: 'danger'
                },
                activated   : {
                    style: 'info'
                },
                online      : {
                    style: 'success'
                },
                offline     : {
                    style: 'default'
                },
                unavailable : {
                    style: 'default'
                },
                master      : 'master',
                slave       : 'slave'
            },
            accessRoles                   : {
                adminAccess              : ['cloudadmin', 'owner', 'administrator'],
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
            emailRegex                    : '^[-!#$%&\'*+/=?^_`{}|~0-9a-zA-Z]+(\\.[-!#$%&\'*+/=?^_`{}|~0-9a-zA-Z]+)*@(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,63}\\.?$',
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
                groups: {
                    windows: {
                        name    : 'windows',
                        os      : 'windows',
                        appTypes: ['bundle', 'client', 'server'],
                    },
                    linux  : {
                        name    : 'linux',
                        os      : 'linux',
                        appTypes: ['bundle', 'client', 'server']
                    },
                    macos  : {
                        name    : 'macos',
                        os      : 'MacOS',
                        appTypes: ['client']
                    },
                    arm    : {
                        name    : 'arm',
                        os      : '',
                        appTypes: ['client', 'server']
                    },
                    sdk    : {
                        name    : 'sdk',
                        os      : '',
                        appTypes: ['universal']
                    }
                },
                platformMatch: {
                    unix: 'Linux',
                    linux: 'Linux',
                    mac: 'MacOS',
                    windows: 'Windows',
                    arm: 'ARM',
                    skd: 'SDK'
                }
            },
            icons                         : {
                default  : '/static/icons/integration_tile_preview_plugin.svg',
                platforms: [
                    { name: 'mac', src: '/static/icons/integration_tile_os_mac.svg' },
                    { name: 'android', src: '/static/icons/integration_tile_os_android.svg' },
                    { name: 'arm', src: '/static/icons/integration_tile_os_arm.svg' },
                    { name: 'linux', src: '/static/icons/integration_tile_os_linux.svg' },
                    { name: 'windows', src: '/static/icons/integration_tile_os_windows.svg' }
                ],
                dir: '/static/icons/',
            },
            webclient                     : {
                useServerTime              : true,
                useSystemTime              : true,
                disableVolume              : true,
                reloadInterval             : 30 * 1000,
                leftPanelPreviewHeight     : 128,
                resetDisplayedTextTimer    : 3 * 1000,
                hlsLoadingTimeout          : 90 * 1000,

                // One minute timeout for manifest:
                // * 30 seconds for gateway to open connection
                // * 30 seconds for server to init camera
                // * 20 seconds for chunks
                // * 10 seconds extra
                updateArchiveStateTimeout  : 60 * 1000,
                updateArchiveRecordsTimeout: 2 * 1000,
                flashChromelessPath        : 'components/flashlsChromeless.swf',
                flashChromelessDebugPath   : 'components/flashlsChromeless_debug.swf',
                staticResources            : 'static/web_common/',
                maxCrashCount              : 2,
                nativeTimeout              : 60 * 1000,
                playerReadyTimeout         : 100,
                endOfArchiveTime           : 30 * 1000,
                chunksToCheckFatal         : 30, // This is used in short cache when requesting chunks for jumpToPosition in timeline directive
                skipFramesRenderingTimeline: true
            },
            messageSubjects                : {
                integration         : ['sales_inquiry', 'technical_inquiry', 'integration_feedback'],
                ipvd_feedback_page  : ['ipvd_feedback_page'],
                ipvd_feedback_device: ['ipvd_feedback_device']
            },
            messageType                   : {
                ipvd_page  : 'ipvd_feedback_page',
                ipvd_device: 'ipvd_feedback_device',
                integration: 'integration',
                unknown    : 'unknown'
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
                vimeo  : {
                    link : 'https://player.vimeo.com/video/',
                    regex: '^https?:\\/\\/vimeo\\.com\\/([\\d]+)$'
                },
                youtube: {
                    link : 'https://www.youtube.com/embed/',
                    regex: '^https?:\\/\\/(?:www\\.youtube\\.com\\/(?:embed\\/|watch\\?v=)|youtu\\.be\\/)([\\w\-]+)$'
                }
            },
            defaultPlatformNames: {
                'arm-64-file'             : 'ARM 64bit',
                'linux-x64-file'          : 'Linux x64',
                'macos-file'              : 'Mac OS',
                'arm-32-file'             : 'ARM 32bit',
                'windows-x64-file'        : 'Windows x64',
                'downloadableInstructions': 'Instructions / Manual'
            },
            animation                     : {
                carouselImageEnter: '0.25s ease-in',
                carouselImageLeave: '0.25s ease-out'
            },
            ipvd: {
                pagerMaxSize                    : 4,
                firmwaresToShow                 : 4,
                analyticsToShow                 : 4,
                sortSupportedDevicesByPopularity: '',
                supportedResolutions            : '',
                supportedHardwareTypes          : '',
                searchTags                      : '',
                vendorsShown                    : '',
            },
            search: {
                maxLength   : 200,
                debounceTime: 500 // ms
            },
            systemHealthMenu: {
                baseUrl: '/health/'
            },
            systemMenu  : {
                baseUrl: '/systems/',
                admin  : {
                    id  : 'admin',
                    icon: 'glyphicon-home',
                    path: ''
                },
                users: {
                    id  : 'users',
                    icon: 'glyphicon-users',
                    path: 'users'
                },
                buttons: {
                    id  : 'buttons'
                }
            },
            accountMenu: {
                baseUrl: '/account',
                icon : 'glyphicon-user',
                settings  : {
                    id  : 'settings',
                    path: ''
                },
                password: {
                    id: 'password',
                    path: '/password'
                },
            },
            meta: {
                viewport: {
                    default: 'width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, shrink-to-fit=no',
                    desktopLayout: 'width=768, maximum-scale=1, user-scalable=yes, shrink-to-fit=no'
                }
            },
            healthMonitoring: {
                valueFormats: {
                    '%': {multiplier: 100, decimals: 0},
                    'TB': {multiplier: 1 / 1024 ** 4},
                    'GB': {multiplier: 1 / 1024 ** 3},
                    'MB': {multiplier: 1 / 1024 ** 2},
                    'KB': {multiplier: 1 / 1024},
                    'B': {multiplier: 1},
                    // Start deprecated formats
                    'GBps': {display: 'GB/s', multiplier: 1 / 1000 ** 3, decimals: 2},
                    'MBps': {display: 'MB/s', multiplier: 1 / 1000 ** 2, decimals: 2},
                    'KBps': {display: 'kB/s', multiplier: 1 / 1000, decimals: 2},
                    'Bps': {display: 'B/s', multiplier: 1, decimals: 0},
                    'Gbps': {display: 'Gbit/s', multiplier: 1 / 1000 ** 3, decimals: 2},
                    'Mbps': {display: 'Mbit/s', multiplier: 1 / 1000 ** 2, decimals: 2},
                    'kbps': {display: 'kbit/s', multiplier: 1 / 1000, decimals: 2},
                    'bps': {display: 'bit/s', multiplier: 1, decimals: 0},
                    'Transactions/s': {multiplier: 1, decimals: 1},
                    // End deprecated formats
                    'TB/s': {multiplier: 1 / 1024 ** 4},
                    'GB/s': {multiplier: 1 / 1024 ** 3},
                    'MB/s': {multiplier: 1 / 1024 ** 2},
                    'KB/s': {multiplier: 1 / 1024},
                    'B/s': {multiplier: 1},

                    'Tbit': {multiplier: 8 * (1 / 1000 ** 4)},
                    'Gbit': {multiplier: 8 * (1 / 1000 ** 3)},
                    'Mbit': {multiplier: 8 * (1 / 1000 ** 2)},
                    'Kbit': {multiplier: 8 * (1 / 1000)},
                    'bit': {multiplier: 8},

                    'Tbit/s': {multiplier: 8 * (1 / 1000 ** 4)},
                    'Gbit/s': {multiplier: 8 * (1 / 1000 ** 3)},
                    'Mbit/s': {multiplier: 8 * (1 / 1000 ** 2)},
                    'Kbit/s': {multiplier: 8 * (1 / 1000)},
                    'bit/s': {multiplier: 8},

                    'TPix/s': {multiplier: 1 / 1000 ** 4},
                    'GPix/s': {multiplier: 1 / 1000 ** 3},
                    'MPix/s': {multiplier: 1 / 1000 ** 2},
                    'KPix/s': {multiplier: 1 / 1000},
                    'Tr/s': {multiplier: 1},
                },
                classFormats: {
                    'resource': 'long-text',
                    'longText': 'long-text',
                    'shortText': 'short-text',
                    'text': 'text',
                    'number': '',
                    'GB': 'volume-metric',
                    'KB': 'volume-metric',
                    'MB': 'volume-metric',
                    'TB': 'volume-metric',
                    '%': 'percent',
                    'Mpix/s': '',
                    'MB/s': '',
                    'Mbit/s': '',
                    'KB/s': '',
                    'Kbit/s': '',
                    'Tr/s': '',
                    'unset': 'no-max-width'
                }
            },
            myIntegrationTagId            : 'mine',
            companyLink                   : '',
            companyName                   : '',
            copyrightYear                 : '',
            feedbackEnabled               : '',
            footerItems                   : '',
            integrationFilterItems        : '',
            integrationFilterLimitation   : '',
            integrationStoreEnabled       : '',
            publicDownloads               : '',
            publicReleases                : '',
            trafficRelayHost              : '',
            supportLink                   : '',
            privacyLink                   : '',
            cloudName                     : '',
            vmsName                       : '',
            pushConfig                    : '',
            googleTagManagerId            : '',
            systemThrottleTime            : 5000,
        };
    }

    getConfig() {
        return this.config;
    }
}
