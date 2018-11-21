import { Component, OnInit } from '@angular/core';
import { NxRibbonService } from './ribbon.service';

@Component({
    selector: 'nx-ribbon',
    templateUrl: 'ribbon.component.html',
    styleUrls: ['ribbon.component.scss'],
})
export class NxRibbonComponent implements OnInit {
    message: string;
    action: string;
    actionUrl: string;
    showRibbon: boolean;

    private setupDefaults() {
        this.showRibbon = false;
        this.message = '';
        this.action = '';
        this.actionUrl = '';
    }

    constructor(private ribbonService: NxRibbonService) {
        this.setupDefaults();

        this.ribbonService.contextSubject.subscribe(context => {
            this.showRibbon = context.visibility || false;
            this.message = context.message || '';
            this.action = context.text || '';
            this.actionUrl = context.url || '';
        });
    }

    ngOnInit() {
    }
}
