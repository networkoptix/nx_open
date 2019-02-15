import {
    Component, OnInit, Input, ElementRef,
    forwardRef, Renderer2, AfterViewInit, ViewEncapsulation
}                                                               from '@angular/core';
import { fromEvent }                                            from 'rxjs';
import { NG_VALUE_ACCESSOR, ControlValueAccessor } from '@angular/forms';

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

export class NxSearchComponent implements OnInit, AfterViewInit, ControlValueAccessor {
    @Input() expandable: any;

    private localFilter: any = {};
    private debounceTime = 500;
    private numberFilters = 0;
    public showAdvancedOptions = false;

    constructor(private _elementRef: ElementRef, private _renderer: Renderer2) {
    }

    ngOnInit() {
        this.expandable = (this.expandable !== undefined);  // optional param
    }

    ngAfterViewInit() {
        fromEvent(this._elementRef.nativeElement, 'keyup')
                .map((evt: any) => evt.target.value)
                .debounceTime(this.debounceTime)
                .distinctUntilChanged()
                .subscribe((event) => {
                    this.localFilter.query = event;
                    this.onChangeCallback(this.localFilter);
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

    selectBooleanFilter() {
        this.onChangeCallback(this.localFilter);
        this.numberOfOptionsSelected();
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
        this.onChangeCallback(this.localFilter);
    }

    modelChanged() {
        setTimeout(() => {
            this.onChangeCallback(this.localFilter);
            this.numberOfOptionsSelected();
        });
    }

}
