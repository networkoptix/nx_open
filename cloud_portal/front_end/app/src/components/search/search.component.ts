import {
    Component, OnInit, Input,
    forwardRef, ViewEncapsulation
} from '@angular/core';
import { NG_VALUE_ACCESSOR, ControlValueAccessor } from '@angular/forms';
import { ActivatedRoute, Router }                  from '@angular/router';
import { NxConfigService }                         from '../../services/nx-config';
import { isArray }                                 from 'rxjs/internal-compatibility';
import { NxUriService }                            from '../../services/uri.service';
import { TranslateService }                        from '@ngx-translate/core';


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

    constructor(private _router: Router,
                private _route: ActivatedRoute,
                private uri: NxUriService,
                private configService: NxConfigService,
                private translate: TranslateService) {

        this.config = configService.getConfig();
        this.uriPath = '/' + this._route.snapshot.url[0].path;
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

    writeValue(value: any): void {
        if (value) {
            this.localFilter = value;
            this.advSearch = (this.localFilter.selects && this.localFilter.selects.length) ||
                    (this.localFilter.multiselects && this.localFilter.multiselects.length) ||
                    (this.localFilter.tags && this.localFilter.tags.length);

            // Update model with query params
            Object.keys(this.params).forEach((key) => {
                if (key === 'search' && this.params.search !== '') {
                    this.localFilter.query = this.params.search;
                    return;
                }

                if (key === 'tags' && this.localFilter.tags.length) {
                    this.params[key]
                            .split(',')
                            .forEach((tagName) => {
                                this.localFilter.tags.find((tag) => {
                                    if (tag.id === tagName) {
                                        tag.value = true;
                                    }
                                });
                            });
                    return;
                }

                if (this.localFilter.selects && this.localFilter.selects.length) {
                    this.localFilter
                        .selects
                        .find((select) => {
                            if (select.id === key) {
                                select.selected = select.items.find((item) => item.name === this.params[key]);
                            }
                        });
                }

                if (this.localFilter.multiselects && this.localFilter.multiselects.length) {
                    this.localFilter
                        .multiselects
                        .find((select) => {
                            if (select.id === key) {
                                select.selected = isArray(this.params[key]) ? this.params[key] : this.params[key].split(',');
                            }
                        });
                }
            });
        }

        this.numberOfOptionsSelected();
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
        let tagsSelected;
        let selectsSelected;
        let multiSelectsSelected;

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
                        selectsSelected = select.label + ' - ' + select.selected.name;
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
                        multiSelectsSelected = label + ' - ' + select.items.find(item => {
                            return (item.label.name || item.id) === select.selected[0];
                        }).label;
                    } else {
                        multiSelectsSelected = select.selected.length + ' ' + select.label.toLowerCase();
                    }
                }
            });
        }

        if (this.localFilter.query !== '') {
            this.numberFilters++;
        }

        if (flag === 1 && this.localFilter.query === '') {
            this.filterSelected = tagsSelected || selectsSelected || multiSelectsSelected;
            return;
        }

        const str = (this.numberFilters === 1) ?
                ' ' + this.lang.search['filter applied'] :
                ' ' + this.lang.search['filters applied'];

        this.filterSelected = this.numberFilters + str;
    }

    resetFilters() {
        this.localFilter.query = '';

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

        this.numberFilters = 0;
        this.filterSelected = '';
        this.uri.resetURI(this.uriPath);
        this.onChangeCallback({...this.localFilter});

        return false;
    }

    resetQuery() {
        this.localFilter.query = '';
        this.setRouteParams();
        this.onChangeCallback({ ...this.localFilter });
        this.numberOfOptionsSelected();
    }

    setRouteParams() {
        interface Params {
            [key: string]: any;
        }

        const queryParams: Params = {};
        queryParams.search = undefined;
        if (this.localFilter.query !== '') {
            queryParams.search = this.localFilter.query;
        }

        let selectedTags;
        queryParams.tags = undefined;
        if (this.localFilter.tags && this.localFilter.tags.length) {
            selectedTags = this.localFilter.tags.filter((tag) => tag.value);
            if (selectedTags.length) {
                queryParams.tags = selectedTags.map((elm) => elm.id).join(',');
            }
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

        this.uri.updateURI(this.uriPath, queryParams, true);
    }

    selectBooleanFilter() {
        this.setRouteParams();
        this.onChangeCallback({ ...this.localFilter });
        this.numberOfOptionsSelected();
    }

    modelChanged() {
        this.setRouteParams();
        this.onChangeCallback({ ...this.localFilter });
        this.numberOfOptionsSelected();
    }
}
