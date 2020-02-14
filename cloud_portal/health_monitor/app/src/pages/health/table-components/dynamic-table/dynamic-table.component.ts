import {
    Component, Input, Output,
    EventEmitter, OnChanges, SimpleChanges,
    OnInit, ViewEncapsulation,
    ViewChild, ElementRef, AfterViewInit,
}                                   from '@angular/core';
import { Location }                 from '@angular/common';
import { ActivatedRoute, Router }   from '@angular/router';
import { DeviceDetectorService }    from 'ngx-device-detector';
import { NxConfigService }          from '../../../../services/nx-config';
import { NxUtilsService }           from '../../../../services/utils.service';
import { NxUriService }             from '../../../../services/uri.service';
import { NxHealthService }          from '../../health.service';
import { NxScrollMechanicsService } from '../../../../services/scroll-mechanics.service';
import { SubscriptionLike }         from 'rxjs';
import { AutoUnsubscribe }          from 'ngx-auto-unsubscribe';
import { NxHealthLayoutService } from '../../health-layout.service';
import { delay } from 'rxjs/operators';

interface Params {
    [key: string]: any;
}

const ALARM_ORDER = {
    'error': 2,
    'warning': 1,
    '': 0
};

const TEXT_FORMATS = ['longText', 'shortText', 'text'];
const GROUP_ID = 0;
const PARAM_ID = 1;
const SORT_DIR = 2;

