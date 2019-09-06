import {
    Component, OnInit, Input,
    forwardRef, ViewEncapsulation, KeyValueDiffer, KeyValueDiffers
} from '@angular/core';
import { NG_VALUE_ACCESSOR, ControlValueAccessor }                         from '@angular/forms';
import { ActivatedRoute, Event as NavigationEvent, NavigationEnd, Router } from '@angular/router';
import { Location }                                                        from '@angular/common';
import { NxConfigService }                                                 from '../../services/nx-config';
import { isArray }                                                         from 'rxjs/internal-compatibility';
import { NxUriService }                                                    from '../../services/uri.service';
import { TranslateService }                                                from '@ngx-translate/core';
import { filter, debounceTime, distinctUntilChanged }                      from 'rxjs/operators';
import { Subject } from 'rxjs/Subject';

/* Usage
 <nx-search
     name="NAME"
     [(ngModel)]="filterModel"
     (ngModelChange)="modelChanged($event)"
     layout?="[compact | full]"     <- DEFAULT to "full"
     [placeholder]?="placeholder"   <- DEFAULT to "Search"
     ngDefaultControl?>
 </nx-search>

 * "Compact" layout (used in Integration page)
    - will hide labels and adjust spacing
    - will not show advanced search and filters selected buttons

 * "Full" layout (used in IPVD page)
    - will show labels and adjust spacing
    - will show advanced search and selected filters buttons

 */

interface Params {
    [key: string]: any;
}

@Component({
    selector     : 'nx-search',
    templateUrl  : './search.component.html',
    encapsulation: ViewEncapsulation.None,
    providers    : [{
        provide    : NG_VALUE_ACCESSOR,
        useExisting: forwardRef(() => NxSearchComponent),
        multi      : true
    }],
    styleUrls    : ['./search.component.scss']
})

export class NxSearchComponent implements OnInit, ControlValueAccessor {
    @Input() layout: any;
    @Input() placeholder: any;
    @Input() dataLoaded: boolean;

    public numberFilters = 0;
    public filterSelected: any;
    public localFilter: any = {};
    public config: any;

    private lang: any = {};
    private params: any = {};
    private uriPath: string;
    private showAdvancedOptions: boolean;

    public advSearch = false;

    private searchUpdated: any = Subject;

    constructor(private _router: Router,
                private _route: ActivatedRoute,
                private location: Location,
                private uri: NxUriService,
                private configService: NxConfigService,
                private translate: TranslateService,
                private differs: KeyValueDiffers) {

        this.config = configService.getConfig();
        this.uriPath = '/' + this._route.snapshot.url[0].path;

        this.location.subscribe((event: PopStateEvent) => {
            // force search component update
            setTimeout(() => this.updateFilter(this.uri.getURI()));
        });

        this.searchUpdated = new Subject();
    }

    ngOnInit() {
        this.dataLoaded = this.dataLoaded === undefined ? true : this.dataLoaded;
        this.placeholder = this.placeholder || '';  // optional param
        this.layout = (this.layout !== undefined) ? this.layout : 'full';
        this.showAdvancedOptions = !(this.layout === 'full'); // hide advanced search in "full" layout
        this.filterSelected = '';

        // Example URI
        // /ipvd?search=Axis&tags=isAptzSupported&resolution=SVGA&vendors=Axis,30X,Sony
        this._route
            .queryParams
            .subscribe(params => {
                this.params = { ...params };
                this.params.search = params.search || '';
                this.params.selects = [];
                this.params.multiselects = [];
            });

        this.searchUpdated.asObservable()
            .debounceTime(this.config.search.debounceTime)
            .distinctUntilChanged()
            .subscribe(data => {
                this.localFilter.query = data;
                this.modelChanged();
            });
    }

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback = () => {
    };
    private onChangeCallback = (_: any) => {
    };

    // Set touched on blur
    onBlur() {
        this.onTouchedCallback();
    }

    onSearchType(value: string) {
        this.searchUpdated.next(value);
    }

    updateFilter(params?) {
        if (params && params.value) {
            this.params = params.value;
        }

        this.localFilter.query = this.params.search || '';

        if (this.localFilter.tags && this.localFilter.tags.length) {
            this.localFilter.tags.forEach(tag => tag.value = false);
            if (this.params.tags) {
                this.params.tags
                    .split(',')
                    .forEach((tagName) => {
                        this.localFilter.tags.find((tag) => {
                            if (tag.id === tagName) {
                                tag.value = true;
                            }
                        });
                    });
            }
        }

        if (this.localFilter.selects && this.localFilter.selects.length) {
            this.localFilter
                .selects
                .find((select) => {
                    if (this.params[select.id]) {
                        select.selected = select.items.find((item) => item.name === this.params[select.id]);
                    } else {
                        select.selected = { value: '0', name: 'All' };
                    }
                });
        }

        if (this.localFilter.multiselects && this.localFilter.multiselects.length) {
            this.localFilter
                .multiselects
                .find((select) => {
                    if (this.params[select.id]) {
                        select.selected = isArray(this.params[select.id]) ? this.params[select.id] : this.params[select.id].split(',');
                    } else {
                        select.selected = [];
                    }
                });
        }

        this.numberOfOptionsSelected();
        this.onChangeCallback(this.localFilter);

    }

