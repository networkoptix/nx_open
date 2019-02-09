import {
    Component, OnInit, Input, ElementRef,
    forwardRef, Renderer2, AfterViewInit, ViewEncapsulation, Inject
}                                                  from '@angular/core';
import { fromEvent }                               from 'rxjs';
import { NG_VALUE_ACCESSOR, ControlValueAccessor } from '@angular/forms';
import { ActivatedRoute, Router }                  from '@angular/router';
import { isArray }                                 from 'rxjs/internal-compatibility';
import { NxConfigService }                         from '../../services/nx-config';

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

    private localFilter: any = {};
    private numberFilters = 0;
    private params: any = {};
    private config: any;

    public advSearch = false;
    public showAdvancedOptions = false;

    constructor(private _elementRef: ElementRef,
                private _router: Router,
                private _route: ActivatedRoute,
                private _renderer: Renderer2,
                configService: NxConfigService) {

        this.config = configService.getConfig();
    }

    ngOnInit() {
        this.expandable = (this.expandable !== undefined);  // optional param

        this._route
            .queryParams
            .subscribe(params => {
                this.params.search = params.search || '';

                if (params.tags) {
                    this.params.tags = params.tags.split(',');
                }

                if (params.selects) {
                    this.params.selects = isArray(params.selects) ? params.selects : [params.selects];
                }

                if (params.multiselects) {
                    this.params.multiselects = isArray(params.multiselects) ? params.multiselects : [params.multiselects];
                }
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
            if (this.params.search !== '') {
                this.localFilter.query = this.params.search;
            }

            if (this.params.tags && this.localFilter.tags.length) {
                this.params.tags.forEach((tagName) => {
                    this.localFilter.tags.find((tag) => tag.id === tagName).value = true;
                    this.showAdvancedOptions = true;
                });
            }

            if (this.params.selects && this.localFilter.selects.length) {
                this.params.selects.forEach((selectParam) => {
                    const selected = selectParam.split('|');
                    const selectedDropdown = this.localFilter
                                                 .selects
                                                 .find((select) => select.id === selected[0]);
                    const selectedItem = selectedDropdown.items
                                                         .find((item) => item.name === selected[1]);

                    selectedDropdown.selected = selectedItem;

                    this.showAdvancedOptions = true;
                });
            }

            if (this.params.multiselects && this.localFilter.multiselects.length) {
                this.params.multiselects.forEach((selectParam) => {
                    const selected = selectParam.split('|');
                    const selectedDropdown = this.localFilter
                                                 .multiselects
                                                 .find((select) => select.id === selected[0]);

                    selectedDropdown.selected = selected[1].split(',');

                    this.showAdvancedOptions = true;
                });
            }
        }
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

        this.localFilter.tags.forEach((filter) => {
            if (filter.value) {
                this.numberFilters++;
            }
        });

        this.localFilter.selects.forEach((filter) => {
            if (filter.selected && filter.selected.value !== '0') { // not default value
                this.numberFilters++;
            }
        });

        this.localFilter.multiselects.forEach((filter) => {
            if (filter.selected) { // not default value
                this.numberFilters += filter.selected.length;
            }
        });
    }

    resetFilters() {
        this.localFilter.query = '';
        this.localFilter.tags.forEach((filter) => {
            filter.value = false;
        });

        this.localFilter.selects.forEach((filter) => {
            filter.selected = filter.items[0];
        });

        this.localFilter.multiselects.forEach((filter) => {
            filter.selected = [];
        });

        this.numberFilters = 0;

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

        let selectedOptions = [];
        queryParams.selects = undefined;
        if (this.localFilter.selects && this.localFilter.selects.length) {
            selectedOptions = this.localFilter.selects.filter((select) => {
                return +select.selected.value !== 0;
            });

            if (selectedOptions.length) {
                queryParams.selects = selectedOptions.map((option) => {
                    return option.id + '|' + option.selected.name;
                });
            }
        }

        const selectedMultiOptions = [];
        queryParams.multiselects = undefined;
        if (this.localFilter.multiselects && this.localFilter.multiselects.length) {
            this.localFilter.multiselects.forEach((select) => {
                if (select.selected && select.selected.length) {
                    selectedMultiOptions.push({ id: select.id, selected : select.selected });
                }
            });

            if (selectedMultiOptions.length) {
                queryParams.multiselects = selectedMultiOptions.map((option) => {
                    return option.id + '|' + option.selected;
                });
            }
        }

        // changes the route without moving from the current view or
        // triggering a navigation event,
        this._router.navigate(['/campage'], {
            queryParams,
            relativeTo         : this._route,
            replaceUrl         : true,
            queryParamsHandling: 'merge',
            // do not trigger navigation
            // skipLocationChange : true
        });
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
