import {
    Component, OnInit, OnDestroy,
    ViewChild, Inject, Input
}                                            from '@angular/core';
import { ActivatedRoute, Router }            from '@angular/router';
import { Title }                             from '@angular/platform-browser';
import { DOCUMENT, Location, TitleCasePipe } from '@angular/common';
import { NgbTabChangeEvent, NgbTabset }      from '@ng-bootstrap/ng-bootstrap';
import { DeviceDetectorService }             from 'ngx-device-detector';

@Component({
    selector   : 'download-component',
    templateUrl: 'download.component.html',
    styleUrls  : [ 'download.component.scss' ]
})

export class DownloadComponent implements OnInit, OnDestroy {
    @Input() routeParamPlatform;

    private sub: any;
    private platform: any;
    private activeOs: string;
    private routeData: any;
    private canViewDownloads: boolean;

    installers: any;
    downloads: any;
    downloadsData: any;
    platformMatch: {};
    canSeeHistory: boolean;

    location: Location;

    @ViewChild('tabs')
    public tabs: NgbTabset;

    private setupDefaults() {

        this.canViewDownloads = false;
        this.downloads = this.configService.config.downloads;

        this.downloadsData = {
            version   : '',
            installers: [ { platform: '', appType: '' } ],
            releaseUrl: ''
        };

        this.installers = [ {} ];

        this.platformMatch = {
            unix   : 'Linux',
            linux  : 'Linux',
            mac    : 'MacOS',
            windows: 'Windows'
        };
    }

    constructor(@Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                @Inject('configService') private configService: any,
                @Inject('account') private account: any,
                @Inject('authorizationCheckService') private authorizationService: any,
                @Inject('locationProxyService') private locationProxy: any,
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
        return this.platformMatch[ this.deviceService.getDeviceInfo().os ];
    }

    private getDownloadsInfo() {
        this.downloads.groups.forEach(platform => {
            return platform.installers.filter(installer => {
                const targetInstaller = this.downloadsData.installers.find(existingInstaller => {

                    return installer.platform === existingInstaller.platform &&
                        installer.appType === existingInstaller.appType;
                });

                if (targetInstaller) {
                    Object.assign(installer, targetInstaller);
                    installer.formatName = this.language.lang.downloads.platforms[ installer.platform ] + ' - ' + this.language.lang.downloads.appTypes[ installer.appType ];
                    installer.url = this.downloadsData.releaseUrl + installer.path;
                }
                return !!targetInstaller;
            })[ 0 ];
        });
    }

    public beforeChange($event: NgbTabChangeEvent) {
        this.getDownloadsInfo();
        this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + $event.nextId);
        this.locationProxy.path('/download/' + $event.nextId, false);
    }

    private getDownloads() {
        // TODO: Commented until we ged rid of AJS
        // this.routeData = this.route.snapshot.data;

        this.sub = this.route.params.subscribe(params => {
            // TODO: Commented until we ged rid of AJS
            // this.platform = params['platform'];
            this.platform = this.routeParamPlatform || this.detectOS();

            // TODO: Commented until we ged rid of AJS
            // this.activeOs = this.platform || this.platformMatch[this.routeData.platform.os];
            this.activeOs = this.platform || this.platformMatch[ this.platform ];

            for (const mobile in this.downloads.mobile) {
                if (this.downloads.mobile[ mobile ].os === this.activeOs) {
                    if (this.language.lang.downloads.mobile[ this.downloads.mobile[ mobile ].name ].link !== 'disabled') {
                        this.document.location.href = this.language.lang.downloads.mobile[ this.downloads.mobile[ mobile ].name ].link;
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
                this.location.go('404');

                return;
            }

            if (!foundPlatform) {
                this.downloads.groups[ 0 ].active = true;
            }
        });

        this.cloudApi
            .getDownloads()
            .then(data => {
                this.downloadsData = data.data;
                this.getDownloadsInfo();
            });

        this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + this.platform);

        setTimeout(() => {
            if (this.tabs) {
                this.tabs.select(this.activeOs);
            }
        });
    }

    ngOnInit(): void {
        this.account
            .get()
            .then(result => {
                this.canSeeHistory = (this.configService.config.publicReleases ||
                    result.is_superuser ||
                    result.permissions.indexOf(this.configService.config.permissions.canViewRelease) > -1);
            });

        if (!this.configService.config.publicDownloads) {
            this.authorizationService
                .requireLogin()
                .then(result => {
                    if (!result) {
                        this.document.location.href = this.configService.config.redirectUnauthorised;
                        return;
                    }

                    this.canViewDownloads = true;
                    this.getDownloads();
                });

        } else {
            this.canViewDownloads = true;
            this.getDownloads();
        }
    }

    ngOnDestroy() {
        this.sub.unsubscribe();
    }
}
