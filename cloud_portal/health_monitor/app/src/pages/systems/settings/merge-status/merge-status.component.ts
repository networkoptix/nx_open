import { Component, OnDestroy, OnInit } from '@angular/core';
import { NxSettingsService } from '../settings.service';
import { NxConfigService } from '../../../../services/nx-config';
import { NxLanguageProviderService } from '../../../../services/nx-language-provider';
import { NxSystemsService } from '../../../../services/systems.service';
import { Subscription } from 'rxjs';
import { AutoUnsubscribe } from 'ngx-auto-unsubscribe';
import { filter } from 'rxjs/operators';

@AutoUnsubscribe()
@Component({
    selector   : 'nx-system-merge',
    templateUrl: 'merge-status.component.html',
    styleUrls  : ['merge-status.component.scss']
})

export class NxSystemMergeStatusComponent implements OnInit, OnDestroy {
    config: any;
    currentlyMerging: boolean;
    isMaster: boolean;
    LANG: any;
    mergeTargetSystem: any;
    system: any;

    private infoSubscription: Subscription;
    private systemSubscription: Subscription;

    constructor(private _config: NxConfigService,
                private language: NxLanguageProviderService,
                private settingsService: NxSettingsService,
                private systemsService: NxSystemsService) {
        this.config = this._config.getConfig();
    }

    ngOnDestroy() {}

    ngOnInit(): void {
        this.LANG = this.language.getTranslations();
        this.systemSubscription = this.settingsService.systemSubject
            .pipe(filter(system => system !== undefined))
            .subscribe((system) => {
                this.system = system;
                this.infoSubscription = this.system.infoSubject.subscribe(_ => {
                   if (this.system.mergeInfo) {
                       this.setMergeStatus(this.system.mergeInfo);
                   } else {
                       this.currentlyMerging = false;
                   }
                });
            });
    }

    getMergeTarget(targetSystemId) {
        return this.systemsService.systems.find((system) => targetSystemId === system.id);
    }

    setMergeStatus(mergeInfo) {
        if (!mergeInfo || Object.keys(mergeInfo).length === 0) {
            return;
        }
        this.currentlyMerging = true;
        this.isMaster = mergeInfo.role ? mergeInfo.role !== this.config.systemStatuses.slave : mergeInfo.masterSystemId === this.system.id;
        this.mergeTargetSystem = this.getMergeTarget(mergeInfo.anotherSystemId) || this.LANG.system.unknownName;
    }
}
