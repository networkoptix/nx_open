import { Component, OnInit, DoCheck, KeyValueDiffers, ViewEncapsulation } from '@angular/core';
import { CamerasService } from '../../services/cameras.service';
import { CampageSearchService } from './campage-search.service';
import { NxModalMessageComponent } from '../../dialogs/message/message.component';
import { NxConfigService } from '../../services/nx-config';

@Component({
  selector: 'campage',
  templateUrl: 'campage.component.html',
    styleUrls: ['campage.component.scss'],
    encapsulation: ViewEncapsulation.None
})

// TODO: what is company used for? Its never assigned.
export class NxCampageComponent implements OnInit, DoCheck {
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
      this.config = this.configService.getConfig();
      this.allowedParameters = ['vendor', 'model', 'hardwareType', 'maxResolution', 'maxFps', 'primaryCodec', 'isAudioSupported', 'isPtzSupported', 'isFisheye', 'isMdSupported', 'isIoSupported'];

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

      // format to fit the multiselect component
      this.hardwareTypes = [
          {id: 'Camera', label: 'Camera'},
          {id: 'Multi-Sensor Camera', label: 'Multi-Sensor Camera'},
          {id: 'Encoder', label: 'Encoder'},
          {id: 'DVR', label: 'DVR'},
          {id: 'Other', label: 'Other'}
      ];

      this.resolutions = [
            {value: '0', name: 'All'},
            {value: '84480', name: '1CIF'},
            {value: '168960', name: '2CIF'},
            {value: '337920', name: 'D1'},
            {value: '307200', name: 'VGA'},
            {value: '786432', name: 'SVGA'},
            {value: '921600', name: '720p'},
            {value: '1310720', name: '1mp'},
            {value: '2073600', name: '1080p'},
            {value: '1920000', name: '2mp'},
            {value: '3145728', name: '3mp'},
            {value: '4915200', name: '5mp'},
            {value: '8000000', name: '8mp'},
            {value: '10039296', name: '10mp'},
            {value: '15824256', name: '16mp'}
        ];
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

      this.filterModel.tags = [
          {
              id  : 'isAudioSupported',
              label: 'Audio',
              value: false
          },
          {
              id  : 'isTwAudioSupported',
              label: '2-Way Audio',
              value: false
          },
          {
              id: 'isPtzSupported',
              label: 'PTZ',
              value: false
          },
          {
              id  : 'isAptzSupported',
              label: 'Advanced PTZ',
              value: false
          },
          {
              id  : 'isFisheye',
              label: 'Fisheye',
              value: false
          },
          {
              id  : 'isMdSupported',
              label: 'Motion',
              value: false
          },
          {
              id  : 'isIoSupported',
              label: 'I/O',
              value: false
          },
          {
              id  : 'isH265',
              label: 'H.265',
              value: false
          },
          {
              id  : 'isMultiSensor',
              label: 'Multi Sensor',
              value: false
          }
      ];
      this.filterModel.selects = [
          {
              id      : 'resolution',
              label   : 'Minimum Resolution',
              items   : this.resolutions,
              selected: this.resolutions[0]
          }
      ];
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

    constructor(private configService: NxConfigService,
                private cameraService: CamerasService,
                private camSearch: CampageSearchService,
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

        if (this.filterModel.multiselects.find(x => x.id === 'vendors') !== undefined) {
            vendorChanges = this.vendorDiffer.diff(this.filterModel.multiselects.find(x => x.id === 'vendors').selected);
        }

        const tagChanges = this.tagDiffer.diff(this.filterModel.tags.map(tag => tag.value));
        if (tagChanges) {
            this.toggleAllowedParameters('isAptzSupported', 'isPtzSupported', 'isAptzSupported');
            this.toggleAllowedParameters('isTwAudioSupported', 'isAudioSupported', 'isTwAudioSupported');

            // make sure cam-table gets the new params
            this.allowedParameters = [...this.allowedParameters];
        }

        const resolutionChanges = this.resolutionDiffer.diff(this.filterModel.selects.find(x => x.id === 'resolution').selected);
        const hardwareChanges = this.hardwareDiffer.diff(this.filterModel.multiselects.find(x => x.id === 'hardwareTypes').selected);

        if (filterChanges || tagChanges || resolutionChanges || vendorChanges || hardwareChanges) {
            this.searchVendor(this.filterModel);
        }
    }

    activate() {
        this.resetFilters();
        this.cameraService
            .getAllCameras(this.company)
            .subscribe(data => {
                this.data = data;
                this.camerasSuccessFn(this.data);
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

           const vm = camera.vendor + camera.model;
           const existing = vendorModelCount[vm];
           if (existing !== undefined) {
               if (camera.count > existing.count) {
                   vendorModelCount[vm] = camera;
               }
           } else {
               vendorModelCount[vm] = camera;
           }
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

      // reformat to fit the multiselect component
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
  }

  // restrict the parameters to be passed and viewed for to cam-table (based on allowedParameters)
  preFilterCameraTable (cameras) {
    const values = Object.keys(cameras).map(key => cameras[key]);
    const filteredCameras = [];
    values.forEach(camera => {
        const filteredCamera = Object.keys(camera)
          .filter(key => this.allowedParameters.indexOf(key) > -1)
          .reduce((obj, key) => {
            obj[key] = camera[key];
            return obj;
          }, {});
        filteredCameras.push(filteredCamera);
    });
    return filteredCameras;
  }

  searchVendor (filter) {
      this.resetActiveCamera();
      this.filter = filter;

      if (this.data) {
          this.camSearch.campageSearch(this.data, this.filter).subscribe(cameras => {
              this.activeCamera = undefined;
              this.cameras = cameras;
              this.camerasSuccessFn(this.cameras);
              this.camerasTable = this.preFilterCameraTable(cameras);
          });
      }
  }

  setVendor(vendor) {
      this.filterModel.query = vendor;
      this.searchVendor(this.filterModel);
      return false;
  }

  activateCamera(elementSelected: any): void {
      if (this.activeCamera === this.cameras[elementSelected.key]) {
          return;
      }
      this.activeCamera = this.cameras[elementSelected.key];
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
