import {
    AfterViewInit,
    Component, ElementRef, EventEmitter, Input,
    Output, ViewChild
}                                   from '@angular/core';
import { NxConfigService }          from '../../../../services/nx-config';
import { NxHealthService }          from '../../health.service';
import { NxScrollMechanicsService } from '../../../../services/scroll-mechanics.service';
import { NxHealthLayoutService } from '../../health-layout.service';

@Component({
    selector   : 'nx-dynamic-table-panel-component',
    templateUrl: './dynamic-table-panel.component.html',
    styleUrls  : ['./dynamic-table-panel.component.scss']
})
export class NxDynamicTablePanelComponent implements AfterViewInit {

    @Input() panelParams: any;
    @Output() public onCloseView: EventEmitter<any> = new EventEmitter<any>();

    CONFIG: any = {};
    name: string;

    windowSize: any = {};
    clientHeight: number;
    offsetHeight: number;
    scrollHeight: number;

    @ViewChild('nxPanelView', { static: false }) nxPanelView: ElementRef;

    constructor(private configService: NxConfigService,
                private healthService: NxHealthService,
                private scrollMechanicsService: NxScrollMechanicsService,
                private healthLayoutService: NxHealthLayoutService
    ) {
        this.CONFIG = this.configService.getConfig();
        this.healthLayoutService.activeEntitySubject.subscribe((activeEntity: any) => {
            this.name = activeEntity ? this.healthService.findEntityName(activeEntity) : '';
        });
    }

    ngAfterViewInit() {
        this.scrollMechanicsService.panelVisible(true);
    }

    closeView() {
        this.healthLayoutService.activeEntity = undefined;
        this.onCloseView.emit(undefined);
    }
}
