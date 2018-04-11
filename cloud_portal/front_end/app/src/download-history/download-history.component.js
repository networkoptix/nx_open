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
const ngx_device_detector_1 = require("ngx-device-detector");
let DownloadHistoryComponent = class DownloadHistoryComponent {
    constructor(language, cloudApi, configService, authorizationService, document, route, router, titleService, deviceService) {
        this.language = language;
        this.cloudApi = cloudApi;
        this.configService = configService;
        this.authorizationService = authorizationService;
        this.document = document;
        this.route = route;
        this.router = router;
        this.titleService = titleService;
        this.deviceService = deviceService;
        this.deviceInfo = null;
    }
    beforeChange($event) {
        const type = $event.nextId;
        this.activeBuilds = this.downloadsData[type];
    }
    ;
    ngOnInit() {
        this.downloads = this.configService.config.downloads;
        this.sub = this.route.params.subscribe(params => {
            this.build = params['build'];
            console.log(this.build);
            this.authorizationService.requireLogin().then(() => {
                this.cloudApi
                    .getDownloadsHistory(this.build)
                    .then(result => {
                    this.linkbase = result.data.updatesPrefix;
                    if (!this.build) { // only one build
                        this.activeBuilds = result.data.releases;
                        this.downloadTypes = ["releases", "patches", "betas"];
                        this.downloadsData = result.data;
                    }
                    else {
                        this.activeBuilds = [result.data];
                        this.downloadTypes = [result.data.type];
                        this.downloadsData = {};
                        this.downloadsData[result.data.type] = this.activeBuilds;
                    }
                }, error => {
                    this.router.navigate(['404']); // Can't find downloads.json in specific build
                });
            });
        });
    }
    ngOnDestroy() {
        this.sub.unsubscribe();
    }
    ngAfterViewChecked() {
        // setTimeout(() => {
        //     if (this.tabs) {
        //         this.tabs.select(this.activeOs);
        //     }
        // });
    }
};
__decorate([
    core_1.ViewChild('tabs'),
    __metadata("design:type", ng_bootstrap_1.NgbTabset)
], DownloadHistoryComponent.prototype, "tabs", void 0);
DownloadHistoryComponent = __decorate([
    core_1.Component({
        selector: 'download-history-component',
        templateUrl: 'download-history.component.html'
    }),
    __param(0, core_1.Inject('languageService')),
    __param(1, core_1.Inject('cloudApiService')),
    __param(2, core_1.Inject('configService')),
    __param(3, core_1.Inject('authorizationCheckService')),
    __param(4, core_1.Inject(common_1.DOCUMENT)),
    __metadata("design:paramtypes", [Object, Object, Object, Object, Object, router_1.ActivatedRoute,
        router_1.Router,
        platform_browser_1.Title,
        ngx_device_detector_1.DeviceDetectorService])
], DownloadHistoryComponent);
exports.DownloadHistoryComponent = DownloadHistoryComponent;
//# sourceMappingURL=download-history.component.js.map