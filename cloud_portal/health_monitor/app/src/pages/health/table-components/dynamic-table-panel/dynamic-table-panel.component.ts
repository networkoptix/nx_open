import {
    Component, ElementRef, EventEmitter, Input, OnChanges,
    Output, SimpleChanges, ViewChild
}                                   from '@angular/core';
import { NxConfigService }          from '../../../../services/nx-config';
import { NxHealthService }          from '../../health.service';

@Component({
    selector   : 'nx-dynamic-table-panel-component',
    templateUrl: './dynamic-table-panel.component.html',
    styleUrls  : ['./dynamic-table-panel.component.scss']
})
export class NxDynamicTablePanelComponent implements OnChanges {

    @Input() panelParams: any;
    @Input() activeEntity: any;
    @Output() public onCloseView: EventEmitter<any> = new EventEmitter<any>();
    // @Output() public onFeedbackClick: EventEmitter<any> = new EventEmitter<any>();

    CONFIG: any = {};
    name: string;

    windowSize: any = {};
    windowScroll: any;
    clientHeight: number;
    offsetHeight: number;
    scrollHeight: number;
    viewScrollFixedTop: boolean;
    viewScrollFixedBottom: boolean;
    elementWidth: any;

    @ViewChild('nxPanelView', { static: false }) nxPanelView: ElementRef;

    constructor(private configService: NxConfigService,
                private healthService: NxHealthService,
    ) {
        this.CONFIG = this.configService.getConfig();
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.activeEntity && changes.activeEntity.currentValue) {
            this.name = this.healthService.findEntityName(changes.activeEntity.currentValue);
        }
    }

    closeView() {
        this.activeEntity = undefined;
        this.onCloseView.emit(this.activeEntity);
    }
}
