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
import { TranslateService }                  from '@ngx-translate/core';

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
    downloads: any;
    downloadsData: any;
    lang: any;
    platformMatch: {};
    canSeeHistory: boolean;
    tabsVisible: boolean;
    sortedPlatforms: any;

    location: Location;

    @ViewChild('tabs')
    public tabs: NgbTabset;

    // TODO: Fix arm supported. It says the same thing as linux

    private setupDefaults() {

        this.config = this.configService.getConfig();

        this.canViewDownloads = false;
        this.tabsVisible = false;
        this.downloads = {... this.config.downloads};

        this.downloadsData = {
            version   : '',
            installers: [ { platform: '', appType: '' } ],
            releaseUrl: ''
        };

        this.sortedPlatforms = [];

        this.platformMatch = {
            unix   : 'Linux',
            linux  : 'Linux',
            mac  : 'MacOS',
            windows: 'Windows',
            arm    : 'Arm',
            sdk    : 'SDK'
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
                private translate: TranslateService,
                location: Location) {

        this.location = location;
        this.setupDefaults();
    }

    private detectOS(): string {
        return this.platformMatch[ this.deviceService.getDeviceInfo().os ];
    }

    public beforeChange($event: NgbTabChangeEvent) {
        this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + $event.nextId);
        this.locationProxy.path('/download/' + $event.nextId, false);
    }

    private getDownloads() {
        // TODO: Commented until we ged rid of AJS
        // this.routeData = this.route.snapshot.data;

        this.sub = this.route.params.subscribe(params => {
            // TODO: Commented until we ged rid of AJS
            // this.platform = params['platform'];
            this.routeParamPlatform = this.routeParamPlatform && this.routeParamPlatform.toLowerCase();
            this.platform = (this.routeParamPlatform in this.platformMatch ? this.platformMatch[this.routeParamPlatform] : this.detectOS()).toLowerCase();

            // TODO: Commented until we ged rid of AJS
            // this.activeOs = this.platform || this.platformMatch[this.routeData.platform.os];

            for (const mobile in this.downloads.mobile) {
                if (this.downloads.mobile[ mobile ].os === this.activeOs) {
                    if (this.lang.downloads.mobile[ this.downloads.mobile[ mobile ].name ].link !== 'disabled') {
                        this.document.location.href = this.lang.downloads.mobile[ this.downloads.mobile[ mobile ].name ].link;
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
                // Sorts platforms based on order defined in nx-config service
                Object.keys(this.config.downloads.groups).forEach((key) => {
                    const checkPlatform = this.config.downloads.groups[key];
                    const platform = this.downloadsData.platforms.find((downloadsPlatform) => {
                        return downloadsPlatform.name === checkPlatform.name;
                    });
                    if (platform) {
                        platform.files = platform.files.filter((installer) => {
                            switch (platform.name) {
                                case 'sdk':
                                    return installer.path.indexOf('sdk') > -1;
                                default:
                                    return this.downloads.groups[platform.name].appTypes.includes(installer.appType);
                            }
                        }).map((installer) => {
                            const translatedPlatform = this.lang.downloads.platforms[installer.platform] || installer.platform;
                            const translatedAppType = this.lang.downloads.appTypes[installer.appType] || this.lang.downloads.appTypes.package;
                            installer.formatName = `${translatedPlatform} - ${translatedAppType}`;
                            installer.url = `${this.downloadsData.releaseUrl}${installer.path}`;
                            return installer;
                        });

                        if (platform.files.length > 0) {
                            this.sortedPlatforms.push(platform);
                        }
                    }
                });

                if (!this.sortedPlatforms.some(platform => platform.name === this.platform)) {
                    this.platform = this.detectOS().toLowerCase();
                }

                this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + this.platform);

                setTimeout(() => {
                    this.tabsVisible = true;
                    if (this.tabs) {
                        this.tabs.select(this.platform);
                    }
                });
            });
    }

    ngOnInit(): void {
        this.translate.getTranslation(this.translate.currentLang).subscribe((lang) => {
            this.lang = lang;
        });
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
