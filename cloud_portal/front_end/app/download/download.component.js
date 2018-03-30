"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
const router_1 = require("@angular/router");
const platform_browser_1 = require("@angular/platform-browser");
const common_1 = require("@angular/common");
const ng_bootstrap_1 = require("@ng-bootstrap/ng-bootstrap");
let DownloadComponent = class DownloadComponent {
    constructor(language, cloudApi, configService, document, route, router, titleService) {
        this.language = language;
        this.cloudApi = cloudApi;
        this.configService = configService;
        this.document = document;
        this.route = route;
        this.router = router;
        this.titleService = titleService;
        this.downloadsData = {
            version: '',
            installers: [{ platform: '', appType: '' }],
            releaseUrl: ''
        };
        this.installers = [{}];
        this.platformMatch = {
            'Open BSD': 'Linux',
            'Sun OS': 'Linux',
            'QNX': 'Linux',
            'UNIX': 'Linux',
            'BeOS': 'Linux',
            'OS/2': 'Linux',
            'Mac OS X': 'MacOS',
            'Mac OS': 'MacOS'
        };
        this.downloads = this.configService.config.downloads;
    }
    beforeChange($event) {
        const platform = $event.nextId;
        this.titleService.setTitle(this.language.lang.pageTitles.downloadPlatform + platform);
        this.router.navigate(['/download', platform]);
    }
    ;
    ngOnInit() {
        this.sub = this.route.params.subscribe(params => {
            this.platform = params['platform'];
            this.activeOs = this.platform || this.platformMatch[window.jscd.os] || window.jscd.os;
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
            });
        });
    }
    ngOnDestroy() {
        this.sub.unsubscribe();
    }
    ngAfterViewChecked() {
        setTimeout(() => {
            if (this.tabs) {
                this.tabs.select(this.activeOs);
            }
        });
    }
};
__decorate([
    core_1.ViewChild('tabs'),
    __metadata("design:type", ng_bootstrap_1.NgbTabset)
], DownloadComponent.prototype, "tabs", void 0);
DownloadComponent = __decorate([
    core_1.Component({
        selector: 'download-component',
        templateUrl: './download/download.component.html'
    }),
    __param(0, core_1.Inject('languageService')),
    __param(1, core_1.Inject('cloudApiService')),
    __param(2, core_1.Inject('configService')),
    __param(3, core_1.Inject(common_1.DOCUMENT)),
    __metadata("design:paramtypes", [Object, Object, Object, Object, router_1.ActivatedRoute,
        router_1.Router,
        platform_browser_1.Title])
], DownloadComponent);
exports.DownloadComponent = DownloadComponent;
//# sourceMappingURL=download.component.js.map