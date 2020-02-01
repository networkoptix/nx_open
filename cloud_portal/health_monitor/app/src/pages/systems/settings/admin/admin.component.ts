import {
    Component, OnDestroy, OnInit, Inject,
    ViewChild, ElementRef, ViewContainerRef,
}                                    from '@angular/core';
import { Location }                  from '@angular/common';
import { ActivatedRoute }            from '@angular/router';
import { NxConfigService }           from '../../../../services/nx-config';
import { NxPageService }             from '../../../../services/page.service';
import { NxDialogsService }          from '../../../../dialogs/dialogs.service';
import { NxSettingsService }         from '../settings.service';
import { NxLanguageProviderService } from '../../../../services/nx-language-provider';
import { NxMenuService }             from '../../../../components/menu/menu.service';
import { NxSystemsService }          from '../../../../services/systems.service';
import { NxAccountService }          from '../../../../services/account.service';
import { NxProcessService }          from '../../../../services/process.service';
import { NxSystem }                  from '../../../../services/system.service';
import { Subscription }              from 'rxjs';
import { throttleTime }              from 'rxjs/operators';
import { AutoUnsubscribe }           from 'ngx-auto-unsubscribe';
import { NxApplyService, Watcher }   from '../../../../services/apply.service';

interface Settings {
    disconnectDisabled: boolean;
    mergeDisabled: boolean;
    renameDisabled: boolean;
    showMerge: boolean;
}

@AutoUnsubscribe()
@Component({
    selector   : 'nx-system-admin-component',
    templateUrl: 'admin.component.html',
    styleUrls  : ['admin.component.scss']
})

export class NxSystemAdminComponent implements OnInit, OnDestroy {
    CONFIG: any = {};
    LANG: any = {};
    system: NxSystem;
    systems: any;
    location: any;

    userDisconnectSystem: any;
    deletingSystem: any;
    currentlyMerging: boolean;
    debugMode: boolean;
    betaMode: boolean;
    settings: Settings;
    settingsSubscription: Subscription;
    settingsServiceSubscription: Subscription;
    systemSubscription: Subscription;
    viewContainerRef: ViewContainerRef;
    limitSessionTimeUnits: any;
    selectedTimeUnit: any;
    timeUnitCount: number;
    currentMaxTimeUnit: number;
    previousInputValue: number;

    saveSettings: any;
    resetVideoEncryptionIfDisabled: any;
    setWarningMessageThroughApplyService: any;
    settingsWatchersSet: boolean;
    timeUnitTracker: any;

    settingsWatchers: any = {
        autoDiscoveryEnabled: new Watcher<boolean>(),
        statisticsAllowed: new Watcher<boolean>(),
        cameraSettingsOptimization: new Watcher<boolean>(),
        auditTrailEnabled: new Watcher<boolean>(),
        trafficEncryptionForced: new Watcher<boolean>(),
        videoTrafficEncryptionForced: new Watcher<boolean>(),
        sessionLimitToggle: new Watcher<boolean>(),
        sessionLimitMinutes: new Watcher<number>(),
        sessionLimitUnit: new Watcher<string>(),
    };

    readonly watchersToNotSave: string[] = ['sessionLimitToggle', 'sessionLimitUnit'];
    readonly minutes: string = 'minutes';
    readonly hours: string = 'hours';

    @ViewChild('timeUnitTracker', { static: false }) set el(el: ElementRef) {
        if (el) {
            this.timeUnitTracker = el;
            this.updateTimeUnitInput(this.selectedTimeUnit);
        }
    }

    private setupDefaults() {
        this.CONFIG = this.configService.getConfig();

        this.debugMode = this.CONFIG.allowDebugMode;
        this.betaMode = this.CONFIG.allowBetaMode;
        this.menuService.setSection('admin');
    }

    private updateSettings(forceMergeState?: boolean) {
        const merging = this.system && typeof(this.system.mergeInfo) !== 'undefined' || forceMergeState;
        const available = this.system && (!this.system.isOnline || !this.system.isAvailable);
        this.settings = {
            disconnectDisabled: merging,
            mergeDisabled: (merging || available) && !(this.debugMode || this.betaMode),
            renameDisabled: merging && this.system.mergeInfo && this.system.mergeInfo.role !== 'master',
            showMerge: this.system && this.system.isMine && this.systemsService.systems.length > 1
        };
    }

