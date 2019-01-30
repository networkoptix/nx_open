import { Component, OnInit, DoCheck, KeyValueDiffers, ViewEncapsulation } from '@angular/core';
import { CamerasService }                                                 from '../../services/cameras.service';
import { CampageSearchService }                                           from './campage-search.service';
import { NxModalMessageComponent }                                        from '../../dialogs/message/message.component';
import { NxConfigService }                                                from '../../services/nx-config';
import { TranslateService }                                               from '@ngx-translate/core';

@Component({
  selector: 'campage',
  templateUrl: 'campage.component.html',
    styleUrls: ['campage.component.scss'],
    encapsulation: ViewEncapsulation.None
})

// TODO: what is company used for? Its never assigned.
export class NxCampageComponent implements OnInit, DoCheck {
    lang: any;
    config: any;
    data: any;
    company: any;
    vendors: string[];
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

  private setupDefaults() {
      this.allowedParameters = ['count', 'vendor', 'model', 'hardwareType', 'maxResolution', 'maxFps', 'primaryCodec', 'isAudioSupported', 'isPtzSupported', 'isFisheye', 'isMdSupported', 'isIoSupported'];

      this.data = undefined;
      this.company = undefined;

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
                private cameraSearchService: CampageSearchService,
                // TODO: Use dialog service when it is not being downgraded
                private messageDialog: NxModalMessageComponent,
                private differs: KeyValueDiffers) {

        this.setupDefaults();
        this.differ = this.differs.find({}).create();
        this.tagDiffer = this.differs.find([]).create();
        this.resolutionDiffer = this.differs.find([]).create();
        this.vendorDiffer = this.differs.find([]).create();
        this.hardwareDiffer = this.differs.find([]).create();

    }

    ngOnInit() {
        this.activate();
    }

    addFilterResolutions() {
        this.resolutions = JSON.parse(this.config.supportedResolutions.replace(/[]/g, '').split(','));

        this.filterModel.selects = [
            {
                id      : 'resolution',
                label   : 'Minimum Resolution',
                items   : this.resolutions,
                selected: this.resolutions[0]
            }
        ];
    }

    addFilterTags() {
        this.filterModel.tags = [
            {
                id   : 'isAudioSupported',
                label: this.lang.isAudioSupported,
                value: false
            },
            {
                id   : 'isTwAudioSupported',
                label: this.lang.isTwAudioSupported,
                value: false
            },
            {
                id   : 'isPtzSupported',
                label: this.lang.isPtzSupported,
                value: false
            },
            {
                id   : 'isAptzSupported',
                label: this.lang.isAptzSupported,
                value: false
            },
            {
                id   : 'isFisheye',
                label: this.lang.isFisheye,
                value: false
            },
            {
                id   : 'isMdSupported',
                label: this.lang.isMdSupported,
                value: false
            },
            {
                id   : 'isIoSupported',
                label: this.lang.isIoSupported,
                value: false
            },
            {
                id   : 'isH265',
                label: this.lang.isH265,
                value: false
            },
            {
                id   : 'isMultiSensor',
                label: this.lang.isMultiSensor,
                value: false
            }
        ];
    }

    addFilterTypes() {
        this.hardwareTypes = JSON.parse(this.config.supportedHardwareTypes.replace(/[]/g, '').split(','));
        this.hardwareTypes.forEach(type => {
            type.label = this.lang[type.label];
        });

        this.filterModel.multiselects = [
            {
                id      : 'hardwareTypes',
                label   : 'Types',
                items   : this.hardwareTypes,
                selected: []
            }
            // vendors will be added later
        ];
    }

