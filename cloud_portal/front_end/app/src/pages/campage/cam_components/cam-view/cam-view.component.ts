import { Component, EventEmitter, Input, OnInit, Output, SimpleChange, SimpleChanges } from '@angular/core';

@Component({
  selector: 'nx-cam-view',
  templateUrl: './cam-view.component.html',
  styleUrls: ['./cam-view.component.scss']
})
export class CamViewComponent implements OnInit {

  @Input() activeCamera: any;
  private _activeCamera: any;
  @Output()
   public onCloseView: EventEmitter<any> = new EventEmitter<any>();
  @Output()
   public onFeedbackClick: EventEmitter<any> = new EventEmitter<any>();

  constructor() { }

  ngOnInit() {
  }

  ngOnChanges(changes: SimpleChanges) {
    const activeCamera: SimpleChange = changes.activeCamera;
    this._activeCamera = activeCamera.currentValue;
  }

  firmwarePercentage (count, total) {
      const percentage = Math.round((count / total) * 100);
      return percentage ? percentage + '%' : '< 1';
  }

  firmwareLength (count, maxFirmware) {
      // Make sure we don't return 0
      var pow = maxFirmware > 200 ? Math.log2(200) / Math.log2(maxFirmware) : 1;
      return Math.round(100 * Math.pow(count / maxFirmware, pow));
  };

}
