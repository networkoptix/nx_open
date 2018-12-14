import {
    Component, OnInit, OnDestroy,
    ViewChild, Inject, Input
}                                            from '@angular/core';
import { ActivatedRoute, Router }            from '@angular/router';
import { Title }                             from '@angular/platform-browser';
import { DOCUMENT, Location, TitleCasePipe } from '@angular/common';
import { NgbTabChangeEvent, NgbTabset }      from '@ng-bootstrap/ng-bootstrap';
import { DeviceDetectorService }             from 'ngx-device-detector';
import { NxConfigService }                   from '../../services/nx-config';

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

    config: any;
    installers: any;
    downloads: any;
    downloadsData: any;
    platformMatch: {};
    canSeeHistory: boolean;
    tabsVisible: boolean;
    foundPlatform: boolean;

    location: Location;

    @ViewChild('tabs')
    public tabs: NgbTabset;

    private setupDefaults() {

        this.config = this.configService.getConfig();

        this.foundPlatform = false;
        this.canViewDownloads = false;
        this.tabsVisible = false;
        this.downloads = this.config.downloads;

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
                @Inject('account') private account: any,
                @Inject('authorizationCheckService') private authorizationService: any,
                @Inject('locationProxyService') private locationProxy: any,
                @Inject(DOCUMENT) private document: any,
                private configService: NxConfigService,
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
        });

        this.cloudApi
            .getDownloads()
            .then(data => {
                this.downloadsData = data.data;

                this.downloads.groups = this.downloads.groups.filter(platform => {
                    return !this.downloadsData.platforms.every(avail => {
                        return avail.name !== platform.name;
                    });
                });

                this.foundPlatform = this.downloads.groups.some(platform => {
                    return ((platform.os || platform.name) === this.activeOs);
                });

                if (!this.foundPlatform) {
                    this.downloads.groups[0].active = true;
                    this.platform = this.downloads.groups[0].os;
                }

                this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + this.platform);

                setTimeout(() => {
                    this.tabsVisible = true;
                    if (this.tabs) {
                        this.tabs.select(this.foundPlatform ? this.activeOs : this.downloads.groups[0].name);
                    }
                });

                this.getDownloadsInfo();
            });
    }

    ngOnInit(): void {
        this.account
            .get()
            .then(result => {
                this.canSeeHistory = (this.config.publicReleases ||
                    result.is_superuser ||
                    result.permissions.indexOf(this.config.permissions.canViewRelease) > -1);
            });

        if (!this.config.publicDownloads) {
            this.authorizationService
                .requireLogin()
                .then(result => {
                    if (!result) {
                        this.document.location.href = this.config.redirectUnauthorised;
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