    toggleAllowedParameters(param, label1, label2) {
        const paramLookup = this.filterModel.tags.find(x => x.id === param);
        const paramLabel = this.allowedParameters.findIndex(x => x === label1 || x === label2);
        if (paramLookup.value) {
            this.allowedParameters[paramLabel] = label2;
        } else {
            this.allowedParameters[paramLabel] = label1;
        }
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
        if (tagChanges) {
            this.toggleAllowedParameters('isAptzSupported', 'isPtzSupported', 'isAptzSupported');
            this.toggleAllowedParameters('isTwAudioSupported', 'isAudioSupported', 'isTwAudioSupported');

            // make sure cam-table gets the new params
            this.allowedParameters = [...this.allowedParameters];
        }

        if (this.filterModel.multiselects.find(x => x.id === 'resolution') !== undefined) {
            resolutionChanges = this.resolutionDiffer.diff(this.filterModel.selects.find(x => {
                return x.id === 'resolution';
            }).selected);
        }

        if (this.filterModel.multiselects.find(x => x.id === 'hardwareTypes') !== undefined) {
            hardwareChanges = this.hardwareDiffer.diff(this.filterModel.multiselects.find(x => {
                return x.id === 'hardwareTypes';
            }).selected);
        }

        if (filterChanges || tagChanges || resolutionChanges || vendorChanges || hardwareChanges) {
            this.searchVendor(this.filterModel);
        }
    }

    activate() {
        this.resetFilters();
        this.cameraService
            .getAllCameras(this.company)
            .subscribe(data => {
                // LANG and CONFIG are not available till now
                this.lang = this.translate.translations[this.translate.currentLang];
                this.config = this.configService.getConfig();

                this.data = data;
                this.camerasSuccessFn(this.data);

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
                            items   : this.vendors.map(v => (
                                    { id: v, label: v }
                            )),
                            selected: []
                        });
            });
    }

  resetFilters() {
      this.filter = {...this.emptyFilter};
  }

  camerasSuccessFn(data) {
       const vendorModelCount = {};
       const vendors = new Set();
       const camerasWithAliases = [];

       data.forEach(camera => {
           camera.isH265 = camera.primaryCodec === 'H.265';

           if (camera.hardwareType === 'Camera' && camera.isMultiSensor) {
               camera.hardwareType = 'Multi-Sensor Camera';
           }

           const res = camera.maxResolution.split('x');
           if (res.length === 2) {
               camera.resolutionArea = res[0] * res[1];
           } else {
               camera.resolutionArea = 0;
           }

           vendors.add(camera.vendor);

           if (camera.aliases) {
               camerasWithAliases.push(camera);
           }

           const vm = camera.vendor.replace(/\s/g, '') + camera.model;
           const existing = vendorModelCount[vm];
           if (existing !== undefined) {
               if (camera.count > existing.count) {
                   vendorModelCount[vm] = camera;
               }
           } else {
               vendorModelCount[vm] = camera;
           }

           camera.sortKey = vm;
       });

       this.allCameras = Object.keys(vendorModelCount);

       camerasWithAliases.forEach(camera => {
           camera.aliases.split(',').forEach(alias => {
               alias = alias.trim();

               const va = camera.vendor + alias;
               if (vendorModelCount[va] === undefined) {
                   const aliasedCamera = {...camera};
                   aliasedCamera.model = alias;

                   this.allCameras.push(aliasedCamera);
               }
           });
       });

       this.vendors = Array.from(vendors).sort((a, b) => {
           return a.toLowerCase().localeCompare(b.toLowerCase());
       });
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
        this.resetActiveCamera();
        // debugger;
        this.filter = filter;

        if (this.data) {
            this.cameraSearchService
                .campageSearch(this.data, this.filter)
                .subscribe(cameras => {
                    this.activeCamera = undefined;
                    this.cameras = cameras;
                    this.camerasSuccessFn(this.cameras);
                    this.camerasTable = [];

                    this.camerasTable = (cameras.length !== this.data.length) ? this.preFilterCameraTable(cameras) : [];
                });
        }
    }

  setVendor(vendor) {
      this.filterModel.query = vendor;
      this.searchVendor(this.filterModel);
      return false;
  }

  activateCamera(elementSelected: any): void {
      const selectedCamera = this.cameras.find((camera) => camera.sortKey === elementSelected.value.sortKey);
      if (this.activeCamera === selectedCamera) {
          return;
      }
      this.activeCamera = selectedCamera;
      this.showAll = false;

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

  open(activeCamera) {
      this.messageDialog.open(this.config.messageType.ipvd, activeCamera.model, activeCamera.model).then(() => {});
      return false;
  }

  resetActiveCamera() {
      this.activeCamera = undefined;
      this.toggleCamview = false;
  }
}
