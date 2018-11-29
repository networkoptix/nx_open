import { Component, Input, Output, EventEmitter, OnChanges, SimpleChanges, SimpleChange, OnInit, ViewEncapsulation } from '@angular/core';
import { PagerService } from '../../../../services/pager-service.service';

@Component({
  selector: 'nx-cam-table',
  templateUrl: './cam-table.component.html',
  styleUrls: ['./cam-table.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class CamTableComponent implements OnChanges, OnInit {

  @Input() headers: string[];
  @Input() elements: any[];
  @Input() allowedParameters: string[];
  @Output()
  public onRowClick: EventEmitter<any> = new EventEmitter<any>();
  private _elements: any[];
  private selectedRow;
  private results;
  pager: any = {};
  pagedItems: any[];

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

  constructor(private pagerService: PagerService) {
      this._elements = this.elements;
  }

  ngOnChanges(changes: SimpleChanges) {
    const elements: SimpleChange = changes.elements;
    this._elements = elements.currentValue;
    this.results = this._elements.length;
    this.setPage(1);
  }

  ngOnInit() {
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
      this.pager = this.pagerService.getPager(this._elements.length, page, 20);

      // get current page of items
      this.pagedItems = this._elements.slice(this.pager.startIndex, this.pager.endIndex + 1);
  }

  yesNo (bVal) {
      if (bVal === undefined || bVal === null)
          return 'Unknown';

      return bVal ? 'Yes' : 'No';
  }

  getCsvData() {
      return this._elements.map(c => ({
                    'Vendor': c.vendor,
                    'Model': c.model,
                    'Max Resolution': c.maxResolution,
                    'Max FPS': c.maxFps,
                    'Codec': c.primaryCodec,
                    '2-Way Audio': this.yesNo(c.isTwAudioSupported),
                    'Advanced PTZ': this.yesNo(c.isAptzSupported),
                    'Fisheye': this.yesNo(c.isFisheye),
                    'Motion': this.yesNo(c.isMdSupported),
                    'I/O': this.yesNo(c.isIoSupported)
               })
        );
  }

  getCurrentDate() {
      return Date.now();
  }

}
