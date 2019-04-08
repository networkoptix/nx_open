import { Component, EventEmitter, Input, OnInit, Output, SimpleChange, SimpleChanges } from '@angular/core';
import { NxConfigService }                                                             from '../../../../services/nx-config';

@Component({
    selector   : 'nx-cam-view',
    templateUrl: './cam-view.component.html',
    styleUrls  : ['./cam-view.component.scss']
})
export class CamViewComponent implements OnInit {

    @Input() activeCamera: any;
    // private _activeCamera: any;
    @Output() public onCloseView: EventEmitter<any> = new EventEmitter<any>();
    @Output() public onFeedbackClick: EventEmitter<any> = new EventEmitter<any>();

    CONFIG: any = {};
    firmwares: any = [];
    firmwaresToShow: number;
    showAll: boolean;

    constructor(private configService: NxConfigService) {
        this.CONFIG = this.configService.getConfig();
    }

    ngOnInit() {
        this.firmwareCleanUp();
        this.firmwaresToShow = this.CONFIG.ipvd.firmwaresToShow;
        this.showAll = false;
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.activeCamera) {
            this.firmwareCleanUp();
            this.firmwaresToShow = this.CONFIG.ipvd.firmwaresToShow;
            this.showAll = false;
        }
    }

    sendFeedback() {
        this.onFeedbackClick.emit(this.activeCamera);
        return false;
    }

    closeView() {
        this.activeCamera = undefined;
        this.onCloseView.emit(this.activeCamera);
    }

    firmwareCleanUp() {
        this.firmwares = this.activeCamera.firmwares.filter((fw) => !fw.name.match(/[<>]+/g));
    }

    firmwarePercentage(count, total) {
        const percentage = Math.round((count / total) * 100);
        return percentage ? percentage + '%' : '< 1';
    }

    firmwareLength(count, maxFirmware) {
        // Make sure we don't return 0
        const pow = maxFirmware > 200 ? Math.log2(200) / Math.log2(maxFirmware) : 1;
        return Math.round(100 * Math.pow(count / maxFirmware, pow));
    }
}