    constructor(@Inject(ViewContainerRef) viewContainerRef,
                private accountService: NxAccountService,
                private applyService: NxApplyService,
                private processService: NxProcessService,
                private route: ActivatedRoute,
                private configService: NxConfigService,
                private language: NxLanguageProviderService,
                private pageService: NxPageService,
                private dialogs: NxDialogsService,
                private systemsService: NxSystemsService,
                private settingsService: NxSettingsService,
                private menuService: NxMenuService,
                location: Location,
    ) {
        this.viewContainerRef = viewContainerRef;
        this.location = location;
        this.setupDefaults();
    }


    ngOnInit(): void {
        this.LANG = this.language.getTranslations();

        this.currentlyMerging = false;
        this.settings = {
            disconnectDisabled: false,
            mergeDisabled: false,
            renameDisabled: false,
            showMerge: true
        };

        this.limitSessionTimeUnits = [
            { value: this.hours, name: this.LANG.system.settings.sessionLimitDuration.hours, id: 2, max: 600, default: 24 },
            { value: this.minutes, name: this.LANG.system.settings.sessionLimitDuration.minutes, id: 1, max: 600 , default: 60},
        ];

        this.settingsWatchersSet = false;

        this.init();
    }

    initApplyService(): void {
        this.resetVideoEncryptionIfDisabled = () => {
            const encryptTraffic = this.settingsWatchers.trafficEncryptionForced.value;
            const encryptVideo = this.settingsWatchers.videoTrafficEncryptionForced.value;
            if (encryptVideo === true) {
                this.applyService.setWarn('');
            }
            if (!encryptTraffic && encryptVideo) {
                this.settingsWatchers.videoTrafficEncryptionForced.value = false;
            }
        };

        this.setWarningMessageThroughApplyService = () => {
            if (this.settingsWatchers.videoTrafficEncryptionForced.value === true) {
                this.applyService.setWarn(this.LANG.system.settings.warningMessages.videoEncryption);
            } else {
                this.applyService.setWarn('');
            }
        };

        this.saveSettings = this.processService.createProcess(() => {
            // handle toggle for sessionLimit, if saving an empty value
            if (this.timeUnitCount === null) {
                this.settingsWatchers.sessionLimitToggle.originalValue = false;
                this.settingsWatchers.sessionLimitToggle.value = false;
            }
            const changes = {};
            const sw = this.settingsWatchers;
            const settings = Object.keys(sw);
            for (const setting of settings) {
                if (this.watchersToNotSave.includes(setting)) {
                    continue;
                }

                // handles save for sessionLimitMinutes differently; only one that isn't a boolean
                const obj = sw[setting];
                if (setting !== 'sessionLimitMinutes') {
                    if (obj.value !== obj.originalValue) {
                        changes[setting] = obj.value;
                        obj.originalValue = obj.value;
                    }
                } else if (sw.sessionLimitToggle.value === true && sw.sessionLimitMinutes.value) {
                    this.updateTimeUnitWatcher();
                    const minutesMatch = (obj.value === obj.originalValue);
                    const unitMatch = (sw.sessionLimitUnit.value === sw.sessionLimitUnit.originalValue);
                    if (!minutesMatch || !unitMatch) {
                        const hourMultiplier = (sw.sessionLimitUnit.value === this.hours) ? 60 : 1;
                        changes[setting] = this.timeUnitCount * hourMultiplier;
                        obj.originalValue = obj.value;
                        if (!unitMatch) {
                            sw.sessionLimitUnit.originalValue = sw.sessionLimitUnit.value;
                        }

                        sw.sessionLimitToggle.originalValue = true;
                    }
                } else if (obj.originalValue !== 0) {
                    changes[setting] = 0;
                    obj.originalValue = 0;
                    obj.value = 0;
                    sw.sessionLimitToggle.originalValue = false;
                }
            }
            return this.system.updateOrGetSystemSettings(changes).toPromise()
                .then(() => this.applyService.reset());
        });

        this.applyService.initPageWatcher(
            this.viewContainerRef,
            this.saveSettings,
            () => {
                this.applyService.reset();
                if (this.settingsWatchers.sessionLimitMinutes) {
                    this.timeUnitCount = this.settingsWatchers.sessionLimitMinutes.originalValue
                        || this.limitSessionTimeUnits[0].default;
                    this.selectedTimeUnit = this.limitSessionTimeUnits.find((e: any) => {
                        return e.value === this.settingsWatchers.sessionLimitUnit.originalValue;
                    });
                }
            },
            Object.values(this.settingsWatchers));

        this.applyService.setVisible(false);
    }

