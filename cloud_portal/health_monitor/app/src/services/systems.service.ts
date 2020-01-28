import { Injectable, OnDestroy } from '@angular/core';
import { of, ReplaySubject } from 'rxjs';
import { distinctUntilChanged, map, tap } from 'rxjs/operators';

import { NxConfigService } from './nx-config';
import { NxLanguageProviderService } from './nx-language-provider';
import { NxCloudApiService } from './nx-cloud-api';
import { NxPollService } from './poll.service';
import { NxToastService } from '../dialogs/toast.service';

@Injectable({
    providedIn: 'root'
})
export class NxSystemsService implements OnDestroy {
    CONFIG: any;
    LANG: any;
    activeSubscription: any;
    currentUser: string;
    mergingSystems: any;
    systems: any;
    systemsPoll: any;
    systemsSubject = new ReplaySubject(0);

    constructor(private cloudApi: NxCloudApiService,
                private config: NxConfigService,
                private language: NxLanguageProviderService,
                private pollService: NxPollService,
                private toastService: NxToastService
    ) {
        this.LANG = this.language.getTranslations();
        this.CONFIG = this.config.getConfig();
        this.systemsPoll = pollService.createPoll(this.cloudApi.systems(), this.CONFIG.updateInterval);
        this.mergingSystems = new Set();
    }
    addToMergeList(systemId) {
        this.mergingSystems.add(systemId);
    }

    removeFromMergeList(systemId) {
        if (this.mergingSystems.has(systemId)) {
            this.mergingSystems.delete(systemId);
            const options = {
                    autoHide: true,
                    classname: 'success',
                    delay: this.CONFIG.alertTimeout
                };
            this.toastService.show(this.LANG.system.mergeSuccess, options);
        }
    }

    forceUpdateSystems(userEmail?) {
        if (userEmail) {
            this.currentUser = userEmail;
        }

        return this.cloudApi.systems().pipe(tap((systems) => {
            this.processSystems(systems);
            this.systemsSubject.next(systems);
        }));
    }

    forceUpdateSystemsAsPromise(userEmail?) {
        return this.forceUpdateSystems(userEmail).toPromise();
    }

    getSystemOwnerName (system, currentUserEmail, forOrder?) {
        if (system.ownerAccountEmail === currentUserEmail) {
            if (forOrder) {
                return `!!!!!!!${system.name}`; // Force my systems to be first
            }
            return this.LANG.system.yourSystem;
        }

        if (system.ownerFullName && system.ownerFullName.trim() !== '') {
            return system.ownerFullName;
        }

        return system.ownerAccountEmail;
    }

    getMySystems(currentUserEmail, currentSystemId) {
        return this.systems.filter((system) => {
            return system.ownerAccountEmail === currentUserEmail && system.id !== currentSystemId;
        }).sort((a, b) => {
            return a.name.toLowerCase() < b.name.toLowerCase() ? -1 : 1;
        });
    }

    getSystem(systemId, useCache = true) {
        let system;
        if (this.systems && this.systems.length > 0) {
            system = this.systems.find((system) => {
                return system.id === systemId;
            });
        }

        if (system && useCache) { // Cache success
            return of(system);
        } else { // Cache miss
            return this.cloudApi.systems(systemId).pipe(map((systems) => {
                return systems[0];
            }));
        }
    }

    getSystemAsPromise(systemId, useCache = true) {
        return this.getSystem(systemId, useCache).toPromise();
    }

    getSystems(userEmail) {
        this.currentUser = userEmail;
        if (this.activeSubscription) {
            this.activeSubscription.unsubscribe();
        }
        this.activeSubscription = this.systemsPoll.pipe(
            tap(systems => this.processSystems(systems)),
            distinctUntilChanged((a, b) => JSON.stringify(a) === JSON.stringify(b))
        ).subscribe(() => {
            this.systemsSubject.next(this.systems);
        });
    }

    stopPoll() {
        if (this.activeSubscription) {
            this.activeSubscription.unsubscribe();
        }
    }

    ngOnDestroy(): void {
        if (this.systemsPoll) {
            this.systemsPoll.unsubscribe();
        }
    }

    private processSystems(systems) {
        this.systems = this.sortSystems(systems, this.currentUser);
        this.systems.forEach((system) => {
            system.isMine = system.ownerAccountEmail === this.currentUser;
            system.canMerge = system.isMine && (system.capabilities &&
                system.capabilities.cloudMerge
                || this.CONFIG.allowDebugMode
                || this.CONFIG.allowBetaMode);
            if (system.mergeInfo !== undefined) {
                this.addToMergeList(system.id);
            } else if (this.mergingSystems.has(system.id)) {
                setTimeout(_ => this.removeFromMergeList(system.id), 500);
            }
        });
    }

    private sortSystems(systems, currentUserEmail) {
        // Alphabet sorting
        const preSort = systems.sort((systemA, systemB) => {
            const systemAName = this.getSystemOwnerName(systemA, currentUserEmail, true);
            const systemBName = this.getSystemOwnerName(systemB, currentUserEmail, true);
            return systemAName < systemBName ? -1 : 1;
        });
        // Sort by usage frequency is more important than Alphabet
        return preSort.sort((systemA, systemB) => {
            return -systemA.usageFrequency < -systemB.usageFrequency ? -1 : 1;
        });
    }

}
