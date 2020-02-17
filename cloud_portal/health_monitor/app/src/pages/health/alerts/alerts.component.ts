import {
    AfterViewInit, Component, ElementRef,
    OnDestroy, OnInit, ViewChild,
    ViewEncapsulation
}                                            from '@angular/core';
import { ActivatedRoute }                    from '@angular/router';
import { Location }                          from '@angular/common';
import { NxConfigService }                   from '../../../services/nx-config';
import { NxMenuService }                     from '../../../components/menu/menu.service';
import { NxHealthService }                   from '../health.service';
import { BehaviorSubject, SubscriptionLike } from 'rxjs';
import { NxUriService }                      from '../../../services/uri.service';
import { AutoUnsubscribe }                   from 'ngx-auto-unsubscribe';
import { NxScrollMechanicsService }          from '../../../services/scroll-mechanics.service';
import { NxUtilsService }                    from '../../../services/utils.service';
import { delay, throttleTime }                      from 'rxjs/operators';
import { NxHealthLayoutService } from '../health-layout.service';

interface Params {
    [key: string]: any;
}

@AutoUnsubscribe()
@Component({
    selector   : 'nx-system-alerts-component',
    templateUrl: 'alerts.component.html',
    styleUrls  : ['alerts.component.scss'],
    encapsulation: ViewEncapsulation.None,
})
export class NxSystemAlertsComponent implements OnInit, AfterViewInit, OnDestroy {

    CONFIG: any;

    filterModel: any;
    params: any = {};
    numFilters: number;
    metricId: any;

    queryParamSubscription: SubscriptionLike;
    breakpointSubscription: SubscriptionLike;
    layoutReadySubscription: SubscriptionLike;
    locationSubscription: SubscriptionLike;
    selectedSubscription: SubscriptionLike;

    layoutReady: boolean;
    fixedLayoutClass: string;
    smallDesktopMode: boolean;
    breakpoint: string;

    manifest: any;
    values: any;

    tableHeaders: any;
    alerts: any;

    activePanelParams: any;

    alertsCount: number;
    alertCards: any;
    alertCardCount: number;

    windowSizeSubscription: any;
    tableWrapper: number;

    @ViewChild('tiles', { static: false }) tilesElement: ElementRef;
    @ViewChild('search', { static: false }) searchElement: ElementRef;
    @ViewChild('area', { static: false }) areaElement: ElementRef;
    // @ViewChild('tableContainer', { static: false }) tableContainer: ElementRef;

    constructor(private route: ActivatedRoute,
                private location: Location,
                private menuService: NxMenuService,
                private configService: NxConfigService,
                private healthService: NxHealthService,
                private uriService: NxUriService,
                private scrollMechanicsService: NxScrollMechanicsService,
                private healthLayoutService: NxHealthLayoutService
    ) {
        this.CONFIG = this.configService.getConfig();
        this.filterModel = {
            selects : [],
            query: ''
        };
    }

