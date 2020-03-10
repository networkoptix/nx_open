import { Component, OnDestroy, OnInit } from '@angular/core';
import { NxRibbonService } from './ribbon.service';
import { distinctUntilChanged } from 'rxjs/operators';
import { Subscription } from 'rxjs';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';
import { Utils } from '../../utils/helpers';

@AutoUnsubscribe()
@Component({
    selector: 'nx-ribbon',
    templateUrl: 'ribbon.component.html',
    styleUrls: ['ribbon.component.scss'],
})
export class NxRibbonComponent implements OnInit, OnDestroy {
    message: string;
    action: string;
    actionUrl: string;
    showRibbon: boolean;
    type: string;
    private ribbonSubscription: Subscription;

    private setupDefaults() {
        this.showRibbon = false;
        this.message = '';
        this.action = '';
        this.actionUrl = '';
        this.type = '';
    }

    constructor(private ribbonService: NxRibbonService) {
        this.setupDefaults();
    }

    ngOnDestroy() {}

    ngOnInit() {
        this.ribbonSubscription = this.ribbonService.contextSubject.pipe(
            distinctUntilChanged((contextA, contextB) => Utils.isEqual(contextA, contextB))
        ).subscribe(context => {
            this.showRibbon = context.visibility || false;
            this.message = context.message || '';
            this.action = context.text || '';
            this.actionUrl = context.url || '';
            this.type = context.type || '';
        });
    }
}
