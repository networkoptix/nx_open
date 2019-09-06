import {
    Component, Inject,
    OnInit, PLATFORM_ID,
    ViewEncapsulation
} from '@angular/core';
import { isPlatformBrowser, Location }           from '@angular/common';
import { BreakpointObserver, BreakpointState }   from '@angular/cdk/layout';
import { ActivatedRoute, NavigationEnd, Router } from '@angular/router';
import { TranslateService }                      from '@ngx-translate/core';
import { CamerasService }          from '../../services/cameras.service';
import { IpvdSearchService }       from './ipvd-search.service';
import { NxModalMessageComponent } from '../../dialogs/message/message.component';
import { NxConfigService }         from '../../services/nx-config';
import { NxUriService }            from '../../services/uri.service';
import { NxUtilsService }          from '../../services/utils.service';
import { Title }                   from '@angular/platform-browser';

interface Params {
    [key: string]: any;
}

@Component({
    selector     : 'ipvd',
    templateUrl  : 'ipvd.component.html',
    styleUrls    : ['ipvd.component.scss'],
    encapsulation: ViewEncapsulation.None
})

export class NxIpvdComponent implements OnInit {
    lang: any = {};
    CONFIG: any = {};
    placeholder: string;
    data: any;
    company: any;
    vendors: any = [];
    resolution: string;
    itemsPerPage: number;
    query: string;
    cameras: any;
    activeCamera: any;
    showAll: boolean;
    hardwareTypes: any[];
    resolutions: any;
    camerasTable: any;
    allowedParameters: string[];
    filterModel: any;
    toggleCamview: boolean;
    params: any;
    mobileDetailMode: boolean;
    noResult: boolean;
    hasNoSearch: boolean;
    debug: any;
    uriPath: string;
    breakpoint: string;


    private setupDefaults() {
        this.allowedParameters = [
            'vendor', 'model', 'hardwareType',
            'maxResolution', 'maxFps', 'primaryCodec', 'isAudioSupported',
            'isTwAudioSupported', 'isPtzSupported', 'isAptzSupported',
            'isFisheye', 'isMdSupported', 'isIoSupported', 'count', 'resolutionArea'
        ];

        this.breakpoint = '(max-width: 767px)';
        this.placeholder = '';
        this.data = undefined;
        this.resolution = '0';
        this.itemsPerPage = 15;
        this.query = '';
        this.noResult = false;
        this.hasNoSearch = true;
        this.cameras = [];
        this.camerasTable = [];
        this.vendors = undefined;

        this.activeCamera = undefined;
        this.showAll = false;
        this.toggleCamview = false;

        this.filterModel = {
            query: ''
        };
        this.filterModel.tags = [];
        this.filterModel.selects = [];
        this.filterModel.multiselects = [];

        this.resolutions = [];
        this.hardwareTypes = [];

        this.uriPath = '/' + this.route.snapshot.url[0].path;
    }

    constructor(private configService: NxConfigService,
                private translate: TranslateService,
                private cameraService: CamerasService,
                private cameraSearchService: IpvdSearchService,
                // TODO: Use dialog service when it is not being downgraded
                private messageDialog: NxModalMessageComponent,
                private uri: NxUriService,
                private route: ActivatedRoute,
                private location: Location,
                private breakpointObserver: BreakpointObserver,
                private router: Router,
                private title: Title,
                @Inject(PLATFORM_ID) private platformId: object) {

        this.setupDefaults();

        if (isPlatformBrowser(this.platformId)) {
            this.router.events.subscribe((event: NavigationEnd) => {
                window.scroll(0, this.uri.pageOffset);
            });
        }

        this.location.subscribe((event: PopStateEvent) => {
            // force view component update without URI update
            setTimeout(() => {
                if (!this.params.camera && this.activeCamera) {
                    this.resetActiveCamera(true);
                }
            });
        });

        this.CONFIG = this.configService.getConfig();
    }

