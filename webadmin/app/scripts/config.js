'use strict';
/*exported Config */

var Config = {

    defaultLogin: 'admin',
    defaultPassword: 'admin',

    globalEditServersPermissions:0x00000020 ,
    globalViewArchivePermission:0x00000100,
    globalViewLivePermission:0x00000080,

    cloud:{
        apiUrl: 'http://cloud-demo.hdw.mx/api',
        portalWhiteList:'http://cloud-demo.hdw.mx/**',
        portalUrl: 'http://cloud-demo.hdw.mx',
        portalRegisterUrl: 'http://cloud-demo.hdw.mx/static/index.html#/register',

        portalSystemUrl: 'http://cloud-demo.hdw.mx/static/index.html#/systems/{systemId}?inline',
        portalConnectUrl: 'http://cloud-demo.hdw.mx/static/index.html#/systems/connect/{systemName}?inline',
        portalDisconnectUrl: 'http://cloud-demo.hdw.mx/static/index.html#/systems/{systemId}/disconnect?inline'
    },

    cloudLocalhost:{
        portalWhiteList:'http://localhost:8000/**',
        portalUrl: 'http://localhost:8000',
        portalRegisterUrl: 'http://localhost:8000/static/index.html#/register',

        portalSystemUrl: 'http://localhost:8000/static/index.html#/systems/{systemId}?inline',
        portalConnectUrl: 'http://localhost:8000/static/index.html#/systems/connect/{systemName}?inline',
        portalDisconnectUrl: 'http://localhost:8000/static/index.html#/systems/{systemId}/disconnect?inline'
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
    ]
};
