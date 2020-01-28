import {
    AfterViewInit,
    Component,
    ElementRef,
    OnInit,
    ViewChild,
    ViewEncapsulation
} from '@angular/core';
import { ActivatedRoute }                      from '@angular/router';
import { NxAccountService }                    from '../../../services/account.service';
import { NxConfigService }                     from '../../../services/nx-config';
import { NxSystem, NxSystemService }           from '../../../services/system.service';
import { NxMenuService }                       from '../../../components/menu/menu.service';
import { NxHealthService }                     from '../health.service';
import { NxUriService }                        from '../../../services/uri.service';
import { NxLanguageProviderService }           from '../../../services/nx-language-provider';
import { SubscriptionLike }                    from 'rxjs';
import { AutoUnsubscribe }                     from 'ngx-auto-unsubscribe';
import { NxScrollMechanicsService }            from '../../../services/scroll-mechanics.service';

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

    mobileDetailMode: boolean;
    breakpoint: string;

    manifest: any;
    values: any;
    alarms: any;

    metricValuesLen: number;

    selectedData: any;
    selectedPanelData: any;
    selectedValues: any;

    menu: any;
    activeEntity: any;
    metricName: string;

    objectValues = Object.values;

    routeSubscription: SubscriptionLike;
    breakpointSubscription: SubscriptionLike;
    tableReadySubscription: SubscriptionLike;

    containerDimensions: any = [];

    windowSizeSubscription: SubscriptionLike;
    tableWidthSubscription: SubscriptionLike;

    fixedLayoutClass: string;
    layoutReady: boolean;

    @ViewChild('search', { static: false }) elementSearch: ElementRef;
    @ViewChild('tableContainer', { static: false }) tableContainer: ElementRef;
    @ViewChild('area', { static: false }) area: ElementRef;

    constructor(private accountService: NxAccountService,
                private configService: NxConfigService,
                private languageService: NxLanguageProviderService,
                private systemService: NxSystemService,
                private route: ActivatedRoute,
                private menuService: NxMenuService,
                private healthService: NxHealthService,
                private uri: NxUriService,
                private scrollMechanicsService: NxScrollMechanicsService,
    ) {
        this.CONFIG = this.configService.getConfig();
        this.LANG  = this.languageService.getTranslations();
        this.containerDimensions = [];

        this.filterModel = {
            query: ''
        };
        this.fixedLayoutClass = '';

        this.tableWidthSubscription = this.scrollMechanicsService
                .elementTableWidthSubject
                .subscribe(width => {
                    if (this.elementSearch) {
                        this.elementSearch.nativeElement.style.width = width + 'px';
                    }
                });
    }

    ngOnInit(): void {
        this.initialId = this.route.snapshot.queryParamMap.get('id');
        let searchParam = this.route.snapshot.queryParamMap.get('search');

        this.routeSubscription = this.route
            .params
            .subscribe((params: any) => {
                this.metricId = params.metric;
                this.metricName = this.healthService.manifest[this.metricId].name;
                this.menuService.setSection(this.metricId);
                this.selectedData = this.healthService.tableHeaders[this.metricId];
                this.selectedPanelData = this.healthService.panelParams[this.metricId];
                this.metricValuesLen = this.metricId in this.healthService.values ? Object.values(this.healthService.values[this.metricId]).length : 0;
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

                this.setLayout();
            });

        this.windowSizeSubscription = this.scrollMechanicsService.windowSizeSubject.subscribe(({ width }) => {
            if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                this.mobileDetailMode = (this.activeEntity !== undefined);
            } else {
                this.mobileDetailMode = false;
            }
            this.setLayout();
        });

        this.tableReadySubscription = this.healthService.tableReadySubject.subscribe(ready => {
            if (ready) {
                this.setLayout();
            }
        });
    }

    ngAfterViewInit() {
        this.setLayout();
    }

    ngOnDestroy() {}

    handleInitialId() {
        if (this.initialId) {
            this.setActiveEntity(this.initialId);
            this.initialId = undefined;
        }
    }

    modelChanged(model) {
        this.filterModel.query = model.query;
        this.search();
    }

    search() {
        this.selectedValues = this.healthService
                                  .itemsSearch(this.healthService.values[this.metricId], this.filterModel) || {};

        this.handleInitialId();
        if (this.activeEntity && !this.selectedValues[this.activeEntity.id]) {
            this.resetActiveEntity();
        }
    }

    setActiveEntity(entity) {
        const queryParams: Params = {};
        this.layoutReady = false;

        if (entity) {
            if (typeof entity === 'string') {
                this.activeEntity = this.selectedValues[entity];
                if (!this.activeEntity) {
                    queryParams.id = undefined;
                    this.uri.updateURI(undefined, queryParams);
                }
            } else {
                this.activeEntity = entity;
                queryParams.id = entity.id;
                this.uri.updateURI(undefined, queryParams);

                if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                    this.mobileDetailMode = true;
                }
            }
        } else {
            this.resetActiveEntity();
        }

        this.setLayout();
    }

    resetActiveEntity(updateURI = true) {
        this.activeEntity = undefined;
        if (updateURI) {
            const queryParams: Params = {};
            queryParams.id = undefined;
            this.uri.updateURI(undefined, queryParams);
        }
        this.mobileDetailMode = false;
        this.setLayout();
    }

    private setLayout() {
        setTimeout(() => {
            if (this.metricValuesLen === 1) {
                this.fixedLayoutClass = 'fixedLayout--no-panel';
            } else {

                const elementSearchHeight = this.elementSearch ? this.elementSearch.nativeElement.offsetHeight : 0;
                this.containerDimensions = [elementSearchHeight + 16];

                if (this.tableContainer && this.healthService.tableReady) {

                    // measure table (not wrapper) width
                    const tableWidth = this.tableContainer.nativeElement.querySelectorAll('table')[0].offsetWidth;
                    this.containerDimensions = [elementSearchHeight + 16, 0]; // trick table's onChanges will pick new dimensions
                    // area available
                    const areaWidth = this.area.nativeElement.offsetWidth;
                    // area available to the table (~80% + gutters
                    const availAreaWidth = areaWidth * .78 + 46;

                    const isTableFit = (availAreaWidth > tableWidth) && !this.mobileDetailMode;
                    if (this.activeEntity && !this.mobileDetailMode) {
                        this.fixedLayoutClass = (isTableFit) ? '' : 'fixedLayout--with-panel';
                    } else {
                        this.fixedLayoutClass = (isTableFit) ? '' : 'fixedLayout--no-panel';
                    }

                    this.layoutReady = true;
                }

                if (this.mobileDetailMode && this.activeEntity) {
                    this.fixedLayoutClass = 'fixedLayout--no-panel';
                    this.layoutReady = true;
                }
            }
        });
    }
}
