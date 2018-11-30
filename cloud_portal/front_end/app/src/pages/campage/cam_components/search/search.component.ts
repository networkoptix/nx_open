import { Component, OnInit, Input, ElementRef, forwardRef, Renderer2, AfterViewInit, ViewChild } from '@angular/core';
import { fromEvent } from 'rxjs';
import { NG_VALUE_ACCESSOR, ControlValueAccessor } from '@angular/forms';

export const CUSTOM_INPUT_CONTROL_VALUE_ACCESSOR: any = {
    provide: NG_VALUE_ACCESSOR,
    useExisting: forwardRef(() => SearchComponent),
    multi: true
};

@Component({
  selector: 'nx-search',
  templateUrl: './search.component.html',
  providers: [CUSTOM_INPUT_CONTROL_VALUE_ACCESSOR],
    styleUrls: ['./search.component.scss']
})

export class SearchComponent implements OnInit, AfterViewInit, ControlValueAccessor {
    @Input() multiselectOptions: any = {};
    private filter: any = {};
    private emptyfilters: any = {};
    private debounceTime = 500;
    private numberFilters = 0;
    public showAdvancedOptions = false;

    constructor(private _elementRef: ElementRef, private _renderer: Renderer2) {
        this.emptyfilters = {
            query: '',
            isPtzSupported: false,
            isAptzSupported: false,
            isAudioSupported: false,
            isTwAudioSupported: false,
            isIoSupported: false,
            isMdSupported: false,
            isFisheye: false,
            isH265: false,
            isMultiSensor: false
        };
    }

    ngOnInit() {
    }

    ngAfterViewInit() {
        fromEvent(this._elementRef.nativeElement, 'keyup')
            .map((evt: any) => evt.target.value)
            .debounceTime(this.debounceTime)
            .distinctUntilChanged()
              .subscribe((event) => {
                this.filter.query = event;
                this.onChangeCallback(this.filter);
              });


      }

      selectBooleanFilter() {
        this.onChangeCallback(this.filter);
        this.numberOfOptionsSelected();
      }

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback = () => {};
    private onChangeCallback = (_: any) => {};

    // Set touched on blur
    onBlur() {
        this.onTouchedCallback();
    }

    isBlank (x) {
      return !x;
    }

    writeValue(value: any): void {
        if (value) {
            this.filter = value;
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
    resetFilters() {
        // deep copy
        this.filter = {...this.emptyfilters, minResolution: {value: '0', name: 'All'}, hardwareTypes: [], vendors: []};
        this.onChangeCallback(this.filter);
        return false;
    }

    resetQuery() {
        this.filter.query = '';
        this.onChangeCallback(this.filter);
    }

    numberOfOptionsSelected() {
        this.numberFilters = Object.keys(this.filter).length <= 1 ? 0 : (Number(this.filter.minResolution.value != 0) +
              Number(this.filter.vendors.length != 0) +
              Number(this.filter.hardwareTypes.length != 0) +

              Number(this.filter.isPtzSupported) +
              Number(this.filter.isAptzSupported) +
              Number(this.filter.isAudioSupported) +
              Number(this.filter.isTwAudioSupported) +
              Number(this.filter.isIoSupported) +
              Number(this.filter.isMdSupported) +
              Number(this.filter.isFisheye) +
              Number(this.filter.isH265));

    }

    modelChanged(model, result: any) {
        // ensure 'change' will be triggered
        switch (model) {
            case 'vendors' : {
                this.filter.vendors = [...result];
                break;
            }
            case 'hardwareTypes' : {
                this.filter.hardwareTypes = [...result];
                break;
            }
            case 'resolutions' : {
                this.filter.minResolution = result;
                break;
            }
            default: {
                // nothing to update;
                break;
            }
        }
        this.numberOfOptionsSelected();
        this.onChangeCallback(this.filter);
    }

}
