import {
    Component, Input, Output, EventEmitter,
    OnChanges, SimpleChanges,
    OnInit, ViewEncapsulation } from '@angular/core';
import { NxPagerService }       from '../../../../services/pager.service';
import { NxConfigService }      from '../../../../services/nx-config';
import { TranslateService }     from '@ngx-translate/core';
import { NxUriService }         from '../../../../services/uri.service';
import { NxUtilsService }       from '../../../../services/utils.service';

@Component({
  selector: 'nx-cam-table',
  templateUrl: './cam-table.component.html',
  styleUrls: ['./cam-table.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class CamTableComponent implements OnChanges, OnInit {

  // @Input() headers: string[];
  @Input() elements: any[];
  @Input() allowedParameters: string[];
  @Input() activeCamera: any;

  @Output() public onRowClick: EventEmitter<any> = new EventEmitter<any>();

  public selectedHeader;
  public showHeaders;

  private _elements: any[];
  private selectedRow;
  private sortOrderASC: boolean;
  private results;
  private cameraHeaders;
  private paramsShown;
  private lang;
  private params: any;
  private debug: boolean;

  pager: any = {};
  pagedItems: any[];
  pagerMaxSize: number;
  CONFIG: any = {};

  // Options for the Excel export
  public csvFilename: any;
  public csvCameraData: any[];
  public csvOptions = {
    fieldSeparator: ',',
    quoteStrings: '"',
    decimalseparator: '.',
    showLabels: true,
    headers: ['Vendor', 'Model', 'Max Resolution', 'Max FPS', 'Codec', '2-Way Audio', 'Advanced PTZ', 'Fisheye', 'Motion', 'I/O'],
    showTitle: true,
    title: 'Camera List',
    useBom: false,
    removeNewLines: true
  };

  constructor(private pagerService: NxPagerService,
              private translate: TranslateService,
              private uri: NxUriService,
              config: NxConfigService) {

      this.lang = this.translate.translations[this.translate.currentLang];
      this.CONFIG = config.getConfig();

      this.sortOrderASC = true;
      this._elements = this.elements;

      this.paramsShown = 6;
      this.cameraHeaders = [
          this.lang.ipvd.vendor,
          this.lang.ipvd.model,
          this.lang.ipvd.hardwareType,
          this.lang.ipvd.maxResolution,
          this.lang.ipvd.maxFps,
          this.lang.ipvd.primaryCodec,
          this.lang.ipvd.isAudioSupported,
          this.lang.ipvd.isPtzSupported,
          this.lang.ipvd.isFisheye,
          this.lang.ipvd.isMdSupported,
          this.lang.ipvd.isIoSupported,
          this.lang.ipvd.count
      ];

      this.pagerMaxSize = this.CONFIG.ipvd.pagerMaxSize;
  }

    toggleHeaderSort(param) {
        let filter;
        for (const [key, value] of Object.entries(this.lang.ipvd)) {
            if (value === param) {
                filter = key;
                break;
            }
        }

        this.sortOrderASC = (this.lang.ipvd[filter] === this.selectedHeader) ? !this.sortOrderASC : true;
        this.toggleSort(filter, false /* reset camera and page params in uri */);
    }

    toggleSort(param, keepURI) {
        let byParam;

        if (param === 'maxResolution') {
            byParam = NxUtilsService.byParam((elm) => {
                // sanitize max resolution field and calc total pixels to sort
                // Found variations:
                // 1920x1080, 1920 x 1080, '2MP, 1080p'
                let pixels = 0;
                let res = elm[param].replace(/[^x\d]+/gi, '');
                if (res === '21080') { // camera reports "2MP, 1080p"
                    pixels = 1920 * 1080; // 1920x1080
                } else {
                    res = res.split('x');
                    pixels = res[0] * res[1];
                }
                return pixels;
            }, !this.sortOrderASC);

        } else if (param === 'maxFps') {
            byParam = NxUtilsService.byParam((elm) => {
                return elm[param];
            }, !this.sortOrderASC);

        } else if (param === 'isFisheye' ||
                param === 'isMdSupported' ||
                param === 'isIoSupported') {

            byParam = NxUtilsService.byParam((elm) => {
                if (elm[param]) {
                    return 0;
                }
                if (elm[param] === false) {
                    return 2;
                }
                if (elm[param] === null) {
                    return 3;
                }
            }, this.sortOrderASC);

        } else if (param === 'isPtzSupported') {
            byParam = NxUtilsService.byParam((elm) => {
                if (elm.isAptzSupported) {
                    return 0;
                }
                if (elm.isPtzSupported) {
                    return 1;
                }
                if (elm.isPtzSupported === false) {
                    return 2;
                }
                if (elm.isPtzSupported === null) {
                    return 3;
                }
            }, this.sortOrderASC);

        } else if (param === 'isAudioSupported') {
            byParam = NxUtilsService.byParam((elm) => {
                if (elm.isAudioSupported === null) {
                    return 3;
                }
                if (elm.isAudioSupported === false) {
                    return 2;
                }
                if (elm.isTwAudioSupported) {
                    return 0;
                }
                if (elm.isAudioSupported) {
                    return 1;
                }
            }, this.sortOrderASC);

        } else {
            byParam = NxUtilsService.byParam((elm) => {
                return (typeof elm[param] === 'string') ? elm[param].toLowerCase() : elm[param];
            }, this.sortOrderASC);
        }

        this._elements.sort(byParam);
        this.setPage(1, keepURI);

        this.selectedHeader = this.cameraHeaders.find(x => {
            return x === this.lang.ipvd[param];
        });
    }

    filterAllowedParams() {
        // filter 'service' params
        const serviceParams = ['count'];
        this.allowedParameters.forEach((item, index) => {
            if (serviceParams.indexOf(item) > -1) {
                this.allowedParameters.splice(index, 1);
            }
        });

        this.cameraHeaders.forEach((item, index) => {
            if (serviceParams.indexOf(item.toLowerCase()) > -1) {
                this.cameraHeaders.splice(index, 1);
            }
        });
    }

    showParametersFor(item) {
         const showParameters = [...this.allowedParameters];
        // adjust PTZ and Audio params
        let idxToBeRemoved;
        let param;

        param = (item.isAptzSupported) ? 'isPtzSupported' : 'isAptzSupported';
        idxToBeRemoved = showParameters.indexOf(param);
        showParameters.splice(idxToBeRemoved, 1);

        param = (item.isTwAudioSupported) ? 'isAudioSupported' : 'isTwAudioSupported';
        idxToBeRemoved = showParameters.indexOf(param);
        showParameters.splice(idxToBeRemoved, 1);

        return showParameters;
    }

    private sortElements(keepURI) {
        // If sort by popularity is set in CMS or default sorting 'Vendor-Model'
        const sortBy = (this.CONFIG.ipvd.sortSupportedDevicesByPopularity) ? 'count' : 'sortKey';
        this.toggleSort(sortBy, keepURI);
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.elements) {
            this.sortOrderASC = !this.CONFIG.ipvd.sortSupportedDevicesByPopularity;
            this._elements = changes.elements.currentValue;
            this.results = this._elements.length;

            this.sortElements(true /* keep uri params */);
            this.csvCameraData = this.getCsvData();
        }

        if (changes.activeCamera) {
            if (!changes.activeCamera.currentValue) {
                this.selectedRow = undefined;
            }
        }
    }

    ngOnInit() {
        this.results = this._elements.length;
        this.csvFilename = Date.now();
        this.csvCameraData = this.getCsvData();

        this.uri
            .getURI()
            .subscribe(params => {
                this.params = { ...params };
                this.debug = true;
                if (params.debug === undefined) {
                    this.debug = false;
                    this.filterAllowedParams();
                }

                this.showHeaders = this.cameraHeaders;

                if (this.params.page) {
                    this.setPage(+this.params.page, true);
                }

                if (this.params.camera) {
                    const row = this.pagedItems.findIndex((camera) => {
                        return camera.model === this.params.camera;
                    });

                    const camera = this.pagedItems.find((camera) => {
                        return camera.model === this.params.camera;
                    });

                    this.setClickedRow(row, { key: row, value: camera });
                }
            });
    }

    setClickedRow(index, element) {
        this.selectedRow = index;
        this.onRowClick.emit(element);
    }

    setPage(page: number, keep?: boolean) {
        // get pager object from service
        this.pager = this.pagerService.getPager(this._elements.length, page, this.CONFIG.layout.tableLarge.rows);

        // get current page of items
        this.pagedItems = this._elements.slice(this.pager.startIndex, this.pager.endIndex + 1);

        if (this.params && +this.params.page > this.pager.pages.length) {
            this.uri.updateURI('/ipvd', [
                {
                    key: 'page', value: this.pager.currentPage
                }
            ]);
        }

        if (!keep) {
            // clear selected camera
            this.setClickedRow(-1, {});
            this.uri.updateURI('/ipvd', [
                {
                    key: 'page', value: this.pager.currentPage
                }, {
                    key  : 'camera', value: undefined
                }]);
        }

        // set look'n'feel for pagination element - don't ellipsize 1 page
        if (this.pager.pages.length - 3 < this.pagerMaxSize) {
            this.pagerMaxSize = this.pager.pages.length;
        }
    }

  getCsvData() {
      return this._elements.map(camera => ({
                    'Vendor': camera.vendor,
                    'Model': camera.model,
                    'Max Resolution': camera.maxResolution,
                    'Max FPS': camera.maxFps,
                    'Codec': camera.primaryCodec,
                    '2-Way Audio': NxUtilsService.yesNo(camera.isTwAudioSupported),
                    'Advanced PTZ': NxUtilsService.yesNo(camera.isAptzSupported),
                    'Fisheye': NxUtilsService.yesNo(camera.isFisheye),
                    'Motion': NxUtilsService.yesNo(camera.isMdSupported),
                    'I/O': NxUtilsService.yesNo(camera.isIoSupported)
               })
        );
  }

    isBoolean(x: any): boolean {
        return !(typeof x === 'string' || typeof x === 'number');
    }
}