    ngOnInit(): void {
        this.numFilters = 4;

        this.params = this.route.snapshot.queryParams;
        this.menuService.setSection('alerts');

        if (!this.healthService.alertsValues) {
            return;
        }

        this.addFilterServers();
        this.addFilterTypes();
        this.addFilterAlarms();

        this.manifest = this.healthService.manifest;
        this.values = this.healthService.values;

        this.initializeHeader();
        this.processAlerts();

        this.alerts = this.healthService.alertsSearch(this.healthService.alertsValues, this.filterModel);
        this.countAlerts();

        if (this.params.id && this.params.metric) {
            const alarm = this.alerts.find((alert) => {
                return alert.entity === this.params.id && alert.metric === this.params.metric;
            });
            this.setActiveEntity(alarm, false);
        }

        this.layoutReadySubscription = this.healthLayoutService.layoutReadySubject.subscribe(() => {
            setTimeout(() => {
                if (this.healthLayoutService.tableElement) {
                    this.tableWrapper = this.healthLayoutService.tableElement
                        .nativeElement.querySelectorAll('.table-wrapper')[0].offsetWidth;
                }
            });
        });

        this.windowSizeSubscription = this.scrollMechanicsService.windowSizeSubject.subscribe(({ width }) => {
            if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                this.healthLayoutService.mobileDetailMode = (this.healthLayoutService.activeEntity !== undefined);
            } else {
                this.healthLayoutService.mobileDetailMode = false;
            }

            this.smallDesktopMode = this.scrollMechanicsService.mediaQueryMin(NxScrollMechanicsService.MEDIA.lg) && this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.xl);

            this.setLayout();
        });

        this.locationSubscription = this.location.subscribe((event: PopStateEvent) => {
            // force view component update without URI update
            setTimeout(() => {
                const params = {...this.route.snapshot.queryParams};

                if (params.id) {
                    const alarm = this.healthService.alertsValues.find((alert) => {
                        return (alert.metric === params.metric && alert.entity === params.id);
                    });
                    this.setActiveEntity(alarm, false);
                } else {
                    this.resetActiveEntity(false);
                }
            });
        });

        this.selectedSubscription = this.menuService
            .selectedSectionSubject
            .pipe(throttleTime(1000))
            .subscribe(selection => {
            // when user click same section in the menu - we need to reset table and entity
            if (this.metricId === selection) {
                this.resetActiveEntity();
                this.resetFilterModel();
                this.alerts = this.healthService.alertsSearch(this.healthService.alertsValues, this.filterModel);
            } else {
                // short circuit first subscription
                this.metricId = 'alerts';
            }
        });
    }

    ngAfterViewInit() {
        this.healthLayoutService.dimensions = [];
        this.healthLayoutService.tilesElement = this.tilesElement;
        this.healthLayoutService.searchElement = this.searchElement;
        this.healthLayoutService.searchTableArea = this.areaElement;

        this.healthLayoutService.fixedLayoutClassSubject.pipe(delay(0)).subscribe((className) => {
            this.fixedLayoutClass = className;
        });

        this.healthLayoutService.layoutReadySubject.pipe(delay(0)).subscribe((value: boolean) => {
            this.layoutReady = value;
        });

        this.healthLayoutService.activeEntitySubject.pipe(delay(0)).subscribe(() => {
            this.setLayout();
        });
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.entity;
    }

    ngOnDestroy() {}

    modelChanged(model) {
        if (JSON.stringify(this.filterModel) !== JSON.stringify(model)) { // avoid unnecessary trips
            this.filterModel = NxUtilsService.deepCopy(model);
            this.alerts = this.healthService.alertsSearch(this.healthService.alertsValues, model);
            this.countAlerts();
        }
    }

    resetFilterModel() {
        if (this.filterModel.selects) {
            this.filterModel.selects.forEach((filter) => {
                filter.selected = filter.items[0];
            });
        }

        this.filterModel = { ...this.filterModel };
        this.countAlerts();
    }

    addFilterAlarms() {
        let selected;
        const alertItems = [
            { value: '0', name: 'All Alerts' },
            { value: 'warning', name: 'Only Warnings' },
            { value: 'error', name: 'Only Errors' }
        ];

        selected = alertItems.filter((item) => {
            return this.params.alertType === item.value;
        })[0];

        this.filterModel.selects.push(
                {
                    id      : 'alertType',
                    label   : '',
                    css     : 'col-12 col-lg-3 mr-0 mr-lg-2 p-0',
                    items   : alertItems,
                    selected: selected || alertItems[0]
                });
    }

    addFilterTypes() {
        const typesItems = [];
        let selected;

        for (const [key, value] of Object.entries(this.healthService.manifest)) {
            const val: any = value;
            if (val.resource !== '') {
                const item = { value: val.resource, name: val.resource };
                typesItems.push(item);

                if (this.params.deviceType === val.resource) {
                    selected = item;
                }
            }
        }

        typesItems.unshift({ value: '0', name: 'All Device Types'});

        this.filterModel.selects.push(
                {
                    id      : 'deviceType',
                    label   : '',
                    css     : 'col-12 col-lg-3 mr-0 mr-lg-2 p-0',
                    items   : typesItems,
                    selected: selected || typesItems[0]
                });
    }

    addFilterServers() {
        const serverItems = [];
        let selected;

        for (const [key, value] of Object.entries(this.healthService.values.servers)) {
            const val: any = value;
            const item = { value: key, name: val._.name.text };
            serverItems.push(item);

            if (this.params.server === key) {
                selected = item;
            }
        }

        serverItems.unshift({ value: '0', name: 'All Servers' });

        this.filterModel.selects.push(
                {
                    id      : 'server',
                    label   : '',
                    css     : 'col-12 col-lg-4 mr-0 mr-lg-2 p-0',
                    items   : serverItems,
                    selected: selected || serverItems[0]
                });
    }

    isFilterEmpty() {
        let singleselect = false;
        if (this.filterModel.selects) {
            this.filterModel.selects.forEach(select => {
                singleselect = singleselect || (select.selected.value > 0) || (select.selected.value !== '0'); // 0 is default choice
            });
        }

        return !singleselect;
    }

    countAlerts() {
        this.alertsCount = Object.values(this.alerts).length;
    }

    processAlerts() {
        /*
         * 1.Reduce converts array of alerts into object
         * [{metric: name, _ :{icon: alarmLevel}}] => { resourceType: {alarmLevel: count} }
         * 2. Map converts object into array of alerts sorted by resourceTypes
         * { resourceType: {alarmLevel: count} } => [{resourceType: name,  alarms : [{alarmLevel: count}]}]
         * Note: alarm levels are sorted alphabetically
         */
        const alarmTypes = Object.values(this.healthService.manifest).filter((resource: any) => {
            return resource.id !== 'systems';
        }).reduce((obj: any, item: any) => {
            obj[item.id] = {
                alarms: {
                    error: 0,
                    warning: 0,
                },
                name: item.name
            };
            return obj;
        }, {});
        this.healthService.alertsValues.filter((value: any) => {
            return value.metric !== 'systems';
        }).forEach((item) => {
            alarmTypes[item.metric].alarms[item._.alarm.icon] += 1;
        });
        this.alertCards = Object.values(alarmTypes).map((alarmType: any) => {
            return {
                alerts: Object.entries(alarmType.alarms).map(([level, count]) => {
                    // If level is error and type is server convert to offline. Otherwise append an s to level.
                    const name = level === 'error' && alarmType.name === 'Servers' ? 'Offline' : `${level}s`;
                    return { count, level, name };
                }).sort((a: any, b: any) => a.level < b.level ? -1 : 1),
                name: alarmType.name
            };
        });
        this.alertCardCount = Object.keys(this.alertCards).length;
    }

    initializeHeader() {
        this.tableHeaders = {
            id: 'alerts',
            values: [{
                id: '_',
                name: '',
                values: [
                    {
                        display: 'table',
                        name: '',
                        id: 'alarm',
                    },
                    {
                        display: 'table',
                        name: 'Type',
                        id: 'type',
                        formatClass: 'text'
                    },
                    {
                        display: 'table',
                        name: 'Server',
                        id: 'server',
                        formatClass: 'long-text'
                    },
                    {
                        display: 'table',
                        name: 'Alert',
                        id: 'message'
                    }
                ]
            }]
        };
    }

    setActiveEntity(alarm, updateURI = true) {
        if (alarm && alarm.entity) {
            this.healthLayoutService.layoutReady = false;
            this.healthLayoutService.activeEntity = this.values[alarm.metric][alarm.entity];
            this.activePanelParams = this.healthService.panelParams[alarm.metric];

            if (updateURI) {
                const queryParams: Params = {};
                queryParams.id = alarm.entity;
                queryParams.metric = alarm.metric;
                this.uriService.updateURI(undefined, queryParams);
            }

            if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                this.healthLayoutService.mobileDetailMode = true;
            }

        } else {
            this.resetActiveEntity();
        }
    }

    resetActiveEntity(updateURI = true) {
        if (updateURI) {
            const queryParams: Params = {};
            queryParams.id            = undefined;
            queryParams.metric        = undefined;
            this.uriService.updateURI(undefined, queryParams);
        }
        this.healthLayoutService.resetActiveEntity();
    }

    canSeeTable() {
        return this.tableHeaders && this.healthService.alertsValues && !this.healthLayoutService.mobileDetailMode;
    }

    private setLayout() {
        this.healthLayoutService.setAlertLayout();
    }
}