    ngOnInit() {
        // Example URI
        // /ipvd?vendors=30X&camera=IPPTZ-ELS2IRL30X-ATI
        this.uri
            .getURI()
            .subscribe(params => {
                this.params = params;
                this.debug = params.debug === '' || false;
                if (Object.keys(this.params).length !== 0) {
                    this.hasNoSearch = false;
                }
            });

        setTimeout(() => {
            this.translate
                .getTranslation(this.translate.currentLang)
                .subscribe((lang) => {
                    this.lang = lang;
                    this.title.setTitle(this.lang.pageTitles.supportedDevices);

                    this.company = this.CONFIG.companyName;
                    this.placeholder = this.lang.search.search_ipvd;

                    // add hardware types and tags
                    this.addFilterTags();
                    this.addFilterTypes();
                    this.addFilterResolutions();

                    this.activate();
                });
        });

        this.breakpointObserver
            .observe([this.breakpoint])
            .subscribe((state: BreakpointState) => {
                this.mobileDetailMode = (state.matches && this.activeCamera);
            });
    }

    addFilterResolutions() {
        this.resolutions = JSON.parse(this.CONFIG.ipvd.supportedResolutions);

        this.filterModel.selects = [
            {
                id      : 'resolution',
                label   : this.lang.search.minResolution,
                items   : this.resolutions,
                selected: this.resolutions[0]
            }
        ];
    }

    setActiveCamera() {
        if (this.params.camera) {
            this.uri.pageOffset = window.pageYOffset;
            const selectedCamera = this.cameras.find((camera) => camera.model === this.params.camera);
            this.activateCamera(selectedCamera);
        }
    }

    addFilterTags() {
        this.filterModel.tags = JSON.parse(this.CONFIG.ipvd.searchTags);
        this.filterModel.tags.forEach(tag => tag.label = this.lang.ipvd[tag.id]);
    }

    addFilterTypes() {
        this.hardwareTypes = JSON.parse(this.CONFIG.ipvd.supportedHardwareTypes);
        this.hardwareTypes.forEach(type => {
            type.label = this.lang.ipvd[type.id];
        });

        this.filterModel.multiselects = [
            {
                id      : 'hardwareTypes',
                label   : this.lang.search.hardwareTypes,
                singular: this.lang.search.hardwareType,
                items   : this.hardwareTypes,
                selected: []
            }
            // vendors will be added later
        ];
    }

    modelChanged(model) {
        this.searchVendor();
    }

    reset() {
        this.cameraService
            .reloadIPVD()
            .subscribe();
    }

    activate() {
        this.cameraService
            .getIPVD()
            .subscribe(data => {
                this.cameras = data.cameras;
                this.vendors = data.vendors;
                this.vendors.sort(NxUtilsService.byParam((elm) => {
                    return elm.name.toLowerCase();
                }, NxUtilsService.sortASC));

                // reformat vendors to fit the multiselect component
                this.filterModel
                    .multiselects.unshift(
                        {
                            id      : 'vendors',
                            label   : this.lang.search.vendors,
                            singular: this.lang.search.vendor,
                            items   : this.vendors.map(v => (
                                    { id: v.name, label: v.name }
                            )),
                            selected: []
                        });

                // Trigger model change for search component
                this.filterModel = { ...this.filterModel };
            });
    }

    // restrict the parameters to be passed and viewed for to cam-table (based on allowedParameters)
    preFilterCameraTable(cameras) {
        const values = Object.keys(cameras).map(key => cameras[key]);
        const filteredCameras = [];
        values.forEach(camera => {
            const filteredCamera = Object.keys(camera)
                                         .filter(key => this.allowedParameters.indexOf(key) > -1 || key === 'sortKey')
                                         .reduce((obj, key) => {
                                             obj[key] = camera[key];
                                             return obj;
                                         }, {});
            filteredCameras.push(filteredCamera);
        });
        return filteredCameras;
    }

