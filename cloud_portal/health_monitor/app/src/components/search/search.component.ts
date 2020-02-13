import {
    Component, OnInit, Input,
    forwardRef, ViewEncapsulation, OnDestroy
}                                                  from '@angular/core';
import { NG_VALUE_ACCESSOR, ControlValueAccessor } from '@angular/forms';
import { ActivatedRoute, Router }         from '@angular/router';
import { Location }                       from '@angular/common';
import { Subject }                        from 'rxjs/Subject';
import { isArray }                        from 'rxjs/internal-compatibility';
import { NxConfigService }                from '../../services/nx-config';
import { NxUriService }                   from '../../services/uri.service';
import { NxLanguageProviderService }      from '../../services/nx-language-provider';
import { Subscription, SubscriptionLike } from 'rxjs';
import { NxScrollMechanicsService }       from '../../services/scroll-mechanics.service';
import { AutoUnsubscribe }                from 'ngx-auto-unsubscribe';
import { debounceTime }                   from 'rxjs/operators';
import { NxUtilsService }                 from '../../services/utils.service';
/* Usage
 <nx-search
     name="NAME"
     [(ngModel)]="filterModel"
     (ngModelChange)="modelChanged($event)"
     layout?="[compact | full]"     <- DEFAULT to "full"
     [placeholder]?="placeholder"   <- DEFAULT to "Search"
     ngDefaultControl?>
 </nx-search>

 * "Selectors" layout (used in Health Monitor page)
 - will hide search box and toggle button
 - will show advanced search and selected filters buttons (tags)

 * "Compact" layout (used in Integration page)
    - will hide labels and adjust spacing
    - will not show advanced search and filters selected buttons

 * "Full" layout (used in IPVD page)
    - will show labels and adjust spacing
    - will show advanced search and selected filters buttons (tags)

 */

interface Params {
    [key: string]: any;
}

@AutoUnsubscribe()
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

export class NxSearchComponent implements OnInit, OnDestroy, ControlValueAccessor {
    @Input() layout: any;
    @Input() layoutMod: any;    // mod for 'selectors' layout (HM is using 100% width width Bootstrap) ... at some point we should unify this BS
    @Input() placeholder: any;
    @Input() dataLoaded: boolean;

    public numberFilters = 0;
    public filterSelected: any;
    public localFilter: any = {};

    CONFIG: any;
    LANG: any = {};

    private searchSubscription: Subscription;
    private locationSubscription: SubscriptionLike;
    private modelSubscription: SubscriptionLike;

    private params: any = {};
    private showAdvancedOptions: boolean;

    public advSearch = false;

    private searchUpdated: any = Subject;
    private modelUpdated: any = Subject;

    constructor(
            private _router: Router,
            private _route: ActivatedRoute,
            private location: Location,
            private uri: NxUriService,
            private configService: NxConfigService,
            private language: NxLanguageProviderService,
            private scrollMechanicsService: NxScrollMechanicsService,
    ) {

        this.CONFIG = this.configService.getConfig();
        this.LANG = this.language.getTranslations();

        this.locationSubscription = this.location.subscribe((event: PopStateEvent) => {
            // force search component update
            setTimeout(() => {
                this.updateFilter(this.uri.getURI());
                this.modelChanged(false);
            });
        });

        this.searchUpdated = new Subject<any>();
        this.modelUpdated = new Subject<any>();
    }

    ngOnInit() {
        this.dataLoaded = this.dataLoaded === undefined ? true : this.dataLoaded;
        this.placeholder = this.placeholder || '';  // optional param
        this.layout = (this.layout !== undefined) ? this.layout : 'full';
        this.showAdvancedOptions = !(this.layout === 'full'); // hide advanced search in "full" layout
        this.filterSelected = '';

        // Example URI
        // /ipvd?search=Axis&tags=isAptzSupported&resolution=SVGA&vendors=Axis,30X,Sony
        this._route.queryParams.subscribe((params) => {
            this.params = params;
            this.updateFilter(undefined, false);
        });

        this.searchSubscription = this.searchUpdated
            .pipe(debounceTime(this.CONFIG.search.debounceTime))
            .subscribe(data => {
                this.localFilter.query = data;
                this.modelChanged();
            });

        this.modelSubscription = this.modelUpdated
            .pipe(debounceTime(this.CONFIG.search.debounceTime))
            .subscribe(data => {
                this.modelChanged();
            });
    }

    ngOnDestroy() {}

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

