import { ElementRef, Injectable } from '@angular/core';
import { BehaviorSubject } from 'rxjs';
import { NxScrollMechanicsService } from '../../services/scroll-mechanics.service';
import { NxHealthService } from './health.service';
import { NxConfigService } from '../../services/nx-config';
import { debounceTime } from 'rxjs/operators';

@Injectable({
    providedIn: 'root'
})
export class NxHealthLayoutService {
    CONFIG: any;
    previousActiveEntity = undefined;
    activeEntitySubject = new BehaviorSubject(undefined);
    fixedLayoutClassSubject = new BehaviorSubject('');
    dimensionsSubject = new BehaviorSubject([]);
    layoutReadySubject = new BehaviorSubject(false);
    metricsValuesCountSubject = new BehaviorSubject(0);
    mobileDetailModeSubject = new BehaviorSubject(false);
    pageSizeSubject = new BehaviorSubject(undefined);
    tableWidthSubject = new BehaviorSubject(0);

    // Common between alerts and metrics
    searchTableAreaSubject = new BehaviorSubject(undefined);
    searchElementSubject = new BehaviorSubject(undefined);

    // Alerts only
    tilesElementSubject = new BehaviorSubject(undefined);

    // Dynamic table elements
    tableElementSubject = new BehaviorSubject(undefined);
    tableHeaderElementSubject = new BehaviorSubject(undefined);
    tableTitleElementSubject = new BehaviorSubject(undefined);

    get activeEntity() {
        return this.activeEntitySubject.getValue();
    }

    set activeEntity(entity: any) {
        this.previousActiveEntity = this.activeEntity;
        this.activeEntitySubject.next(entity);
    }

    get searchTableArea() {
        return this.searchTableAreaSubject.getValue();
    }

    set searchTableArea(element: ElementRef) {
        this.searchTableAreaSubject.next(element);
    }

    get dimensions() {
        return this.dimensionsSubject.getValue();
    }

    set dimensions(dimensions: number[]) {
        this.dimensionsSubject.next(dimensions);
    }

    get fixedLayoutClass() {
        return this.fixedLayoutClassSubject.getValue();
    }

    set fixedLayoutClass(className: string) {
        this.fixedLayoutClassSubject.next(className);
    }

    get layoutReady() {
        return this.layoutReadySubject.getValue();
    }

    set layoutReady(value: boolean) {
        this.layoutReadySubject.next(value);
    }

    get metricsValuesCount() {
        return this.metricsValuesCountSubject.getValue();
    }

    set metricsValuesCount(count) {
        this.metricsValuesCountSubject.next(count);
    }

    get mobileDetailMode() {
        return this.mobileDetailModeSubject.getValue();
    }

    set mobileDetailMode(value: boolean) {
        this.mobileDetailModeSubject.next(value);
    }

    get pageSize() {
        return this.pageSizeSubject.getValue();
    }

    set pageSize(pageSize: number) {
        if (pageSize !== this.pageSize) {
            this.pageSizeSubject.next(pageSize);
        }
    }

    get searchElement() {
        return this.searchElementSubject.getValue();
    }

    set searchElement(element: ElementRef) {
        this.searchElementSubject.next(element);
    }

    get tableElement() {
        return this.tableElementSubject.getValue();
    }

    set tableElement(element: ElementRef) {
        this.tableElementSubject.next(element);
    }

    get tableHeaderElement() {
        return this.tableHeaderElementSubject.getValue();
    }

    set tableHeaderElement(element: ElementRef) {
        this.tableHeaderElementSubject.next(element);
    }

    get tableTitleElement() {
        return this.tableTitleElementSubject.getValue();
    }

    set tableTitleElement(element: ElementRef) {
        this.tableTitleElementSubject.next(element);
    }

    get tableWidth() {
        return this.tableWidthSubject.getValue();
    }

    set tableWidth(width: number) {
        this.tableWidthSubject.next(width);
    }

    get tilesElement() {
        return this.tilesElementSubject.getValue();
    }

