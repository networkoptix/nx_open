import {
    Component, OnInit, Input, ElementRef,
    forwardRef, Renderer2, ViewEncapsulation,
    SimpleChanges, OnChanges
}                                                  from '@angular/core';
import { NG_VALUE_ACCESSOR, ControlValueAccessor } from '@angular/forms';
import { ActivatedRoute, Router }                  from '@angular/router';
import { NxConfigService }                         from '../../services/nx-config';
import { isArray }                                 from 'rxjs/internal-compatibility';
import { NxUriService }                            from '../../services/uri.service';

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
    @Input() expandable: any;
    @Input() skinny: any;


    private localFilter: any = {};
    private numberFilters = 0;
    private params: any = {};
    private config: any;
    private uriPath: string;
    private showAdvancedOptions: boolean;

    public advSearch = false;

    constructor(private _elementRef: ElementRef,
                private _router: Router,
                private _route: ActivatedRoute,
                private _renderer: Renderer2,
                private uri: NxUriService,
                configService: NxConfigService) {

        this.config = configService.getConfig();
        this.uriPath = '/' + this._route.snapshot.url[0].path;


    }

    ngOnInit() {
        this.skinny = (this.skinny !== undefined);  // optional param
        this.expandable = (this.expandable !== undefined);  // optional param
        this.showAdvancedOptions = !this.expandable;

        // Example URI
        // /campage?search=Axis&tags=isAptzSupported&resolution=SVGA&vendors=Axis,30X,Sony
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

    isBlank(x) {
        return !x;
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
                                        this.showAdvancedOptions = true;
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
                                const selectedItem = select.items.find((item) => item.name === this.params[key]);
                                select.selected = selectedItem;
                                this.showAdvancedOptions = true;
                            }
                        });
                }

                if (this.localFilter.multiselects && this.localFilter.multiselects.length) {
                    this.localFilter
                        .multiselects
                        .find((select) => {
                            if (select.id === key) {
                                select.selected = isArray(this.params[key]) ? this.params[key] : this.params[key].split(',');

                                this.showAdvancedOptions = true;
                            }
                        });
                }
            });
        }

        this.numberOfOptionsSelected();
        const normalizedValue = this.isBlank(value) ? '' : value;
        this._renderer.setProperty(this._elementRef.nativeElement, 'value', normalizedValue);
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
        this.numberFilters = 0;

        if (this.localFilter.tags) {
            this.localFilter.tags.forEach((filter) => {
                if (filter.value) {
                    this.numberFilters++;
                }
            });
        }

        if (this.localFilter.selects) {
            this.localFilter.selects.forEach((filter) => {
                if (filter.selected && filter.selected.value !== '0') { // not default value
                    this.numberFilters++;
                }
            });
        }

        if (this.localFilter.multiselects) {
            this.localFilter.multiselects.forEach((filter) => {
                if (filter.selected) { // not default value
                    this.numberFilters += filter.selected.length;
                }
            });
        }
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
        this.onChangeCallback(this.localFilter);
        this.numberOfOptionsSelected();
    }

    modelChanged() {
        this.setRouteParams();
        this.onChangeCallback(this.localFilter);
        this.numberOfOptionsSelected();
    }
}
