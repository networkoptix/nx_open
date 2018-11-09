import { Component, OnInit } from '@angular/core';
import { CamerasService } from '../../services/cameras.service';
import { CampageSearchService } from '../../services/campage-search.service';

@Component({
  selector: 'campage-component',
  templateUrl: 'campage.component.html',
  styleUrls: ['campage.component.scss']
})

export class NxCampageComponent implements OnInit {

    data: any;
    test_data: any;
    camerasPromise: any;
    company: any;
    vendorGroups: number;
    vendors: string[];
    showAdvancedOptions: boolean;
    resolution: string;
    itemsPerPage: number;
    query: string;
    allCameras = [];
    cameras: any;
    totalCameras: number;
    totalVendors: number;
    activeCamera: any;
    hardwareTypes: string[];
    resolutions: any;
    filter: any;
    emptyFilter: any;
    boolKeys: string[];
    cameraHeaders: string[];
    testArrayE: string[];

  private setupDefaults() {
      this.cameraHeaders = ['Manufacturer', 'Model', 'Type', 'Max Resolution', 'Max FPS', 'Codec', 'Audio', 'PTZ', 'Fisheye', 'Motion', 'I/O'];
      this.testArrayE = ['e1', 'e2', 'e3'];

      this.data = null;
      this.test_data = null;
      this.camerasPromise = null;
      this.company = null;

      this.vendorGroups = 4;
      this.showAdvancedOptions = false;
      this.resolution = '0';

      this.itemsPerPage = 15;
      this.query = '';

      this.allCameras = [];
      this.cameras = [];
      this.vendors = [];

      this.totalCameras = 0;
      this.totalVendors = 0;

      this.activeCamera = null;

      this.hardwareTypes = [
            'Camera',
            'Multi-Sensor Camera',
            'Encoder',
            'DVR',
            'Other'
      ];

      this.resolutions = [
            {value: '0', title: 'All'},
            {value: '84480', title: '1CIF'},
            {value: '168960', title: '2CIF'},
            {value: '337920', title: 'D1'},
            {value: '307200', title: 'VGA'},
            {value: '786432', title: 'SVGA'},
            {value: '921600', title: '720p'},
            {value: '1310720', title: '1mp'},
            {value: '2073600', title: '1080p'},
            {value: '1920000', title: '2mp'},
            {value: '3145728', title: '3mp'},
            {value: '4915200', title: '5mp'},
            {value: '8000000', title: '8mp'},
            {value: '10039296', title: '10mp'},
            {value: '15824256', title: '16mp'}
        ];
      this.filter = {};
      this.emptyFilter = {
            minResolution: this.resolutions[0],
            hardwareTypes: [],
            vendors: [],
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
      this.boolKeys = Object.keys(this.emptyFilter).filter(key => {
          return key.startsWith('is');
      });
  }

  constructor(private cameraService: CamerasService,
              private camSearch: CampageSearchService) {
      this.setupDefaults();
  }

  ngOnInit() {
      this.activate();
  }

  activate() {
      this.resetFilters();

      // this.camerasPromise = this.cameraService.getAllCameras(this.company);
      this.cameraService.getAllCameras(this.company).subscribe(data => {
          this.data = data;
          this.camerasSuccessFn();
      });

      // this.camerasPromise.then(this.camerasSuccessFn, this.camerasErrorFn);
  }

  resetFilters() {
      this.filter = {...this.emptyFilter};
  }

  camerasSuccessFn() {
       const response = this.data;

       const vendor_model_count = {};
       const vendors = new Set();
       const camerasWithAliases = [];

       response.forEach(c => {
           this.test_data = c;
           c.isH265 = c.primaryCodec === 'H.265';

           if (c.hardwareType === 'Camera' && c.isMultiSensor) {
               c.hardwareType = 'Multi-Sensor Camera';
           }

           const res = c.maxResolution.split('x');
           if (res.length === 2) {
               c.resolutionArea = res[0] * res[1];
           }
           else {
               c.resolutionArea = 0;
           }

           vendors.add(c.vendor);

           if (c.aliases) {
               camerasWithAliases.push(c);
           }

           const vm = c.vendor + c.model;
           const existing = vendor_model_count[vm];
           if (existing != undefined) {
               if (c.count > existing.count) {
                   vendor_model_count[vm] = c;
               }
           } else {
               vendor_model_count[vm] = c;
           }
       });

       this.allCameras = Object.keys(vendor_model_count);

       camerasWithAliases.forEach(c => {
           c.aliases.split(',').forEach(alias => {
               alias = alias.trim();

               const va = c.vendor + alias;
               if (vendor_model_count[va] === undefined) {
                   const aliasedCamera = {...c};
                   aliasedCamera.model = alias;

                   this.allCameras.push(aliasedCamera);
               }
           });
       });

       this.totalCameras = this.allCameras.length;

       this.vendors = Array.from(vendors).sort(function (a, b) {
           return a.toLowerCase().localeCompare(b.toLowerCase());
       });

       this.totalVendors = this.vendors.length;
  }

  camerasErrorFn(data, status, headers, config) {

  }

  setVendor (vendor) {
        this.query = vendor;
        this.camSearch.campageSearch(this.data, this.query, this.filter, this.boolKeys).subscribe(cameras => {
            this.cameras = cameras;
            this.testArrayE = cameras;
        });
  }

  vendorGroup (n) {
      function nstart(m) {
          if (m == 0) {
              return 0;
          }

          return nstart(m - 1) + ncol(m - 1);
      }

      function ncol(m) {
          return m < (cols - (M * cols - total)) ? M : (M - 1);
      }

      const total = this.vendors.length,
          cols = this.vendorGroups,
          M = Math.ceil(total / cols),
          n1 = nstart(n),
          n2 = ncol(n);

      return {start: n1, end: n1 + n2};
  }

}
