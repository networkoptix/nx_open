import { Component, Input, OnInit } from '@angular/core';
import { NxConfigService } from '../../../services/nx-config';

// TODO: need to style component

@Component({
    selector   : 'nx-system-alert-card-component',
    templateUrl: 'card.component.html',
    styleUrls  : ['card.component.scss']
})
export class NxSystemAlertCardComponent implements OnInit {
    @Input() data: any;
    CONFIG: any;

    constructor(private config: NxConfigService) {
        this.CONFIG = this.config.getConfig();
    }

    ngOnInit() {
    }

}
