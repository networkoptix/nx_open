import {
    Component, Input, Output, EventEmitter,
    OnChanges, SimpleChanges,
    OnInit, ViewEncapsulation }   from '@angular/core';
import { NxPagerService }         from '../../../../services/pager.service';
import { NxConfigService }        from '../../../../services/nx-config';
import { TranslateService }       from '@ngx-translate/core';
import { NxUriService }           from '../../../../services/uri.service';

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

  private _elements: any[];
  private selectedRow;
  private selectedHeader;
  private sortOrderASC: boolean;
  private results;
  private cameraHeaders;
  private showHeaders;
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
          this.lang.campage.vendor,
          this.lang.campage.model,
          this.lang.campage.hardwareType,
          this.lang.campage.maxResolution,
          this.lang.campage.maxFps,
          this.lang.campage.primaryCodec,
          this.lang.campage.isAudioSupported,
          this.lang.campage.isPtzSupported,
          this.lang.campage.isFisheye,
          this.lang.campage.isMdSupported,
          this.lang.campage.isIoSupported,
          this.lang.campage.count
      ];

      this.pagerMaxSize = this.CONFIG.campage.pagerMaxSize;
  }

    byKey(a, b) {
        if (+a.key < +b.key) {
            return -1;
        }
        if (+a.key > +b.key) {
            return 1;
        }
        return 0;
    }

    sortBy(fn) {
        return (a, b) => {
            if (fn(a) < fn(b)) {
                return (this.sortOrderASC) ? -1 : 1;
            }
            if (fn(a) > fn(b)) {
                return (this.sortOrderASC) ? 1 : -1;
            }
            return 0;
        };
    }

    sortByResolution(fn) {
        return (a, b) => {
            const x = fn(a).map(Number);
            const y = fn(b).map(Number);

            if (x[0] < y[0] || x[1] < y[1]) {
                return (this.sortOrderASC) ? -1 : 1;
            }
            if (x[0] > y[0] || x[1] > y[1]) {
                return (this.sortOrderASC) ? 1 : -1;
            }
            return 0;

        };
    }

    toggleHeaderSort(param) {
        let filter;
        for (const [key, value] of Object.entries(this.lang.campage)) {
            if (value === param) {
                filter = key;
                break;
            }
        }

        this.sortOrderASC = (this.lang.campage[filter] === this.selectedHeader) ? !this.sortOrderASC : true;
        this.toggleSort(filter);
    }

    toggleSort(param) {
        let byParam;

        if (param === 'maxResolution') {
            byParam = this.sortByResolution((elm) => {
                return elm[param].split('x');
            });
        } else {
            byParam = this.sortBy((elm) => {
                return (typeof elm[param] === 'string') ? elm[param].toLowerCase() : elm[param];
            });
        }
        this._elements.sort(byParam);
        this.setPage(1, true /* keep uri intact */);

        this.selectedHeader = this.cameraHeaders.find(x => {
            return x === this.lang.campage[param];
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

        this.cameraHeaders.splice(-1, 1); // remove 'count'
    }

    showParametersFor(item) {
         const showParameters = [...this.allowedParameters];
        // adjust PTZ and Audio params
        let idxToBeRemoved;
        let param;

        param = (item.value.isAptzSupported) ? 'isPtzSupported' : 'isAptzSupported';
        idxToBeRemoved = showParameters.indexOf(param);
        showParameters.splice(idxToBeRemoved, 1);

        param = (item.value.isTwAudioSupported) ? 'isAudioSupported' : 'isTwAudioSupported';
        idxToBeRemoved = showParameters.indexOf(param);
        showParameters.splice(idxToBeRemoved, 1);

        return showParameters;
    }

    sortElements() {
        // If sort by popularity is set in CMS or default sorting 'Vendor-Model'
        const sortBy = (this.CONFIG.campage.sortSupportedDevices) ? 'count' : 'sortKey';
        this.toggleSort(sortBy);
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.elements) {
            this.sortOrderASC = !this.CONFIG.campage.sortSupportedDevices;
            this._elements = changes.elements.currentValue;
            this.results = this._elements.length;

            this.sortElements();
        }

        if (changes.activeCamera) {
            if (!changes.activeCamera.currentValue) {
                this.selectedRow = undefined;
            }
        }
    }

    ngOnInit() {
        this.uri
            .getURI()
            .subscribe(params => {
                if (params.debug === undefined) {
                    this.debug = false;
                    this.filterAllowedParams();
                }

                this.showHeaders = this.cameraHeaders;
            });

        this.results = this._elements.length;
        this.csvFilename = Date.now();
        this.csvCameraData = this.getCsvData();

        this.uri
            .getURI()
            .subscribe(params => {
                this.params = { ...params };

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
            this.uri.updateURI('/campage', [
                {
                    key: 'page', value: this.pager.currentPage
                }
            ]);
        }

        if (!keep) {
            // clear selected camera
            this.setClickedRow(-1, {});
            this.uri.updateURI('/campage', [
                {
                    key: 'page', value: this.pager.currentPage
                }, {
                    key  : 'camera', value: undefined
                }]);
            // this.uri.updateURI('camera', undefined);
        }

        // set look'n'feel for pagination element - don't ellipsize 1 page
        if (this.pager.pages.length - 3 < this.pagerMaxSize) {
            this.pagerMaxSize = this.pager.pages.length;
        }
    }

  static yesNo (bVal) {
      if (bVal === undefined || bVal === null) {
          return 'Unknown';
      }

      return bVal ? 'Yes' : 'No';
  }

  getCsvData() {
      return this._elements.map(camera => ({
                    'Vendor': camera.vendor,
                    'Model': camera.model,
                    'Max Resolution': camera.maxResolution,
                    'Max FPS': camera.maxFps,
                    'Codec': camera.primaryCodec,
                    '2-Way Audio': CamTableComponent.yesNo(camera.isTwAudioSupported),
                    'Advanced PTZ': CamTableComponent.yesNo(camera.isAptzSupported),
                    'Fisheye': CamTableComponent.yesNo(camera.isFisheye),
                    'Motion': CamTableComponent.yesNo(camera.isMdSupported),
                    'I/O': CamTableComponent.yesNo(camera.isIoSupported)
               })
        );
  }

    isString(x: any): boolean {
        return typeof x === 'string';
    }
}