    init(): void {
        if (this.settingsServiceSubscription) {
            this.settingsServiceSubscription.unsubscribe();
        }
        this.settingsServiceSubscription = this.settingsService
            .systemSubject
            .subscribe((system) => {
                this.system = system;
                if (system) {
                    this.pageService.setPageTitle(this.LANG.pageTitles.systemName.replace('{{systemName}}', this.system.info.name));
                    this.system.updateOrGetSystemSettings().subscribe((res: any) => {
                        this.cleanUpWatchers(res.reply.settings);
                        this.initApplyService();

                        if (this.systemSubscription) {
                            this.systemSubscription.unsubscribe();
                        }
                        this.systemSubscription = system.infoSubject
                            .pipe(throttleTime(this.CONFIG.systemThrottleTime))
                            .subscribe(() => {
                                this.settingsService.footerSubject.next(true);
                                this.updateSettings(this.currentlyMerging);
                                if (!this.applyService.locked && this.system.permissions.isAdmin) {
                                    if (this.settingsSubscription) {
                                        this.settingsSubscription.unsubscribe();
                                    }
                                    this.settingsSubscription = this.system.updateOrGetSystemSettings()
                                        .subscribe((res: any) => {
                                            this.setWatcherValues(res.reply.settings);
                                        });
                                }
                            });
                    });

                    this.deletingSystem = this.processService.createProcess(() => {
                        return this.system.deleteFromCurrentAccount();
                    }, {
                        successMessage: this.LANG.system.successDeleted.replace('{{systemName}}', this.system.info.name),
                        errorPrefix   : this.LANG.errorCodes.cantUnshareWithMeSystemPrefix
                    }).then(() => {
                        this.updateAndGoToSystems();
                    }, (error) => {
                        return error;
                    });
                }
            });
    }

    // removes watcher(s) if setting does not exist
    cleanUpWatchers(settings) {
        Object.keys(this.settingsWatchers).forEach(sw => {
            if (!(sw in settings) && !this.watchersToNotSave.includes(sw)) {
                delete this.settingsWatchers[sw];
            }
        });

        if (!this.settingsWatchers.sessionLimitMinutes) {
            delete this.settingsWatchers.sessionLimitToggle;
            delete this.settingsWatchers.sessionLimitUnit;
        }
    }

    setWatcherValues(settings) {
        this.applyService.setVisible(false);
        this.applyService.hardReset();
        const sw = this.settingsWatchers;
        Object.keys(sw).forEach(setting => {
            if (setting in settings) {
                let curr = settings[setting];
                /**
                 * sets initial values for system & security settings
                 * sessionLimitMinutes is the only one that's a number & not a boolean,
                 * so it needs custom code to handle
                 */
                if (isNaN(curr)) {
                    sw[setting].value = curr === 'true';
                } else {
                    curr = parseInt(curr);
                    sw.sessionLimitToggle.value = Boolean(curr);
                    this.timeUnitCount = curr;
                    if (curr % 60 === 0) {
                        this.timeUnitCount /= 60;
                        sw.sessionLimitUnit.value = this.hours;
                    } else {
                        sw.sessionLimitUnit.value = this.minutes;
                    }

                    sw[setting].value = this.timeUnitCount || 0;
                    this.timeUnitCount = this.timeUnitCount || this.limitSessionTimeUnits[0].default;
                    this.selectedTimeUnit = this.limitSessionTimeUnits.find(e => {
                                                return e.value === sw.sessionLimitUnit.value;
                                            });
                }
            }
        });
        this.settingsWatchersSet = true;
        this.applyService.reset();
        this.applyService.setVisible(true);
    }

    disconnect() {
        if (this.system.isMine) {
            // User is the owner. Deleting system means unbinding it and disconnecting all accounts
            // dialogs.confirm(this.LANG.system.confirmDisconnect, this.LANG.system.confirmDisconnectTitle, this.LANG.system.confirmDisconnectAction, 'danger').
            this.dialogs.disconnect(this.system.id)
                .then((result) => {
                    if (result) {
                        this.updateAndGoToSystems();
                    }
                });
        }
    }

    updateAndGoToSystems() {
        this.userDisconnectSystem = true;
        this.systemsService
            .forceUpdateSystems(this.accountService.getEmail())
            .subscribe(() => {
                setTimeout(() => {
                    window.location.href = '/systems';
                });
            });
    }

