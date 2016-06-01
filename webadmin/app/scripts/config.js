'use strict';
/*exported Config */

var Config = {

    defaultLogin: 'admin',
    defaultPassword: 'admin',
    newServerFlag: 'SF_NewSystem',
    publicIpFlag: 'SF_HasPublicIP',

    globalEditServersPermissions:0x00000020 ,
    globalViewArchivePermission:0x00000100,
    globalViewLivePermission:0x00000080,

    productName: 'Nx Witness',
    cloud:{
        productName: 'Nx Cloud',
        webadminSetupContext: '?from=webadmin&context=setup',
        clientSetupContext: '?from=client&context=setup',
        webadminContext:'?from=webadmin&context=settings',

        portalUrl: 'http://cloud-demo.hdw.mx',
        apiUrl: '/api',
        portalRegisterUrl: '/static/index.html#/register',
        portalSystemUrl: '/static/index.html#/systems/{systemId}'
    },

    demo:'/~ebalashov/webclient/api',
    demoMedia:'//10.0.2.186:7001',

    webclientEnabled: true, // set to false to disable webclient from top menu and show placeholder instead
    allowDebugMode: false, // Allow debugging at all. Set to false in production
    debug: {
        video: true, // videowindow.js - disable loader, allow rightclick
        videoFormat: false,//'flashls', // videowindow.js - force video player
        chunksOnTimeline: false // timeline.js - draw debug events
    },
    helpLinks:[
        // Additional Links to show in help
        /*{
            url: '#/support/',
            title: 'Support',
            target: '' // new|frame
        }*/
    ],

    settingsConfig: {
        auditTrailEnabled: {label:'Audit trail enabled', type:'checkbox'},
        cameraSettingsOptimization: {label:'Allow device setting optimization', type:'checkbox', setupWizard: true},
        disabledVendors: {label:'Disabled vendors', type:'text'},
        ec2AliveUpdateIntervalSec: {label:'System alive update interval', type:'number'},
        ec2ConnectionKeepAliveTimeoutSec: {label:'Connection keep alive timeout', type:'number'},
        ec2KeepAliveProbeCount: {label:'Connection keep alive probes', type:'number'},
        emailFrom: {label:'Email from', type:'text'},
        emailSignature: {label:'Email signature', type:'text'},
        emailSupportEmail: {label:'Support Email', type:'text'},
        ldapAdminDn: {label:'LDAP admin DN', type:'text'},
        ldapAdminPassword: {label:'LDAP admin password', type:'text'},
        ldapSearchBase: {label:'LDAP search base', type:'text'},
        ldapSearchFilter: {label:'LDAP search filter', type:'text'},
        ldapUri: {label:'LDAP URI', type:'text'},
        serverAutoDiscoveryEnabled: {label:'Enable device auto discovery', type:'checkbox', setupWizard: true},
        smtpConnectionType: {label:'SMTP connection type', type:'text'},
        smtpHost: {label:'SMTP host', type:'text'},
        smtpPort: {label:'SMTP port', type:'number'},
        smtpSimple: {label:'SMTP simple', type:'checkbox'},
        smtpTimeout: {label:'SMTP timeout', type:'number'},
        smptPassword: {label:'SMTP password', type:'text'},
        smtpUser: {label:'SMTP user', type:'text'},
        updateNotificationsEnabled: {label:'Update notifications enabled', type:'checkbox'},
        arecontRtspEnabled: {label:'Arecont RTSP Enabled', type:'checkbox'},
        backupNewCamerasByDefault: {label:'Backup new cameras by default',type:'checkbox'},
        statisticsAllowed: {label:'Send anonymous usage statistics and crash reports', type:'checkbox', setupWizard: true},
        backupQualities: {label:'Backup qualities',type:'text'},
        serverDiscoveryPingTimeoutSec:{label:'Server discovery timeout',type:'number'}
    }
};