    writeValue(value: any): void {
        // Avoid localFilter update if filter in not initialized (page refresh)
        if (value &&
            ((value.tags && value.tags.length) ||
             (value.selects && value.selects.length) ||
             (value.multiselects && value.multiselects.length))) {

            this.localFilter = value;
            this.advSearch = (this.localFilter.selects && this.localFilter.selects.length) ||
                    (this.localFilter.multiselects && this.localFilter.multiselects.length) ||
                    (this.localFilter.tags && this.localFilter.tags.length);

            // Update model with query params
            this.updateFilter();
        }
    }

    // From ControlValueAccessor interface
    registerOnChange(fn: any) {
        this.onChangeCallback = fn;
    }

    // From ControlValueAccessor interface
    registerOnTouched(fn: any) {
        this.onTouchedCallback = fn;
    }

    toggleOptions() {
        this.showAdvancedOptions = !this.showAdvancedOptions;
        return false;
    }

    numberOfOptionsSelected() {
        this.lang = this.translate.translations[this.translate.currentLang];

        // No need to run this function while ngModel's writeValue initializes
        if (Object.keys(this.localFilter).length === 0 || !this.lang) {
            return;
        }

        this.placeholder = this.placeholder || this.lang.search.Search;  // optional param
        this.numberFilters = 0;
        this.filterSelected = '';

        let flag = 0;
        let tagsSelected = '';
        let selectsSelected = '';
        let multiSelectsSelected = '';

        if (this.localFilter.tags) {
            this.localFilter.tags.forEach((filter) => {
                if (filter.value) {
                    this.numberFilters++;
                    if (this.numberFilters > 1) {
                        selectsSelected = this.numberFilters + ' ' + this.lang.search['filters applied'];
                    } else {
                        tagsSelected = filter.label;
                    }
                    flag++;
                }
            });
        }

        if (this.localFilter.selects && this.localFilter.selects.length) {
            this.localFilter.selects.forEach((select) => {
                if (select.selected && select.selected.value !== '0') { // not default value
                    this.numberFilters++;
                    if (this.numberFilters > 1) {
                        selectsSelected = this.numberFilters + ' ' + this.lang.search['filters applied'];
                    } else {
                        selectsSelected = select.label + ' &ndash; ' + select.selected.name;
                    }
                    flag++;
                }
            });
        }

        if (this.localFilter.multiselects && this.localFilter.multiselects.length) {
            this.localFilter.multiselects.forEach((select) => {

                this.numberFilters += select.selected.length;

                if (select.selected.length > 0) {
                    flag++;

                    if (select.selected.length === 1) {
                        const label = select.singular || select.label;
                        multiSelectsSelected = label + ' &ndash; ' + select.items.find(item => {
                            return (item.label.name || item.id) === select.selected[0];
                        }).label;
                    } else {
                        multiSelectsSelected = select.selected.length + ' ' + select.label.toLowerCase();
                    }
                }
            });
        }

        if (flag === 1) {
            this.filterSelected = tagsSelected || selectsSelected || multiSelectsSelected;
        } else {
            const str = (this.numberFilters === 1) ?
                    ' ' + this.lang.search['filter applied'] :
                    ' ' + this.lang.search['filters applied'];

            this.filterSelected = this.numberFilters + str;
        }
    }

    clearFilters() {
        if (this.localFilter.tags) {
            this.localFilter.tags.forEach((filter) => {
                filter.value = false;
            });
        }

        if (this.localFilter.selects) {
            this.localFilter.selects.forEach((filter) => {
                filter.selected = filter.items[0];
            });
        }

        if (this.localFilter.multiselects) {
            this.localFilter.multiselects.forEach((filter) => {
                filter.selected = [];
            });
        }
    }

    resetFilters() {
        this.clearFilters();
        this.numberFilters = 0;
        this.filterSelected = '';
        this.setRouteParams();
        this.onChangeCallback(this.localFilter);

        return false;
    }

    resetQuery() {
        this.localFilter.query = '';
        this.setRouteParams();
        this.onChangeCallback(this.localFilter);
        this.numberOfOptionsSelected();
    }

    setRouteParams(forControl?): boolean {
        const queryParams: Params = {};

        switch (forControl) {
            case 'tags':
                let selectedTags;
                queryParams.tags = undefined;
                if (this.localFilter.tags && this.localFilter.tags.length) {
                    selectedTags = this.localFilter.tags.filter((tag) => tag.value);
                    if (selectedTags.length) {
                        queryParams.tags = selectedTags.map((elm) => elm.id).join(',');
                    }
                }
                if (queryParams.tags === this.params.tags) {
                    return false;
                }

                break;

            default:
                queryParams.search = undefined;
                if (this.localFilter.query !== '') {
                    queryParams.search = this.localFilter.query;
                }

                if (this.localFilter.selects && this.localFilter.selects.length) {
                    this.localFilter.selects.forEach((select) => {
                        queryParams[select.id] = undefined;
                        if (+select.selected.value !== 0) {
                            queryParams[select.id] = select.selected.name;
                        }
                    });
                }

                if (this.localFilter.multiselects && this.localFilter.multiselects.length) {
                    this.localFilter.multiselects.forEach((select) => {
                        queryParams[select.id] = undefined;
                        if (select.selected && select.selected.length) {
                            queryParams[select.id] = select.selected.join(',');
                        }
                    });
                }
        }

        this.uri.pageOffset = window.pageYOffset;
        this.uri.updateURI(this.uriPath, queryParams);
        return true;
    }

    modelChanged(byControl?) {
        if (this.setRouteParams(byControl)) {
            this.onChangeCallback(this.localFilter);
            this.numberOfOptionsSelected();
        }
    }
}