    delete() {
        if (!this.system.isMine) {
            // User is not owner. Deleting means he'll lose access to it
            this.dialogs.confirm(this.LANG.system.confirmUnshareFromMe,
                                 this.LANG.system.confirmUnshareFromMeTitle,
                                 this.LANG.system.confirmUnshareFromMeAction,
                                 'btn-danger',
                                 this.LANG.dialogs.cancelButton)
                .then((result) => {
                    if (result === true) {
                        return this.deletingSystem.run();
                    }
                });
        }
    }

    rename() {
        return this.dialogs
                   .rename(this.system.id, this.system.info.name)
                   .then((finalName) => {
                       if (finalName) {
                           this.system.info.name = finalName;
                       }

                       this.pageService.setPageTitle(this.system.info.name + ' -');
                       this.systemsService.forceUpdateSystems(this.accountService.getEmail());
                   });
    }

    mergeSystems() {
        this.systems = this.systemsService.getMySystems(this.accountService.getEmail(), this.system.id);
        this.currentlyMerging = true;
        this.updateSettings(this.currentlyMerging);
        this.settingsService.system = this.system;

        return this.dialogs
                   .merge(this.system, this.systems, this.accountService)
                   .then((mergeInfo) => {
                       if (mergeInfo) {
                           this.system.mergeInfo = mergeInfo;
                           const systemId = mergeInfo.role === 'master' ? this.system.id : mergeInfo.anotherSystemId;
                           this.systemsService.addToMergeList(systemId);
                       }
                   }, (error) => {
                       if (!error.primarySystemName && !error.secondarySystemName) {
                           return;
                       }
                       const commonErrorMsg = this.LANG.dialogs.merge.commonText
                                                  .replace('{{primarySystem}}', error.primarySystemName)
                                                  .replace('{{secondarySystem}}', error.secondarySystemName);
                       let responseError = this.LANG.errorCodes[error.errorText] || this.LANG.errorCodes[error.resultCode];
                       if (!responseError) {
                           responseError = this.LANG.errorCodes.unknownMergeError;
                       } else {
                           responseError = responseError.replace('{{failedSystem}}', error.failedSystemName);
                       }

                       // HTML needed for section formatting
                       const dialogBody = '<p>' + commonErrorMsg + '</p><p>' + responseError + '</p>';

                       // Handling promise to satisfy the linter.
                       this.dialogs.confirm(
                               dialogBody,
                               this.LANG.dialogs.merge.mergeFailedTitle,
                               this.LANG.dialogs.okButton,
                               'btn-primary',
                               undefined).then(() => {});
                   }).finally(() => {
                       this.currentlyMerging = false;
                       this.updateSettings(this.currentlyMerging);
                       this.settingsService.system = this.system;
                   });
    }

    updateUserRole() {
        let userRole = this.system.accessRole;
        if (this.system.accessRole in this.LANG.accessRoles) {
            userRole = this.LANG.accessRoles[this.system.accessRole].label;
        }
        return userRole;
    }

    updateTimeUnitInput(timeUnit) {
        this.currentMaxTimeUnit = timeUnit.max;
        const el = this.timeUnitTracker;
        if (el) {
            if (el.nativeElement.value > this.currentMaxTimeUnit) {
                el.nativeElement.value = this.currentMaxTimeUnit;
            }
            el.nativeElement.setAttribute('max', this.currentMaxTimeUnit);

            if (this.selectedTimeUnit.value !== timeUnit.value) {
                this.settingsWatchers.sessionLimitUnit.value = timeUnit.value;
                this.selectedTimeUnit = timeUnit;
            }
        }
    }

    storePreviousValue(e) {
        // prevents [.+-e] from being inputed
        if (['.', '+', '-', 'e'].indexOf(e.key) > -1) {
            e.preventDefault();
        }
        this.previousInputValue = this.timeUnitCount;
    }

    validationCheckForInput() {
        if (this.timeUnitCount > this.currentMaxTimeUnit) {
            this.timeUnitCount = this.previousInputValue;
        }

        this.updateTimeUnitWatcher();
    }

    updateTimeUnitWatcher() {
        const sw = this.settingsWatchers;
        if (sw.sessionLimitUnit.value === this.minutes && this.timeUnitCount % 60 === 0) {
            sw.sessionLimitUnit.value = this.hours;
            this.selectedTimeUnit = this.limitSessionTimeUnits.find(e => {
                                        return e.value === sw.sessionLimitUnit.value;
                                    });
            this.timeUnitCount /= 60;
        }
        sw.sessionLimitMinutes.value = this.timeUnitCount;
    }

    ngOnDestroy() {}
}
