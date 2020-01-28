import { Component, EventEmitter, OnDestroy, OnInit, Output, ViewEncapsulation } from '@angular/core';
import { Subscription, timer } from 'rxjs';
import { NxUtilsService } from '../../../services/utils.service';
import { NxConfigService } from '../../../services/nx-config';
import { NxHealthService } from '../health.service';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';

@AutoUnsubscribe()
@Component({
    selector: 'nx-health-update',
    templateUrl: './update-info.component.html',
    styleUrls: ['update-info.component.scss'],
})
export class NxUpdateInfoComponent implements OnInit, OnDestroy {
    @Output() updateHealth = new EventEmitter();
    CONFIG: any;

    lastUpdate: string;
    timerSubscription: Subscription;

    constructor(private config: NxConfigService,
                private healthService: NxHealthService) {
        this.CONFIG = this.config.getConfig();
    }

    ngOnDestroy() {}

    ngOnInit() {
        this.initUpdateTime();
        this.updateHealth.subscribe(() => {
            this.initUpdateTime();
        });
    }

    initUpdateTime() {
        if (this.timerSubscription) {
            this.timerSubscription.unsubscribe();
        }

        this.lastUpdate = '0 min ago';

        const minute = 60 * 1000;
        this.timerSubscription = timer(0, minute).subscribe((minutes) => {
            if (minutes) {
                const time = this.healthService.secondsToTime(minutes * 60, 'updateTime');
                this.lastUpdate = `${time.replace(/m/, ' min')} ago`;
            }
        });
    }
}
