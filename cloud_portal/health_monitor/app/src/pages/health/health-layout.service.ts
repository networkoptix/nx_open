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
    activeEntitySubject = new BehaviorSubject(undefined);
    fixedLayoutClassSubject = new BehaviorSubject('');
    dimensionsSubject = new BehaviorSubject([]);
    layoutReadySubject = new BehaviorSubject(false);
    metricsValuesCountSubject = new BehaviorSubject(0);
    mobileDetailModeSubject = new BehaviorSubject(false);
    pageSizeSubject = new BehaviorSubject(undefined);
    tableWidthSubject = new BehaviorSubject(0);

    searchTableAreaSubject = new BehaviorSubject(undefined);
    searchElementSubject = new BehaviorSubject(undefined);
    tableElementSubject = new BehaviorSubject(undefined);
    tableHeaderElementSubject = new BehaviorSubject(undefined);
    tableTitleElementSubject = new BehaviorSubject(undefined);
    tilesElementSubject = new BehaviorSubject(undefined);

    private static ROW_HEIGHT = 26;

    get activeEntity() {
        return this.activeEntitySubject.getValue();
    }

    set activeEntity(entity: any) {
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
        if (dimensions !== this.dimensions) {
            this.dimensionsSubject.next(dimensions);
        }
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
        this.dimensions = [elementTilesHeight, searchElementHeight, 17 /*separator = 1px + padding*/];

        if (this.tableElement && this.healthService.tableReady) {
            this.tableWidth = this.tableElement.nativeElement.querySelectorAll('table')[0].offsetWidth;
            // area available
            const areaWidth = this.searchTableArea.nativeElement.offsetWidth;
            // area available to the table (- gutter)
            const availAreaWidth = areaWidth - NxHealthService.PANEL_WIDTH - 16;

            const isTableFit = (availAreaWidth > this.tableWidth) && !this.mobileDetailMode;
            if (this.activeEntity && !this.mobileDetailMode) {
                this.fixedLayoutClass = (isTableFit) ? '' : 'fixedLayout--with-panel';
            } else {
                this.fixedLayoutClass = 'fixedLayout--no-panel';
            }

            if (this.mobileDetailMode && this.activeEntity) {
                this.fixedLayoutClass = 'fixedLayout--no-panel';
            }
            this.layoutReady = true;
        }
    }

    setMetricsLayout() {
        if (!this.searchTableArea) {
            return;
        }
        if (this.metricsValuesCount === 1) {
            this.fixedLayoutClass = 'fixedLayout--no-panel';
        } else {
            const elementSearchHeight = this.searchElement ? this.searchElement.nativeElement.offsetHeight : 0;
            // Don't ask why this segment is duplicated ... it's important and it's working -- TT
            if (this.tableElement && this.healthService.tableReady) {
                // measure table (not wrapper) width
                this.tableWidth = this.tableElement.nativeElement.querySelectorAll('table')[0].offsetWidth;

                if (!this.mobileDetailMode) {
                    this.dimensions = [elementSearchHeight + 16, 0]; // trick table's onChanges will pick new dimensions
                }
                // area available
                const areaWidth = this.searchTableArea ? this.searchTableArea.nativeElement.offsetWidth : 0;
                // area available to the table (- gutter)
                const availAreaWidth = areaWidth - NxHealthService.PANEL_WIDTH - 16;

                const isTableFit = (availAreaWidth > this.tableWidth) && !this.mobileDetailMode;
                if (this.activeEntity && !this.mobileDetailMode) {
                    if (this.searchElement) {
                        this.searchElement.nativeElement.style.width = 'auto';
                    }
                    this.fixedLayoutClass = (isTableFit) ? '' : 'fixedLayout--with-panel';
                } else {
                    this.fixedLayoutClass = 'fixedLayout--no-panel';
                }
                this.layoutReady = true;
            } else if (!this.mobileDetailMode) {
                this.dimensions = [elementSearchHeight + 16];
            }

            if (this.mobileDetailMode && this.activeEntity) {
                this.fixedLayoutClass = 'fixedLayout--no-panel';
                this.layoutReady = true;
            }
        }
    }

    setSearchWidth(width) {
        if (this.searchElement) {
            this.searchElement.nativeElement.style.width = `${width}px`;
        }
    }

    setTableDimensions() {
        const windowSize = this.scrollMechanicsService.windowSizeSubject.getValue();

        const ELEMENTS_HEIGHT = this.dimensions.reduce((prev, curr) => prev + curr, 0);
        const THEAD_HEIGHT = this.tableHeaderElement ? this.tableHeaderElement.nativeElement.offsetHeight : 0;
        const PADDING = 16;
        const PAGINATION_HEIGHT = 64;

        let availSpace = windowSize.height - 4 * PADDING - ELEMENTS_HEIGHT - THEAD_HEIGHT - 48 - PAGINATION_HEIGHT;

        if (this.tableTitleElement) {
            availSpace -= this.tableTitleElement.nativeElement.offsetHeight;
        }

        let pageSize = Math.ceil(availSpace / NxHealthLayoutService.ROW_HEIGHT);
        if (pageSize < 5) {
            pageSize = 5;
        }
        this.pageSize = pageSize;
        if (this.tableElement && this.tableElement.nativeElement.offsetWidth !== 0) {
            this.tableWidth = this.tableElement.nativeElement.offsetWidth;
        }
        this.healthService.tableReady = true;
    }
}