    set tilesElement(element: ElementRef) {
        this.tilesElementSubject.next(element);
    }

    constructor(private config: NxConfigService,
                private healthService: NxHealthService,
                private scrollMechanicsService: NxScrollMechanicsService) {
        this.CONFIG = this.config.getConfig();
        this.pageSize = this.CONFIG.layout.tableLarge.rows;

        this.dimensionsSubject.pipe(debounceTime(10)).subscribe(() => {
            if (this.tableHeaderElement) {
                this.setTableDimensions();
            }
        });

        this.tableWidthSubject.subscribe((width) => this.setSearchWidth(width));
        this.activeEntitySubject.subscribe(() => this.setTableDimensions());
    }

    resetActiveEntity() {
        this.activeEntity = undefined;
        this.mobileDetailMode  = false;
    }

    setAlertLayout() {
        const searchElementHeight = this.searchElement ? this.searchElement.nativeElement.offsetHeight : 0;
        const elementTilesHeight = this.tilesElement ? this.tilesElement.nativeElement.offsetHeight : 0;
        if (!this.mobileDetailMode) {
            this.dimensions = [elementTilesHeight, searchElementHeight, 17 /*separator = 1px + padding*/];
        }
        const cannotSetSearch = this.previousActiveEntity === undefined;
        this.setLayout(cannotSetSearch);
    }

    setMetricsLayout() {
        if (this.metricsValuesCount === 1) {
            this.fixedLayoutClass = 'fixedLayout--no-panel';
        } else {
            const elementSearchHeight = this.searchElement ? this.searchElement.nativeElement.offsetHeight : 0;
            if (!this.mobileDetailMode) {
                this.dimensions = [elementSearchHeight + 16];
            }
            this.setLayout();
        }
    }

    setTableDimensions() {
        const windowSize = this.scrollMechanicsService.windowSizeSubject.getValue();

        const ELEMENTS_HEIGHT = this.dimensions.reduce((prev, curr) => prev + curr, 0);
        const THEAD_HEIGHT = this.tableHeaderElement ? this.tableHeaderElement.nativeElement.offsetHeight : 0;
        const PADDING = 16;
        const PAGINATION_HEIGHT = 64;
        const ROW_HEIGHT = 26 ;

        let availSpace = windowSize.height - 4 * PADDING - ELEMENTS_HEIGHT - THEAD_HEIGHT - 48 - PAGINATION_HEIGHT;

        if (this.tableTitleElement) {
            availSpace -= this.tableTitleElement.nativeElement.offsetHeight;
        }

        let pageSize = Math.ceil(availSpace / ROW_HEIGHT);
        if (pageSize < 5) {
            pageSize = 5;
        }
        this.pageSize = pageSize;
        if (this.tableElement && this.tableElement.nativeElement.offsetWidth !== 0) {
            this.tableWidth = this.tableElement.nativeElement.offsetWidth;
        }
        this.healthService.tableReady = true;
    }

    private setLayout(cannotSearchStyle?: boolean) {
        if (!this.tableElement || !this.healthService.tableReady) {
            return;
        }

        // measure table (not wrapper) width
        const table = this.tableElement.nativeElement.querySelectorAll('table')[0];
        this.tableWidth = table ? table.offsetWidth : 0;

        if (this.activeEntity && !this.mobileDetailMode) {
            const areaWidth = this.searchTableArea ? this.searchTableArea.nativeElement.offsetWidth : 0;
            const widthPanel = this.healthService.getPanelWidth();
            const isTableFit = (areaWidth > this.tableWidth + widthPanel + 16); // +gutter
            this.fixedLayoutClass = (isTableFit) ? '' : 'fixedLayout--with-panel';

            if (!cannotSearchStyle && this.searchElement) {
                this.searchElement.nativeElement.style.width = 'auto';
            }
        } else {
            this.fixedLayoutClass = 'fixedLayout--no-panel';
        }
        this.layoutReady = true;
    }

    private setSearchWidth(width) {
        if (this.searchElement) {
            this.searchElement.nativeElement.style.width = `${width}px`;
        }
    }
}
