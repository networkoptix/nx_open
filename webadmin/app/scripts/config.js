'use strict';

var Config = {
    globalEditServersPermissions:0x00000020 ,
    globalViewArchivePermission:0x00000100,
    globalViewLivePermission:0x00000080,

    demo:"/~ebalashov/webclient/api",
    demoMedia:"//10.0.2.186:7001",
    webclientEnabled: true, // set to false to disable webclient from top menu and show placeholder instead
    allowDebugMode:false, // Allow debugging at all. Set to false in production
    debug: {
        video: false, // videowindow.js - disable loader, allow rightclick
        videoFormat: false,//"flashls", // videowindow.js - force video player
        chunksOnTimeline: false // timeline.js - draw debug events
    },
    helpLinks:[
        // Additional Links to show in help
        /*{
            url: "#/support/",
            title: "Support",
            target: "" // new|frame
        }*/
    ]
};
