'use strict';

var DownloadsConfig = {
    version: '2.5.0.11500',
    description: 'Download our amazing software and see everything yourself!',
    readMoreLink: 'http://www.networkoptix.com/all-nx-witness-release-notes/',

    platforms:[
        {
            name: 'Windows',
            description: '<b>Supported versions:</b> XP, Vista, 7, 8, 10, Server 2003, Server 2008, Server 2012.',
    
            linksGroups:[
                {
                    name: 'Windows 10, Windows 8, Window 7, Windows Server (64-bit)',
                    links:[
                        {
                            name: 'Server and client app (x64)',
                            src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x64.exe',
                            classes: 'btn btn-primary'
                        },
                        {
                            name: 'Client only (x64)',
                            src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x64-client-only.msi',
                            classes: 'btn btn-default'
                        },
                        {
                            name: 'Server only (x64)',
                            src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x64-server-only.msi',
                            classes: 'btn btn-default'
                        }
                    ]
                },
    
                {
                    name: 'Windows Vista, Windows XP (32-bit)',
                    links:[
                        {
                            name: 'Server and client app (x86)',
                            src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x86.exe',
                            classes: 'btn btn-default'
                        },
                        {
                            name: 'Client only (x86)',
                            src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x86-client-only.msi',
                            classes: 'btn btn-default'
                        },
                        {
                            name: 'Server only (x86)',
                            src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x86-server-only.msi',
                            classes: 'btn btn-default'
                        }
                    ]
                }
            ]
        },
        {
            name: 'Linux',
            description: '<b>Supported versions:</b> Ubuntu 14.04 LTS, 12.04 LTS, 10.04 LTS',
    
            linksGroups:[
                {
                    name: 'Ubuntu 64-bit',
                    links:[
                        {
                            name: 'Server (x64)',
                            src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-mediaserver-2.5.0.11500-x64-release.deb',
                            classes: 'btn btn-primary'
                        },
                        {
                            name: 'Client app(x64)',
                            src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-client-2.5.0.11500-x64-release.deb',
                            classes: 'btn btn-default'
                        }
                    ]
                },
    
                {
                    name: 'Ubuntu 32-bit (x84)',
                    links:[
                        {
                            name: 'Server (x86)',
                            src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-mediaserver-2.5.0.11500-x86-release.deb',
                            classes: 'btn btn-default'
                        },
                        {
                            name: 'Client app (x86)',
                            src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-client-2.5.0.11500-x86-release.deb',
                            classes: 'btn btn-default'
                        }
                    ]
                }
            ]
        },
        {
            name: 'Mac OS',
            description: '<b>Supported versions:</b> OSX 10.11 El Capitan 10.10 Yosemite, 10.9 Mavericks, 10.8 Mountain Lion.',

            linksGroups:[
                {
                    links:[
                        {
                            name: 'Download Mac Desktop Client',
                            src: 'http://updates.networkoptix.com/default/11500/macos/networkoptix-nxwitness-client-2.5.0.11500-x64.dmg',
                            classes: 'btn btn-primary'
                        }
                    ]
                }
            ]
        },
        {
            name: 'iPhone, iPad (iOS)',
            description: '',

            linksGroups:[
                {
                    links:[
                        {
                            name: 'Get it on AppStore',
                            src: 'https://itunes.apple.com/eg/app/hd-witness/id648369716',
                            classes: 'btn btn-primary app-store'
                        }
                    ]
                }
            ]
        },
        {
            name: 'Android',
            description: '',

            linksGroups:[
                {
                    links:[
                        {
                            name: 'Get it on PlayMarket',
                            src: 'https://play.google.com/store/apps/details?id=com.networkoptix.hdwitness',
                            classes: 'btn btn-primary play-market'
                        }
                    ]
                }
            ]
        }
        
    ]
}
