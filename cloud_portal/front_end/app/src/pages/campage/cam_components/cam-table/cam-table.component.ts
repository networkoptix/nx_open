import {
    Component, Input, Output, EventEmitter,
    OnChanges, SimpleChanges, SimpleChange,
    OnInit, ViewEncapsulation } from '@angular/core';
import { PagerService }         from '../../../../services/pager-service.service';
import { NxConfigService }      from '../../../../services/nx-config';
import { TranslateService }     from '@ngx-translate/core';
import { _ }                    from '@biesbjerg/ngx-translate-extract';

// _('Unknown');
// _('Yes');
// _('No');
// _('Camera List');

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
  private showParameters;
  private showHeaders;
  private paramsShown;
  private lang;
  private sortables;

  pager: any = {};
  pagedItems: any[];
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

  constructor(private pagerService: PagerService,
              private translate: TranslateService,
              config: NxConfigService) {

      this.lang = this.translate.translations[this.translate.currentLang];
      this.CONFIG = config.getConfig();

      this.sortOrderASC = true;
      this._elements = this.elements;

      this.paramsShown = 6;
      this.cameraHeaders = [
          this.lang.vendor,
          this.lang.model,
          this.lang.hardwareType,
          this.lang.maxResolution,
          this.lang.maxFps,
          this.lang.primaryCodecamera,
          this.lang.isAudioSupported,
          this.lang.isPtzSupported,
          this.lang.isFisheye,
          this.lang.isMdSupported,
          this.lang.isIoSupported
      ];

      this.sortables = [
          this.lang.vendor,
          this.lang.model,
          this.lang.hardwareType
      ];
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
            // if (this.sortOrderASC) {
                // return -(fn(a) < fn(b)) || +(fn(a) > fn(b));
                if (fn(a) < fn(b)) {
                    return (this.sortOrderASC) ? -1 : 1;
                }
                if (fn(a) > fn(b)) {
                    return (this.sortOrderASC) ? 1 : -1;
                }
                return 0;
            // }

            // return +(fn(a) < fn(b)) || -(fn(a) > fn(b));
        };
    }

    isSortable(header) {
        return this.sortables.indexOf(header) > -1;
    }

    toggleHeaderSort(param) {
        if (!this.isSortable(param)) {
            return;
        }

        let filter;
        for (const [key, value] of Object.entries(this.lang)) {
            if (value === param) {
                filter = key;
                break;
            }
        }

        this.sortOrderASC = (this.lang[filter] === this.selectedHeader) ? !this.sortOrderASC : true;
        this.toggleSort(filter);
    }

    toggleSort(param) {
        const byParam = this.sortBy((elm) => {
            return elm[param];
        });
        this._elements.sort(byParam);
        this.setPage(1);

        this.selectedHeader = this.cameraHeaders.find(x => {
            return x === this.lang[param];
        });
    }

    toggleHeaderCaption(param, label1, label2) {
        const paramLookup = this.allowedParameters.find(x => x === param);
        const paramLabel = this.cameraHeaders.findIndex(x => x === label1 || x === label2);
        if (paramLookup) {
            this.cameraHeaders[paramLabel] = label2;
        } else {
            this.cameraHeaders[paramLabel] = label1;
        }
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.elements) {
            this._elements = changes.elements.currentValue;
            this.toggleSort('sortKey');

            this.results = this._elements.length;
        }

        if (changes.activeCamera) {
            this.showParameters = (changes.activeCamera.currentValue) ? this.allowedParameters.slice(0, this.paramsShown) : this.allowedParameters;
            this.showHeaders = (changes.activeCamera.currentValue) ? this.cameraHeaders.slice(0, this.paramsShown) : this.cameraHeaders;
        }

        if (changes.allowedParameters) {
            this.toggleHeaderCaption('isTwAudioSupported', this.lang.isAudioSupported, this.lang.isTwAudioSupported);
            this.toggleHeaderCaption('isAptzSupported', this.lang.isPtzSupported, this.lang.isAptzSupported);

            this.showParameters = (this.activeCamera) ? this.allowedParameters.slice(0, this.paramsShown) : this.allowedParameters;
            this.showHeaders = (this.activeCamera) ? this.cameraHeaders.slice(0, this.paramsShown) : this.cameraHeaders;
        }
    }

  ngOnInit() {
      this.showParameters = this.allowedParameters;
      this.results = this._elements.length;
      this.setPage(1);
      this.csvFilename = this.getCurrentDate();
      this.csvCameraData = this.getCsvData();
  }

  setClickedRow (index, element) {
      this.selectedRow = index;
      this.onRowClick.emit(element);
  }

  setPage(page: number) {
      // get pager object from service
      this.pager = this.pagerService.getPager(this._elements.length, page, this.CONFIG.layout.tableLarge.rows);

      // get current page of items
      this.pagedItems = this._elements.slice(this.pager.startIndex, this.pager.endIndex + 1);
  }

  yesNo (bVal) {
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
                    'Codecamera': camera.primaryCodecamera,
                    '2-Way Audio': this.yesNo(camera.isTwAudioSupported),
                    'Advancameraed PTZ': this.yesNo(camera.isAptzSupported),
                    'Fisheye': this.yesNo(camera.isFisheye),
                    'Motion': this.yesNo(camera.isMdSupported),
                    'I/O': this.yesNo(camera.isIoSupported)
               })
        );
  }

  getCurrentDate() {
      return Date.now();
  }

    isString(x: any): boolean {
        return typeof x === 'string';
    }
}
