import {
    Component, Input, Output,
    EventEmitter, OnChanges, SimpleChanges,
    OnInit, ViewEncapsulation,
    ViewChild, ElementRef, AfterViewInit,
}                                   from '@angular/core';
import { ActivatedRoute, Router }   from '@angular/router';
import { DeviceDetectorService }    from 'ngx-device-detector';
import { NxConfigService }          from '../../../../services/nx-config';
import { NxUtilsService }           from '../../../../services/utils.service';
import { NxUriService }             from '../../../../services/uri.service';
import { NxHealthService }          from '../../health.service';
import { NxScrollMechanicsService } from '../../../../services/scroll-mechanics.service';
import { SubscriptionLike }         from 'rxjs';
import { AutoUnsubscribe }          from 'ngx-auto-unsubscribe';

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
    @Input() dimensions;
    @Input() activeEntity;
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
    tableScrollFixed: boolean;
    elementWidth: any;
    showHorizontalTooltip: boolean;
    hideTooltip: any;
    mobileDetailMode: boolean;

    resizeSubscription: SubscriptionLike;

    @ViewChild('thead', { static: false }) thead: ElementRef;
    @ViewChild('tableHeaderElement', { static: false }) tableHeaderElement: ElementRef;
    @ViewChild('nxTable', { static: false }) dataTable: ElementRef;
    @ViewChild('tooltip', { static: false }) tableTooltip: ElementRef;

    // CSS does not use CONFIG so this is here to avoid confusion if changing the value
    private static ROW_HEIGHT = 26;

    constructor(private configService: NxConfigService,
                private uri: NxUriService,
                private utilsService: NxUtilsService,
                private router: Router,
                private route: ActivatedRoute,
                private healthService: NxHealthService,
                private scrollMechanicsService: NxScrollMechanicsService,
                private deviceDetectorService: DeviceDetectorService,
    ) {
        this.CONFIG = this.configService.getConfig();
        this.elements = this.elements || [];

        this.pagedItems = [];
        this.pagerMaxSize = this.CONFIG.ipvd.pagerMaxSize;
        this.currentPage = 1;
        this.pageSize = this.CONFIG.layout.tableLarge.rows;
        this.healthService.tableReady = false;
        this.showHorizontalTooltip = false;

        this.resizeSubscription = this.scrollMechanicsService.windowSizeSubject.subscribe(() => {
            this.mobileDetailMode = this.activeEntity && this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg);

            if (this.dataTable) {
                setTimeout(() => this.scrollMechanicsService.setElementTableWidth(this.dataTable.nativeElement.offsetWidth));
            }
        });
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.id;
    }

    ngOnChanges(changes: SimpleChanges) {
        let resetURI;
        let setDimensions;

        if (changes.activeEntity && !changes.activeEntity.firstChange) {
            this.selectedEntity = changes.activeEntity.currentValue;
            if (this.scrollMechanicsService.mediaQueryMax(NxScrollMechanicsService.MEDIA.lg)) {
                this.mobileDetailMode = !!this.selectedEntity;
            }
            // TODO: Try to remove timeout in CLOUD-4233
            setTimeout(() => {
                if (this.dataTable && !this.mobileDetailMode) {
                    this.scrollMechanicsService.setElementTableWidth(this.dataTable.nativeElement.offsetWidth);
                }
            });

            if (!changes.activeEntity.firstChange && !this.healthService.tableReady) {
                setDimensions = true;
                this.setPage(undefined, this.startIndex || 0);
            }
        }

        if (changes.headers) {
            this.selectedHeader = undefined;

            if (changes.headers.previousValue !== undefined &&
                    changes.headers.previousValue !== changes.headers.currentValue) {
                resetURI = true;
            }
        }

        if (changes.elements) {
            this._elements = Object.values(changes.elements.currentValue);
            if (!changes.elements.firstChange) {
                resetURI = true;
                if (this.dataTable) {
                    const tableWrapper = this.dataTable.nativeElement.querySelectorAll('.table-wrapper')[0];
                    tableWrapper.scrollLeft = 0;
                }

                this.setPage(1);
                setDimensions = true;
            }
        }

        if (changes.dimensions &&
            !changes.dimensions.firstChange &&
            changes.dimensions.currentValue.length &&
            JSON.stringify(changes.dimensions.currentValue) !== JSON.stringify(changes.dimensions.previousValue)) { // break circular dep

            setDimensions = true;
        }

        if (setDimensions) {
            // TODO: Try to remove timeout in CLOUD-4233
            setTimeout(() => {
                if (this.dataTable) {
                    this.setTableDimensions();
                }
            });
        }

        if (resetURI) {
            setTimeout(() => {
                const queryParams: Params = {};
                queryParams.sortBy = undefined;
                queryParams.page = undefined;
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
        this.windowSize = this.scrollMechanicsService.windowSizeSubject.getValue();

        const ELEMENTS_HEIGHT = this.dimensions.reduce((prev, curr) => prev + curr, 0);
        const THEAD_HEIGHT = this.thead.nativeElement.offsetHeight;
        const PADDING = 16;
        const PAGINATION_HEIGHT = 64;

        let availSpace = this.windowSize.height - 4 * PADDING - ELEMENTS_HEIGHT - THEAD_HEIGHT - 48 - PAGINATION_HEIGHT;

        if (this.tableHeader) {
            availSpace -= this.tableHeaderElement.nativeElement.offsetHeight;
        }

        this.pageSize = Math.ceil(availSpace / NxDynamicTableComponent.ROW_HEIGHT);
        if (this.pageSize < 5) {
            this.pageSize = 5;
        }

        // TODO: Remove in CLOUD-4233
        setTimeout(() => {
            if (this.dataTable.nativeElement.offsetWidth !== 0) {
                this.scrollMechanicsService.setElementTableWidth(this.dataTable.nativeElement.offsetWidth);
            }

            this.healthService.tableReady = true;
        }, 100);

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

        if (this.activeEntity) {
            this.startIndex = this._elements.findIndex(elem => {
                return this.activeEntity === elem;
            });
        }
        if ([undefined, -1].includes(this.startIndex)) {
            this.startIndex = parseInt(this.params.index) || 0;
        }

        // TODO: Remove if table dimensions timeout can be removed in CLOUD-4233
        this.healthService.tableReadySubject.subscribe(ready => {
            if (ready) {
                this.setPage(undefined, this.startIndex);
            }
        });
    }

    ngAfterViewInit(): void {
        if (this.dimensions.length) {
            setTimeout(() => this.setTableDimensions());
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

    setPagedItems(startIndex?) {
        if (!startIndex) {
            startIndex = (this.currentPage - 1) * this.pageSize;
        } else {
            const page = Math.floor(this.startIndex / this.pageSize) + 1;
            startIndex = (page - 1) * this.pageSize;
            if (page !== this.currentPage) {
                this.currentPage = page;
            }
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
        if (this.mobileDetailMode) {
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
        }

        if (startIndex) {
            this.startIndex = startIndex;
        }

        // preserve window offset
        this.uri.pageOffset = window.pageYOffset;
        this.setPagedItems(startIndex);
        const index = (this.startIndex === 0) ? undefined : this.startIndex;

        if (this.params && parseInt(this.params.index, 10) !== index) { // this.params.page is string - no strict comparison
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

        if (updateURI || updateURI === undefined) {
            const queryParams: Params = {};

            queryParams.page = undefined;
            queryParams.sortBy = groupId + ',' + paramId;
            queryParams.sortBy += (this.sortOrderASC) ? ',ASC' : ',DESC';
            this.params.sortBy = queryParams.sortBy;
            this.uri.updateURI(undefined, queryParams);
        }

        function sortFunc() {
            if (paramId === 'alarm') {
                return (elm) => {
                    return elm[groupId] && elm[groupId][paramId] && ALARM_ORDER[elm[groupId][paramId].icon] || '';
                };
            } else {
                return (elm) => {
                    return elm[groupId] && elm[groupId][paramId] && elm[groupId][paramId].text || '';
                };
            }
        }

        this._elements.sort(NxUtilsService.byParam(sortFunc(), this.sortOrderASC));
        this.sortOrderASC = !this.sortOrderASC;

        if (updateURI || updateURI === undefined) {
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
