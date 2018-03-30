import { Component, OnInit, OnDestroy, ChangeDetectorRef, Inject } from '@angular/core';
import { ActivatedRoute, Router }                                  from '@angular/router';
import { Title }                                                   from "@angular/platform-browser";
import { DOCUMENT }                                                from "@angular/common";
import { NgbTabChangeEvent }                                       from "@ng-bootstrap/ng-bootstrap";


@Component({
    selector: 'download-component',
    templateUrl: './download/download.component.html'
})

export class DownloadComponent implements OnInit, OnDestroy {
    private sub: any;
    private platform: any;

    installers: any;
    downloads: any;
    downloadsData: any;
    platformMatch = {
        'Open BSD': 'Linux',
        'Sun OS': 'Linux',
        'QNX': 'Linux',
        'UNIX': 'Linux',
        'BeOS': 'Linux',
        'OS/2': 'Linux',

        'Mac OS X': 'MacOS',
        'Mac OS': 'MacOS'
    };

    constructor(@Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('configService') private configService: any,
                @Inject(DOCUMENT) private document: any,
                private route: ActivatedRoute,
                private router:Router,
                private titleService: Title) {

        this.downloadsData = {
            version: '',
            installers: [{platform: '', appType: ''}],
            releaseUrl: ''
        };

        this.installers = [{}];
    }

    public beforeChange($event: NgbTabChangeEvent) {
        const platform = $event.nextId;

        this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + platform);

        this.router.navigate(['/download', platform]);
    };

    ngOnInit(): void {
        this.sub = this.route.params.subscribe(params => {
            console.log(params['platform']);
            this.platform = params['platform'];
        });

        this.downloads = this.configService.config.downloads;

        console.log('BEFORE', this.downloads.groups);

        this.cloudApi.getDownloads().then(data => {
            this.downloadsData = data.data;

            this.downloads.groups.forEach(platform => {

                platform.installers.filter(installer => {
                    const targetInstaller = this.downloadsData.installers.find(existingInstaller => {

                        return installer.platform === existingInstaller.platform &&
                            installer.appType === existingInstaller.appType;
                    });

                    if (targetInstaller) {
                        Object.assign(installer, targetInstaller);
                        installer.formatName = this.language.lang.downloads.platforms[installer.platform] + ' - ' + this.language.lang.downloads.appTypes[installer.appType];
                        installer.url = this.downloadsData.releaseUrl + installer.path;
                    }
                    return !!targetInstaller;
                })[0];
                console.log('AFTER', this.downloads.groups);
            });

            console.log(this.downloadsData);

        });

        // console.log(this.downloads.groups);

        let foundPlatform = false,
            activeOs = this.platform; // || this.platformMatch[window.jscd.os] || window.jscd.os;

        this.downloads.groups.forEach(platform => {
            // TODO: activate the platform tab
            //platform.active = (platform.os || platform.name) === activeOs;
            foundPlatform = platform.active || foundPlatform;
        });

        for (let mobile in this.downloads.mobile) {
            if (this.downloads.mobile[mobile].os === activeOs) {
                if (this.language.lang.downloads.mobile[this.downloads.mobile[mobile].name].link !== 'disabled') {
                    this.document.location.href = this.language.lang.downloads.mobile[this.downloads.mobile[mobile].name].link;
                    return;
                }
                break;
            }
        }

        if (this.platform && !foundPlatform) {
            this.router.navigate(['404']);

            return;
        }
        if (!foundPlatform) {
            this.downloads.groups[0].active = true;
        }


    }

    ngOnDestroy() {
        this.sub.unsubscribe();
    }
}