    onSearchType(value: any) {
        this.searchUpdated.next(value);
    }

    onModelChange(value: any) {
        this.modelUpdated.next(value);
    }

    updateFilter(params?, resetUri?) {
        if (params && params.value) {
            this.params = params.value;
        }

        this.localFilter.query = '';

        if (this.params.search && this.params.search.length > 0) {
            this.localFilter.query = this.params.search;
        }

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
                        select.selected = select.items.find((item) => item.value === this.params[select.id]);
                    } else {
                        if (!select.selected) {
                            select.selected = { value: '0', name: 'All' };
                        }
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
        // the component relaying on model change may not be ready
        // this.modelChanged(resetUri);
    }

    writeValue(value: any): void {
        // Avoid localFilter update if filter in not initialized (page refresh)
        if (value &&
            ((value.tags && value.tags.length) ||
             (value.selects && value.selects.length) ||
             (value.multiselects && value.multiselects.length))) {

            if (JSON.stringify(this.localFilter) === JSON.stringify(value)) {
                return;
            }
            this.localFilter = NxUtilsService.deepCopy(value);
            this.advSearch = (this.localFilter.selects && this.localFilter.selects.length) ||
                    (this.localFilter.multiselects && this.localFilter.multiselects.length) ||
                    (this.localFilter.tags && this.localFilter.tags.length);

            // Update model with query params
            this.params = this._route.snapshot.queryParams;
            this.updateFilter(undefined, false);
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
        this.scrollMechanicsService.offsetSubject.next(this.showAdvancedOptions);
        return false;
    }

    numberOfOptionsSelected() {
        // No need to run this function while ngModel's writeValue initializes
        if (Object.keys(this.localFilter).length === 0) {
            return;
        }

        this.placeholder = this.placeholder || this.LANG.search.Search;  // optional param
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
                        selectsSelected = this.numberFilters + ' ' + this.LANG.search['filters applied'];
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
                        selectsSelected = this.numberFilters + '&nbsp;' + this.LANG.search['filters applied'];
                    } else {
                        selectsSelected = select.label + '&nbsp;&ndash;&nbsp;' + select.selected.name;
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

                    let label;
                    if (select.selected.length === 1) {
                        label = select.label;

                        if (select.singular) {
                            label = select.singular;
                        }

                        label += ' &ndash; ';

                        if (select.searchLabelSingular || select.searchLabelSingular === '') {
                            label = select.searchLabelSingular;
                        }

                        multiSelectsSelected = label + select.items.find(item => {
                            return (item.label.name || item.id) === select.selected[0];
                        }).label;

                    } else {
                        if (select.searchLabel || select.searchLabel === '') {
                            label = select.searchLabel;
                        } else {
                            label = select.label.toLowerCase();
                        }

                        multiSelectsSelected = select.selected.length + ' ' + label;
                    }
                }
            });
        }
        if (flag === 1) {
            this.filterSelected = tagsSelected || selectsSelected || multiSelectsSelected;
        } else {
            const str = (this.numberFilters === 1) ?
                    ' ' + this.LANG.search['filter applied'] :
                    ' ' + this.LANG.search['filters applied'];
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

        this.modelChanged(true);
    }

    resetQuery() {
        this.localFilter.query = '';
        this.modelChanged(true);
    }

    setRouteParams(resetUri?): Promise<any> {
        const queryParams: Params = {};

        let selectedTags;
        queryParams.tags = undefined;
        if (this.localFilter.tags && this.localFilter.tags.length) {
            selectedTags = this.localFilter.tags.filter((tag) => tag.value);
            if (selectedTags.length) {
                queryParams.tags = selectedTags.map((elm) => elm.id).join(',');
            }
        }

        queryParams.search = undefined;
        if (this.localFilter.query !== '') {
            queryParams.search = this.localFilter.query;
        }

        if (this.localFilter.selects && this.localFilter.selects.length) {
            this.localFilter.selects.forEach((select) => {
                queryParams[select.id] = undefined;
                if (+select.selected.value !== 0) {
                    queryParams[select.id] = select.selected.value;
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

        this.uri.pageOffset = window.pageYOffset;

        return this.uri.updateURI(this.uri.getURL(), queryParams);
    }

    modelChanged(resetUri?) {
        this.setRouteParams(resetUri)
            .then(() => {
                this.numberOfOptionsSelected();
                this.onChangeCallback(this.localFilter);
            });
    }
}
