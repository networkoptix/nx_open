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
  private results;
  private cameraHeaders;
  private showParameters;
  private showHeaders;
  private paramsShown;
  private lang;


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
      this._elements = this.elements;

      this.paramsShown = 6;
      this.cameraHeaders = [
          this.lang.manufacturer,
          this.lang.model,
          this.lang.type,
          this.lang.max_resolution,
          this.lang.max_fps,
          this.lang.codec,
          this.lang.audio,
          this.lang.ptz,
          this.lang.fisheye,
          this.lang.motion,
          this.lang.io
      ];
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
            this.results = this._elements.length;
            this.setPage(1);
        }

        if (changes.activeCamera) {
            this.showParameters = (changes.activeCamera.currentValue) ? this.allowedParameters.slice(0, this.paramsShown) : this.allowedParameters;
            this.showHeaders = (changes.activeCamera.currentValue) ? this.cameraHeaders.slice(0, this.paramsShown) : this.cameraHeaders;
        }

        if (changes.allowedParameters) {
            this.toggleHeaderCaption('isTwAudioSupported', this.lang.audio, this.lang.tw_audio);
            this.toggleHeaderCaption('isAptzSupported', this.lang.ptz, this.lang.aptz);

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