    filterEmpty() {
        let tags = false;
        if (this.filterModel.tags) {
            tags = this.filterModel.tags.find(tag => tag.value === true);
        }

        let multiselect = false;
        if (this.filterModel.multiselects) {
            this.filterModel.multiselects.forEach(select => {
                multiselect = multiselect || (select.selected.length > 0);
            });
        }

        let singleselect = false;
        if (this.filterModel.selects) {
            this.filterModel.selects.forEach(select => {
                singleselect = singleselect || (select.selected.value > 0); // 0 is default choice
            });
        }

        return tags || multiselect || singleselect || this.filterModel.query !== '';
    }

    searchVendor() {
        if (!this.params.camera && this.activeCamera) {
            this.resetActiveCamera();
        }

        if (this.cameras) {
            if (this.filterEmpty()) {
                this.cameraSearchService
                    .ipvdSearch(this.cameras, this.filterModel)
                    .subscribe(cameras => {
                        this.noResult = (cameras.length === 0);
                        if (!this.noResult) {
                            this.camerasTable = this.preFilterCameraTable(cameras);
                        } else {
                            this.camerasTable = [];
                        }
                        this.setActiveCamera();
                    });
            } else {
                this.hasNoSearch = true;
                this.noResult = false;
                this.camerasTable = [];
                this.resetActiveCamera();
                this.uri.resetURI(this.uriPath);
            }
        }
    }

    setVendor(vendor) {
        this.filterModel.query = vendor;
        this.searchVendor();
        return false;
    }

    activateCamera(elementSelected: any): void {
        if (!elementSelected) {
            return;
        }
        if (Object.keys(elementSelected).length === 0 || elementSelected.key === -1) {
            // call was not initiated by linking the element in HTML
            // this.resetActiveCamera();
            return;
        }

        const selectedCamera = this.cameras.find((camera) => {
            return camera.sortKey === (elementSelected.sortKey || elementSelected.value.sortKey);
        });

        setTimeout(() => {
            if (this.activeCamera && this.activeCamera.sortKey === selectedCamera.sortKey) {
                return;
            }
            this.activeCamera = {... selectedCamera};
            this.showAll = false;

            const queryParams: Params = {};
            queryParams.camera = selectedCamera.model || selectedCamera.value.model;
            this.uri.updateURI(this.uriPath, queryParams);

            if (this.breakpointObserver.isMatched(this.breakpoint)) {
                this.mobileDetailMode = true;
            }

            if (typeof this.activeCamera.firmwares === 'string') {
                const firmwares = JSON.parse(this.activeCamera.firmwares);
                let firmwaresArray = [];

                let maxFirmwareCount = 0,
                    totalCameraCount = 0;

                Object.keys(firmwares).forEach(key => {
                    const count = firmwares[key];
                    firmwaresArray.push({ name: key, count });

                    totalCameraCount += count;
                    if (count > maxFirmwareCount) {
                        maxFirmwareCount = count;
                    }
                });

                firmwaresArray = firmwaresArray.sort((a, b) => {
                    return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
                });

                firmwaresArray.reverse();

                this.activeCamera.maxFirmwareCount = maxFirmwareCount;
                this.activeCamera.totalCameraCount = totalCameraCount;
                this.activeCamera.firmwares = firmwaresArray;
            }

            this.toggleCamview = true;
        }, 10);
    }


    openFeedback(param) {
        const type = (param === 'device') ? this.CONFIG.messageType.ipvd_device : this.CONFIG.messageType.ipvd_page;
        const device = (this.activeCamera) ? this.activeCamera.model : '';
        this.messageDialog
            .open(type, device, device)
            .then(() => {
            });

        return false;
    }

    resetActiveCamera(skipUpdateURI?) {
        if (!skipUpdateURI) {
            const queryParams: Params = {};
            queryParams.camera = undefined;
            this.uri.updateURI(this.uriPath, queryParams);
        }

        this.activeCamera = undefined;
        this.mobileDetailMode = false;
        this.toggleCamview = false;
    }
}