@AutoUnsubscribe()
@Component({
    selector     : 'nx-dynamic-table',
    templateUrl  : './dynamic-table.component.html',
    styleUrls    : ['./dynamic-table.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class NxDynamicTableComponent implements OnChanges, OnInit, AfterViewInit {
    @Input() tableHeader = '';
    @Input() headers: any = [];
    @Input() elements: any = [];
    @Input() showGroups = true;

    @Output() public onRowClick: EventEmitter<any> = new EventEmitter<any>();

    CONFIG: any;

    _elements: any = [];
    params: any = {};

    public selectedEntity;
    public selectedGroup;
    public selectedHeader;
    public showHeaders;

    private sortOrderASC: boolean;

    offset: number;
    currentPage: number;
    pageSize: number;
    startIndex: number;
    pager: any = {};
    pagedItems: any[];
    pagerMaxSize: number;
    serviceParams;
    serviceHeaders;

    windowSize: any = {};
    clientHeight: number;
    offsetHeight: number;
    scrollHeight: number;
    showHorizontalTooltip: boolean;
    hideTooltip: any;
    mobileDetailMode: boolean;

    pageSubscription: SubscriptionLike;
    resizeSubscription: SubscriptionLike;
    locationSubscription: SubscriptionLike;
    queryParamSubscription: SubscriptionLike;

    @ViewChild('tableHead', { static: false }) tableHeadElement: ElementRef;
    @ViewChild('tableTitle', { static: false }) tableTitleElement: ElementRef;
    @ViewChild('nxTable', { static: false }) tableElement: ElementRef;
    @ViewChild('tooltip', { static: false }) tableTooltip: ElementRef;

    constructor(private configService: NxConfigService,
                private uri: NxUriService,
                private utilsService: NxUtilsService,
                private router: Router,
                private route: ActivatedRoute,
                private location: Location,
                private healthService: NxHealthService,
                private scrollMechanicsService: NxScrollMechanicsService,
                private deviceDetectorService: DeviceDetectorService,
                private healthLayoutService: NxHealthLayoutService,
    ) {
        this.CONFIG = this.configService.getConfig();
        this.elements = this.elements || [];

        this.pagedItems = [];
        this.currentPage = 1;
        this.healthService.tableReady = false;
        this.showHorizontalTooltip = false;

        this.resizeSubscription = this.scrollMechanicsService.windowSizeSubject.subscribe(() => {
            this.mobileDetailMode = this.healthLayoutService.activeEntity && this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg);
            if (this.tableElement) {
                this.healthLayoutService.tableWidth = this.tableElement.nativeElement.offsetWidth;
            }

            this.setPagerSize();
        });

        this.locationSubscription = this.location.subscribe((event: PopStateEvent) => {
            // force view component update without URI update
            setTimeout(() => {
                this.params = {...this.route.snapshot.queryParams};

                this.startIndex = this.params.index || 0;

                if (this.params.sortBy) {
                    this.sortBy(this.params.sortBy);
                } else {
                    this.sortOrderASC   = true;
                    this.selectedGroup  = undefined;
                    this.selectedHeader = undefined;
                }

                this.setPage(undefined, this.startIndex);
            });
        });
    }

    ngOnInit() {
        this.params = {...this.route.snapshot.queryParams};
        if (this.params.sortBy) {
            this.sortBy(this.params.sortBy);
        } else {
            this.sortOrderASC = true;
            this.selectedGroup = undefined;
            this.selectedHeader = undefined;
        }

        if (this.healthLayoutService.activeEntity) {
            this.setPagerSize();
            this.startIndex = this._elements.findIndex(elem => {
                return this.healthLayoutService.activeEntity === elem;
            });
        }
        if ([undefined, -1].includes(this.startIndex)) {
            this.startIndex = parseInt(this.params.index) || 0;
        }
        this.healthService.tableReady = true;
    }
    initLayoutService() {
        this.healthLayoutService.tableHeaderElement = this.tableHeadElement;
        this.healthLayoutService.tableTitleElement = this.tableTitleElement;
        this.healthLayoutService.tableElement = this.tableElement;

        this.pageSubscription = this.healthLayoutService.pageSizeSubject.pipe(delay(0)).subscribe(pageSize => {
            this.pageSize = pageSize;
            this.setPage(1);
        });

        this.healthLayoutService.activeEntitySubject.subscribe((activeEntity: any) => {
            this.setActiveEntity(activeEntity);
        });
    }

    private setPagerSize() {
        if (this.healthLayoutService.activeEntity && this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.xl)) {
            this.pagerMaxSize = this.CONFIG.ipvd.pagerMaxSizeSmall;
        } else {
            this.pagerMaxSize = this.CONFIG.ipvd.pagerMaxSize;
        }
    }

    private setActiveEntity(activeEntity) {
        this.selectedEntity = activeEntity;
        if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
            this.mobileDetailMode = !!this.selectedEntity;
        }

        if (!activeEntity && !this.healthService.tableReady) {
            this.setPage(undefined, this.startIndex || 0);
        }
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.id;
    }

    ngOnChanges(changes: SimpleChanges) {
        let resetURI;

        if (changes.headers) {
            this.selectedHeader = undefined;

            if (changes.headers.previousValue !== undefined &&
                JSON.stringify(changes.headers.previousValue) !== JSON.stringify(changes.headers.currentValue)) {
                resetURI = true;
            }
        }

        if (changes.elements) {
            this._elements = Object.values(changes.elements.currentValue);
            this.setPage(1);
            if (this.tableElement) {
                this.setTableDimensions();
            }
        }

        if (resetURI) {
            setTimeout(() => {
                const queryParams: Params = {};
                queryParams.sortBy = undefined;
                queryParams.page = undefined;
                queryParams.index = undefined;

                this.uri
                    .updateURI(undefined, queryParams)
                    .then(() => {
                        this.sortOrderASC = true;
                        this.selectedHeader = undefined;
                    });
            });
        }
    }

    private setTableDimensions() {
        setTimeout(() => this.healthLayoutService.setTableDimensions());
    }

    ngAfterViewInit(): void {
        this.initLayoutService();
        if (this.healthLayoutService.dimensions) {
            this.setTableDimensions();
        }
    }

    ngOnDestroy() {
    }

    showTooltip (event) {
        if (this.deviceDetectorService.browser.toLowerCase() !== 'ie') {
            this.showHorizontalTooltip = true;
            if (this.hideTooltip) {
                clearTimeout(this.hideTooltip);
            }
            this.hideTooltip = setTimeout(() => {
                this.showHorizontalTooltip = false;
                this.hideTooltip           = undefined;
            }, 1000);
        }
    }

    sortBy(param) {
        const sortBy = param.split(',');
        this.sortOrderASC = (sortBy[SORT_DIR] === 'ASC');
        this.selectedGroup = sortBy[GROUP_ID];
        this.selectedHeader = sortBy[PARAM_ID];

        this.toggleSort(sortBy[GROUP_ID], sortBy[PARAM_ID], false);
    }

    setClickedRow(element) {
        this.onRowClick.emit(element);
        this.selectedEntity = element;
    }

    setPagedItems(startIndex) {
        const page = Math.floor(this.startIndex / this.pageSize) + 1;
        startIndex = (page - 1) * this.pageSize;
        if (page !== this.currentPage) {
            this.currentPage = page;
        }

        const endIndex = startIndex + this.pageSize;
        this.pagedItems = this._elements.slice(startIndex, endIndex);

        // TODO: Remove if table dimensions timeout can be removed in CLOUD-4233
        if (this.healthService.tableReady) {
            this.startIndex = startIndex;
        }
    }

    setPage(page: number, startIndex?) {
        // TODO: possible optimization - we may not need snapshot params here
        if (this.mobileDetailMode || startIndex === 0 && this.params.index === undefined) {
            return;
        }

        this.params = { ...this.route.snapshot.queryParams };
        if (page) {
            const numPages = Math.ceil(this._elements.length / this.pageSize);
            // outsmarting pagination component which fire (pageChange) if currentPage > numPages
            // which causes panel to be disposed on window resize if table resize and we're on last page -- TT
            // for more details -> ask me no later than 3 hours after I commit the code
            if (this.currentPage !== page && this.currentPage <= numPages) {
                this.setClickedRow(undefined);
            }
            this.currentPage = page;
            this.startIndex = (page - 1) * this.pageSize;
        } else {
            this.startIndex = startIndex || 0;
        }

        // preserve window offset
        this.uri.pageOffset = window.pageYOffset;
        setTimeout(() => this.setPagedItems(this.startIndex));
        const index = (this.startIndex === 0) ? undefined : this.startIndex;
        const pageParam = this.params && parseInt(this.params.index, 10) || undefined;

        if (pageParam !== index) {
            const queryParams: Params = {};
            queryParams.index = (this.currentPage === 1) ? undefined : this.startIndex;

            this.uri.updateURI(this.uri.getURL(), queryParams);
        }
    }

    getCleanTitle(text: string): string {
        return text.replace(/\<br\>/g, ' ')
                   .replace(/\<\/?span\>/g, '');
    }

    isBoolean(x: any): boolean {
        return !(typeof x === 'string' || typeof x === 'number');
    }

    toggleSort(groupId, paramId, updateURI?, format?) {
        if (this.selectedGroup !== groupId || this.selectedHeader !== paramId) {
            this.sortOrderASC = TEXT_FORMATS.includes(format);
        }
        this.selectedGroup = groupId;
        this.selectedHeader = paramId;

        function sortFunc() {
            switch (paramId) {
                case 'alarm':
                    return (elm) => {
                        return elm[groupId] && elm[groupId][paramId] && ALARM_ORDER[elm[groupId][paramId].icon] || '';
                    };
                case 'resolution':
                    return (elm) => {
                        if (elm[groupId] && elm[groupId][paramId] && elm[groupId][paramId].value) {
                            const res = elm[groupId][paramId].value.toLowerCase().split('x');

                            if (res.length === 2) {
                                return res[0] * res[1];
                            } else {
                                return 0; // invalid format
                            }
                        } else {
                            return 0; // no data
                        }
                    };
                default:
                    return (elm) => {
                        const format = elm[groupId] && elm[groupId][paramId] && elm[groupId][paramId].formatClass || undefined;

                        if (['longText', 'long-text', 'shortText', 'short-text', 'text', 'no-max-width'].includes(format)) {
                            return elm[groupId] && elm[groupId][paramId] && elm[groupId][paramId].text || '';
                        }
                        return elm[groupId] && elm[groupId][paramId] && elm[groupId][paramId].value || 0;
                    };
            }
        }

        this._elements.sort(NxUtilsService.byParam(sortFunc(), this.sortOrderASC));
        this.sortOrderASC = !this.sortOrderASC;

        if (updateURI || updateURI === undefined) {
            const queryParams: Params = {};

            queryParams.page   = undefined;
            queryParams.sortBy = groupId + ',' + paramId;
            queryParams.sortBy += (this.sortOrderASC) ? ',ASC' : ',DESC';
            this.params.sortBy = queryParams.sortBy;
            this.uri.updateURI(undefined, queryParams);

            setTimeout(() => this.setPage(1));
        }
    }

    private getTitle(item, headerGroupId, headerId) {
        let title;
        if (item && item[headerGroupId] && item[headerGroupId][headerId]) {
            title = item[headerGroupId][headerId].tooltip || item[headerGroupId][headerId].text;
        }
        if (title === undefined) {
            title = '';
        }
        return title;
    }
}
