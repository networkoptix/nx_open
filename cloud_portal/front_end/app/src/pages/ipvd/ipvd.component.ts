import {
    Component,
    OnInit,
    DoCheck,
    KeyValueDiffers,
    ViewEncapsulation
}                                              from '@angular/core';
import { CamerasService }                      from '../../services/cameras.service';
import { IpvdSearchService }                   from './ipvd-search.service';
import { NxModalMessageComponent }             from '../../dialogs/message/message.component';
import { NxConfigService }                     from '../../services/nx-config';
import { TranslateService }                    from '@ngx-translate/core';
import { NxUriService }                        from '../../services/uri.service';
import { BreakpointObserver, BreakpointState } from '@angular/cdk/layout';
import { NxUtilsService }                      from '../../services/utils.service';

@Component({
    selector     : 'ipvd',
    templateUrl  : 'ipvd.component.html',
    styleUrls    : ['ipvd.component.scss'],
    encapsulation: ViewEncapsulation.None
})

export class NxIpvdComponent implements OnInit, DoCheck {
    lang: any = {};
    CONFIG: any = {};
    placeholder: string;
    data: any;
    company: any;
    vendors: any = [];
    resolution: string;
    itemsPerPage: number;
    query: string;
    allCameras: any;
    cameras: any;
    activeCamera: any;
    showAll: boolean;
    hardwareTypes: any[];
    resolutions: any;
    filter: any;
    emptyFilter: any;
    camerasTable: any;
    allowedParameters: string[];
    filterModel: any;
    differ: any;
    tagDiffer: any;
    resolutionDiffer: any;
    vendorDiffer: any;
    hardwareDiffer: any;
    toggleCamview: boolean;
    params: any;
    mobileDetailMode: boolean;

    hwtypes: any;
    private ASC = true;
    private DESC = false;

  private setupDefaults() {
      this.allowedParameters = [
          'vendor', 'model', 'hardwareType',
          'maxResolution', 'maxFps', 'primaryCodec', 'isAudioSupported',
          'isTwAudioSupported', 'isPtzSupported', 'isAptzSupported',
          'isFisheye', 'isMdSupported', 'isIoSupported', 'count'
      ];

      this.placeholder = '';
      this.data = undefined;
      this.resolution = '0';
      this.itemsPerPage = 15;
      this.query = '';

      this.allCameras = [];
      this.cameras = [];
      this.camerasTable = [];
      this.vendors = undefined;

      this.activeCamera = undefined;
      this.showAll = false;
      this.toggleCamview = false;

      this.filter = {};
      this.emptyFilter = {
            query: '',
            isPtzSupported: false,
            isAptzSupported: false,
            isAudioSupported: false,
            isTwAudioSupported: false,
            isIoSupported: false,
            isMdSupported: false,
            isFisheye: false,
            isH265: false,
            isMultiSensor: false
        };
      this.filterModel = this.emptyFilter;

      this.resolutions = [];
      this.hardwareTypes = [];
      this.filterModel.tags = [];
      this.filterModel.selects = [];
      this.filterModel.multiselects = [];
  }

    constructor(private configService: NxConfigService,
                private translate: TranslateService,
                private cameraService: CamerasService,
                private cameraSearchService: IpvdSearchService,
                // TODO: Use dialog service when it is not being downgraded
                private messageDialog: NxModalMessageComponent,
                private differs: KeyValueDiffers,
                private uri: NxUriService,
                private breakpointObserver: BreakpointObserver) {

        this.setupDefaults();
        this.differ = this.differs.find({}).create();
        this.tagDiffer = this.differs.find([]).create();
        this.resolutionDiffer = this.differs.find([]).create();
        this.vendorDiffer = this.differs.find([]).create();
        this.hardwareDiffer = this.differs.find([]).create();
    }

    ngOnInit() {
        // Example URI
        // /ipvd?vendors=30X&camera=IPPTZ-ELS2IRL30X-ATI
        this.uri
            .getURI()
            .subscribe(params => {
                this.params = { ...params };
            });

        this.activate();

        this.breakpointObserver
            .observe(['(max-width: 991px)'])
            .subscribe((state: BreakpointState) => {
                this.mobileDetailMode = (state.matches && this.activeCamera);
            });
    }

    addFilterResolutions() {
        this.resolutions = JSON.parse(this.CONFIG.ipvd.supportedResolutions);

        this.filterModel.selects = [
            {
                id      : 'resolution',
                label   : 'Minimum Resolution',
                items   : this.resolutions,
                selected: this.resolutions[0]
            }
        ];
    }

