Config.helpLinks.push({
    url: "http://support.networkoptix.com",
    title: "Support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "http://networkoptix.com/calculator/#/",
    title: "Hardware calculator",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "https://itunes.apple.com/eg/app/hd-witness/id648369716?mt=8",
    title: "iOS Client",
    target: "new" // new|frame
});

Config.helpLinks.push({
    urls: [
        {
            url: "https://itunes.apple.com/eg/app/hd-witness/id1050899754?mt=8",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "https://play.google.com/store/apps/details?id=com.networkoptix.nxwitness",
            class:'googleplay',
            button: "Android Client"
        }
    ],
    title: "Mobile Apps",
    description:"View live and recorded video from your VMS system on your iOs or Android mobile device",
    target: "new" // new|frame
});

Config.allowDebugMode = false;
//Config.webclientEnabled = false; // Uncomment this string to disable web client
