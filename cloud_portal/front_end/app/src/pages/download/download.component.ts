import {
    Component, OnInit, OnDestroy,
    ViewChild, Inject, Input
}                                       from '@angular/core';
import { ActivatedRoute, Router }       from '@angular/router';
import { Title }                        from '@angular/platform-browser';
import { DOCUMENT, Location }           from '@angular/common';
import { NgbTabChangeEvent, NgbTabset } from '@ng-bootstrap/ng-bootstrap';
import { DeviceDetectorService }        from 'ngx-device-detector';

@Component({
    selector: 'download-component',
    templateUrl: 'download.component.html',
    styleUrls: ['download.component.scss']
})

export class DownloadComponent implements OnInit, OnDestroy {
    @Input() routeParamPlatform;

    private sub: any;
    private platform: any;
    private activeOs: string;
    private routeData: any;
    private userAuthorized: boolean;

    installers: any;
    downloads: any;
    downloadsData: any;
    platformMatch: {};
    showTabs: string;

    location: Location;

    @ViewChild('tabs')
    public tabs: NgbTabset;

    private setupDefaults() {
        this.showTabs = 'visible';
        this.userAuthorized = false;
        this.downloads = this.configService.config.downloads;

        this.downloadsData = {
            version: '',
            installers: [{platform: '', appType: ''}],
            releaseUrl: ''
        };

        this.installers = [{}];

        this.platformMatch = {
            'unix': 'Linux',
            'linux': 'Linux',
            'mac': 'MacOS',
            'windows': 'Windows'
        };
    }

    constructor(@Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('configService') private configService: any,
                @Inject('authorizationCheckService') private authorizationService: any,
                @Inject(DOCUMENT) private document: any,
                private deviceService: DeviceDetectorService,
                private route: ActivatedRoute,
                private router: Router,
                private titleService: Title,
                location: Location) {

        this.location = location;
        this.setupDefaults();
    }

    private detectOS(): string {
        return this.platformMatch[this.deviceService.getDeviceInfo().os];
    }

    private getDownloadersPer(platform: string) {
        this.cloudApi
            .getDownloads()
            .then(data => {
                this.downloadsData = data.data;

                this.downloads.groups.forEach(platform => {
                    return platform.installers.filter(installer => {
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
                });
            });
    }

    public beforeChange($event: NgbTabChangeEvent) {
        const platform = $event.nextId;

        if (this.location.path() === '/download/' + platform) {
            return;
        }

        this.showTabs = 'hidden';

        // TODO: Repace this once 'register' page is moved to A5
        // AJS and A5 routers freak out about route change *****
        // this.router.navigate(['/download', platform]);
        this.document.location.href = '/download/' + platform;
    }

    private getDownloads () {
        this.userAuthorized = true;
        // TODO: Commented until we ged rid of AJS
        //this.routeData = this.route.snapshot.data;

        this.sub = this.route.params.subscribe(params => {
            // TODO: Commented until we ged rid of AJS
            //this.platform = params['platform'];
            this.platform = this.routeParamPlatform || this.detectOS();

            // TODO: Commented until we ged rid of AJS
            //this.activeOs = this.platform || this.platformMatch[this.routeData.platform.os];
            this.activeOs = this.platform || this.platformMatch[this.platform];

            for (let mobile in this.downloads.mobile) {
                if (this.downloads.mobile[mobile].os === this.activeOs) {
                    if (this.language.lang.downloads.mobile[this.downloads.mobile[mobile].name].link !== 'disabled') {
                        this.document.location.href = this.language.lang.downloads.mobile[this.downloads.mobile[mobile].name].link;
                        return;
                    }
                    break;
                }
            }

            let foundPlatform = false;
            this.downloads.groups.forEach(platform => {
                foundPlatform = ((platform.os || platform.name) === this.activeOs) || foundPlatform;
            });

            if (this.platform && !foundPlatform) {
                this.router.navigate(['404']);

                return;
            }

            if (!foundPlatform) {
                this.downloads.groups[0].active = true;
            }
        });

        this.getDownloadersPer(this.platform);
        this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + this.platform);

        setTimeout(() => {
            if (this.tabs) {
                this.tabs.select(this.activeOs);
            }
        });
    }

    ngOnInit(): void {

        if (!this.configService.publicDownloads) {
            this.authorizationService
                .requireLogin()
                .then((result) => {
                    if (!result) {
                        this.document.location.href = this.configService.config.redirectUnauthorised;
                        return;
                    }

                    this.getDownloads();
                });

        } else {
            this.getDownloads();
        }
    }

    ngOnDestroy() {
        this.sub.unsubscribe();
    }
}