    setActiveCamera() {
        if (this.params.camera && this.cameras.length) {
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
                label   : 'Types',
                singular: 'Type',
                items   : this.hardwareTypes,
                selected: []
            }
            // vendors will be added later
        ];
    }

    ngDoCheck() {
        const filterChanges = this.differ.diff(this.filterModel);
        let vendorChanges;
        let hardwareChanges;
        let resolutionChanges;

        if (this.filterModel.multiselects.find(x => x.id === 'vendors') !== undefined) {
            vendorChanges = this.vendorDiffer.diff(this.filterModel.multiselects.find(x => {
                return x.id === 'vendors';
            }).selected);
        }

        const tagChanges = this.tagDiffer.diff(this.filterModel.tags.map(tag => tag.value));

        if (this.filterModel.selects.find(x => x.id === 'resolution') !== undefined) {
            resolutionChanges = this.resolutionDiffer.diff(this.filterModel.selects.find(x => {
                return x.id === 'resolution';
            }).selected);
        }

        if (this.filterModel.multiselects.find(x => x.id === 'hardwareTypes') !== undefined) {
            hardwareChanges = this.hardwareDiffer.diff(this.filterModel.multiselects.find(x => {
                return x.id === 'hardwareTypes';
            }).selected);
            this.hwtypes = this.filterModel.multiselects.find(x => {
                return x.id === 'hardwareTypes';
            }).selected;
        }

        if (filterChanges || tagChanges || resolutionChanges || vendorChanges || hardwareChanges) {
            this.searchVendor(this.filterModel);
        }
    }

    activate() {
        this.resetFilters();
        this.cameraService
            .getIPVD()
            .subscribe(data => {
                // LANG and CONFIG are available till now
                this.CONFIG = this.configService.getConfig();
                this.company = this.CONFIG.companyName;
                this.lang = this.translate.translations[this.translate.currentLang];
                this.placeholder = this.lang.search.search_ipvd;

                this.data = data;

                this.vendors = data.vendors;
                this.vendors.sort(NxUtilsService.byParam((elm) => {
                    return elm.name.toLowerCase();
                }, NxUtilsService.sortASC));

                // add hardware types and tags
                this.addFilterTags();
                this.addFilterTypes();
                this.addFilterResolutions();

                // reformat vendors to fit the multiselect component
                this.filterModel
                    .multiselects.unshift(
                        {
                            id      : 'vendors',
                            label   : 'Manufacturers',
                            singular: 'Manufacturer',
                            items   : this.vendors.map(v => (
                                    { id: v.name, label: v.name }
                            )),
                            selected: []
                        });

                // Trigger model change for search component
                this.filterModel = {...this.filterModel};
            });
    }

  resetFilters() {
      this.filter = {...this.emptyFilter};
  }

  // restrict the parameters to be passed and viewed for to cam-table (based on allowedParameters)
  preFilterCameraTable (cameras) {
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

    searchVendor(filter) {
        if (!this.params.camera) {
            this.resetActiveCamera();
        }

        this.filter = filter;

        if (this.data) {
            this.cameraSearchService
                .ipvdSearch(this.data.cameras, this.filter)
                .subscribe(cameras => {
                    this.activeCamera = undefined;
                    this.cameras = cameras;
                    // this.camerasSuccessFn(this.cameras);
                    this.camerasTable = [];
                    this.camerasTable = (cameras.length !== this.data.length) ? this.preFilterCameraTable(cameras) : [];

                    this.setActiveCamera();
                });
        }
    }

  setVendor(vendor) {
      this.filterModel.query = vendor;
      this.searchVendor(this.filterModel);
      return false;
  }

  activateCamera(elementSelected: any): void {
      if (!elementSelected) {
          return;
      }
      if (Object.keys(elementSelected).length === 0 || elementSelected.key === -1) {
          // call was not initiated by linking the element in HTML
          this.resetActiveCamera();
          return;
      }

      this.uri.updateURI('/ipvd', [
          {
              key: 'camera', value: elementSelected.model || elementSelected.value.model
          }
      ]);

      const selectedCamera = this.cameras.find((camera) => {
          return camera.sortKey === (elementSelected.sortKey || elementSelected.value.sortKey);
      });

      if (this.activeCamera === selectedCamera) {
          return;
      }

      this.activeCamera = {...selectedCamera};
      this.showAll = false;

      if (this.breakpointObserver.isMatched('(max-width: 991px)')) {
          this.mobileDetailMode = true;
      }

      if (typeof this.activeCamera.firmwares === 'string') {
          const firmwares = JSON.parse(this.activeCamera.firmwares);
          let firmwaresArray = [];

          let maxFirmwareCount = 0,
              totalCameraCount = 0;

          Object.keys(firmwares).forEach(key => {
              const count = firmwares[key];
              firmwaresArray.push({name: key, count});

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
  }

    openFeedback(param) {
        const type = (param === 'device') ? this.CONFIG.messageType.ipvd_device : this.CONFIG.messageType.ipvd_page ;
        const device = (this.activeCamera) ? this.activeCamera.model : '';
        this.messageDialog
            .open(type, device, device)
            .then(() => {});

        return false;
    }

  resetActiveCamera() {
      this.uri.updateURI('/ipvd', [{ key: 'camera', value: undefined }]);
      this.activeCamera = undefined;
      this.toggleCamview = false;
      this.mobileDetailMode = false;
  }
}
