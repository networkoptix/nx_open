import {
    AfterViewInit, Component,
    ElementRef, OnInit, ViewChild,
    ViewEncapsulation
}                                    from '@angular/core';
import { ActivatedRoute }            from '@angular/router';
import { Location }                  from '@angular/common';
import { NxAccountService }          from '../../../services/account.service';
import { NxConfigService }           from '../../../services/nx-config';
import { NxSystem, NxSystemService } from '../../../services/system.service';
import { NxMenuService }             from '../../../components/menu/menu.service';
import { NxHealthService }           from '../health.service';
import { NxUriService }              from '../../../services/uri.service';
import { NxLanguageProviderService } from '../../../services/nx-language-provider';
import { SubscriptionLike }          from 'rxjs';
import { AutoUnsubscribe }           from 'ngx-auto-unsubscribe';
import { NxScrollMechanicsService }  from '../../../services/scroll-mechanics.service';
import { delay, throttleTime } from 'rxjs/operators';
import { NxHealthLayoutService } from '../health-layout.service';

interface Params {
    [key: string]: any;
}

@AutoUnsubscribe()
@Component({
    selector   : 'nx-system-metrics-component',
    templateUrl: 'metrics.component.html',
    styleUrls  : ['metrics.component.scss'],
    encapsulation: ViewEncapsulation.None,
})
export class NxSystemMetricsComponent implements OnInit, AfterViewInit {
    CONFIG: any;
    LANG: any;
    account: any;

    filterModel: any;
    system: NxSystem;
    metricId: any;
    initialId: any;

    layoutReady: boolean;
    fixedLayoutClass: string;
    breakpoint: string;

    manifest: any;
    values: any;
    alarms: any;

    selectedData: any;
    selectedPanelData: any;
    selectedValues: any;

    menu: any;
    metricName: string;

    objectValues = Object.values;

    selectedSubscription: SubscriptionLike;
    routeSubscription: SubscriptionLike;
    queryParamSubscription: SubscriptionLike;
    breakpointSubscription: SubscriptionLike;
    windowSizeSubscription: SubscriptionLike;
    locationSubscription: SubscriptionLike;
    layoutReadySubscription: SubscriptionLike;
    fixedLayoutClassSubscription: SubscriptionLike;
    activeEntitySubscription: SubscriptionLike;

    @ViewChild('search', { static: false }) searchElement: ElementRef;
    @ViewChild('area', { static: false }) areaElement: ElementRef;

    constructor(private accountService: NxAccountService,
                private configService: NxConfigService,
                private languageService: NxLanguageProviderService,
                private systemService: NxSystemService,
                private route: ActivatedRoute,
                private location: Location,
                private menuService: NxMenuService,
                private healthService: NxHealthService,
                private uri: NxUriService,
                private scrollMechanicsService: NxScrollMechanicsService,
                private healthLayoutService: NxHealthLayoutService
    ) {
        this.CONFIG = this.configService.getConfig();
        this.LANG  = this.languageService.getTranslations();
        this.filterModel = {
            query: ''
        };
    }

    ngOnInit(): void {
        this.initialId = this.route.snapshot.queryParamMap.get('id');
        let searchParam = this.route.snapshot.queryParamMap.get('search');

        this.locationSubscription = this.location.subscribe((event: PopStateEvent) => {
            // force view component update without URI update
            setTimeout(() => {
                const params = {...this.route.snapshot.queryParams};

                if (params.id) {
                    this.setActiveEntity(params.id);
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
                this.filterModel.query = '';
                this.resetActiveEntity();
                this.search();
            }
        });

        this.routeSubscription = this.route
            .params
            .subscribe((params: any) => {
                this.metricId = params.metric;
                this.metricName = this.healthService.manifest[this.metricId].name;
                this.menuService.setSection(this.metricId);
                this.selectedData = this.healthService.tableHeaders[this.metricId];
                this.selectedPanelData = this.healthService.panelParams[this.metricId];
                this.healthLayoutService.metricsValuesCount = this.metricId in this.healthService.values ? Object.values(this.healthService.values[this.metricId]).length : 0;
                this.resetActiveEntity(false);

                if (!searchParam || !searchParam.length) {
                    this.filterModel.query = '';
                    this.selectedValues = this.healthService.values[this.metricId] || {};
                    this.handleInitialId();
                } else {
                    this.filterModel.query = searchParam;
                    searchParam = undefined;
                    this.search();
                }
            });

        this.windowSizeSubscription = this.scrollMechanicsService.windowSizeSubject.subscribe(({ width }) => {
            if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                this.healthLayoutService.mobileDetailMode = (this.healthLayoutService.activeEntity !== undefined);
            } else {
                this.healthLayoutService.mobileDetailMode = false;
            }
            this.setLayout();
        });
    }

    ngAfterViewInit() {
        this.healthLayoutService.dimensions = [];
        this.healthLayoutService.searchTableArea = this.areaElement;
        this.healthLayoutService.searchElement = this.searchElement;

        this.fixedLayoutClassSubscription = this.healthLayoutService.fixedLayoutClassSubject.pipe(delay(0)).subscribe((className: string) => {
            this.fixedLayoutClass = className;
        });

        this.layoutReadySubscription = this.healthLayoutService.layoutReadySubject.pipe(delay(0)).subscribe((value: boolean) => {
            this.layoutReady = value;
        });

        this.activeEntitySubscription = this.healthLayoutService.activeEntitySubject.pipe(delay(0)).subscribe(() => {
            this.setLayout();
        });
    }

    ngOnDestroy() {
        this.healthLayoutService.resetActiveEntity();
    }

    handleInitialId() {
        if (this.initialId) {
            this.setActiveEntity(this.initialId);
            this.initialId = undefined;
        }
    }

    modelChanged(model) {
        if (this.filterModel.query !== model.query) {
            this.filterModel.query = model.query;
            this.search();
        }
    }

    search() {
        this.selectedValues = this.healthService.itemsSearch(this.healthService.values[this.metricId], this.filterModel);

        this.handleInitialId();
        if (this.healthLayoutService.activeEntity && !this.selectedValues[this.healthLayoutService.activeEntity.id]) {
            this.resetActiveEntity();
        }
    }

    setActiveEntity(entity) {
        const queryParams: Params = {};
        this.layoutReady = this.healthLayoutService.activeEntity ? true : false;

        if (entity) {
            // Happens when we get the entity from the url.
            if (typeof entity === 'string') {
                this.healthLayoutService.activeEntity = this.selectedValues[entity];
                if (!this.healthLayoutService.activeEntity) {
                    queryParams.id = undefined;
                    this.uri.updateURI(undefined, queryParams);
                }
            } else {
                this.healthLayoutService.activeEntity = entity;
                queryParams.id = entity.id;
                this.uri.updateURI(undefined, queryParams);

                if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                    this.healthLayoutService.mobileDetailMode = true;
                }
            }
        } else {
            this.resetActiveEntity();
        }
    }

    resetActiveEntity(updateURI = true) {
        if (updateURI) {
            const queryParams: Params = {};
            queryParams.id            = undefined;
            this.uri.updateURI(undefined, queryParams);
        }
        this.healthLayoutService.resetActiveEntity();
    }

    private setLayout() {
        this.healthLayoutService.setMetricsLayout();
    }
}
