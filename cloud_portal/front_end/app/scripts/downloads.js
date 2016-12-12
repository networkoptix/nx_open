'use strict';

var DownloadsConfig = {
    version: '2.5.0.11500',
    readMoreLink: 'http://www.networkoptix.com/all-nx-witness-release-notes/',

    mobile:[
        {
            os: 'iOS',
            src: 'https://itunes.apple.com/eg/app/hd-witness/id648369716'
        },
        {
            os: 'Android',
            src: 'https://play.google.com/store/apps/details?id=com.networkoptix.hdwitness'
        }
    ],
    platforms:[
        {
            os: 'Windows',
            name: 'Windows',
            description: 'Vista, 7, 8, 10, Server 2003, Server 2008, Server 2012.',

            installers:[
                {
                    name: 'Windows x64 - Client & Server',
                    src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x64.exe'
                },
                {
                    name: 'x64 - Client only',
                    src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x64-client-only.msi'
                },
                {
                    name: 'x64 - Server only',
                    src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x64-server-only.msi'
                },
                {
                    name: 'x86 - Client only',
                    src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x86-client-only.msi'
                },
                {
                    name: 'x86 - Server only',
                    src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x86-server-only.msi'
                },
                {
                    name: 'x86 - Client & Server',
                    src: 'http://updates.networkoptix.com/default/11500/windows/networkoptix-nxwitness-2.5.0.11500-x86.exe'
                }
            ]
        },
        {
            os: 'Linux',
            name: 'Ubuntu Linux',
            description: 'Ubuntu 14.04 LTS, 12.04 LTS, 10.04 LTS',
            installers:[
                {
                    name: 'Ubuntu x64 - Client',
                    src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-client-2.5.0.11500-x64-release.deb'
                },
                {
                    name: 'x64 - Server',
                    src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-mediaserver-2.5.0.11500-x64-release.deb'
                },
                {
                    name: 'x86 - Server',
                    src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-mediaserver-2.5.0.11500-x86-release.deb'
                },
                {
                    name: 'x86 - Client',
                    src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-client-2.5.0.11500-x86-release.deb'
                }
            ]
        },
        {
            os: 'MacOS',
            name: 'Mac OS',
            description: 'OSX 10.11 El Capitan 10.10 Yosemite, 10.9 Mavericks, 10.8 Mountain Lion.',
            installers:[
                {
                    name: 'Mac OSX - Client',
                    src: 'http://updates.networkoptix.com/default/11500/linux/nxwitness-client-2.5.0.11500-x64-release.deb'
                }
            ]
        }
    ]
}
