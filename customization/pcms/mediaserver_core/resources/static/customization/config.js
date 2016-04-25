Config.helpLinks.push({
    url: "supportcn@foxmail.com",
    title: "Support",
    description:"Have a question about specific features of your Nx Witness system?",
    button:"get support",
    target: "new" // new|frame
});

Config.helpLinks.push({
    url: "supportcn@foxmail.com",
    title: "Hardware calculator",
    description:"Building a new Nx Witness system or expanding your current system? Use the Nx System Calculator to calculate suggested storage and network requirements.",
    button:"calculate",
    target: "new" // new|frame
});


Config.helpLinks.push({
    urls: [
        {
            url: "supportcn@foxmail.com",
            button: "iOS Client",
            class:'appstore'
        },
        {
            url: "supportcn@foxmail.com",
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
