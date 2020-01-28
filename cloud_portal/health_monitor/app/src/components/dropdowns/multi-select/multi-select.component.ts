import {
    Component, OnInit, ViewEncapsulation,
    Input, forwardRef, OnChanges, SimpleChanges,
    ViewChild, ElementRef, Output, EventEmitter
}                                                  from '@angular/core';
import { ControlValueAccessor, NG_VALUE_ACCESSOR } from '@angular/forms';
import { NxLanguageProviderService }               from '../../../services/nx-language-provider';

const noop = () => {
};

/* Usage
 <nx-multi-select
     name="permissions"
     canSelectAll?
     canSearch?
     description="Roles"
     [items]="[{label: 'a', id: 1}, {label: 'b', id:3}]"
     [ngModel]="[1, 3]"       <- selected items id's
     (ngModelChanged)="onChange(result)">
 </nx-multi-select>
 */

@Component({
    selector     : 'nx-multi-select',
    templateUrl  : 'multi-select.component.html',
    styleUrls    : ['multi-select.component.scss'],
    encapsulation: ViewEncapsulation.None,
    providers    : [
        {
            provide    : NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxMultiSelectDropdown),
            multi      : true
        }
    ]
})

export class NxMultiSelectDropdown implements OnInit, ControlValueAccessor, OnChanges {
    @Input('items') itemsOrig: any;
    @Input() canSelectAll: any;
    @Input() canSearch: any;

    public items: any = {};
    public filter: string;
    public show: boolean;
    public textSelected: any = {};

    LANG: any = {};

    private innerValue: any;

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback: () => void = noop;
    private onChangeCallback: (_: any) => void = noop;

    constructor(private language: NxLanguageProviderService) {
        this.LANG = this.language.getTranslations();
        this.show = false;
        this.filter = '';
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
        this.canSelectAll = (this.canSelectAll !== undefined);
        this.canSearch = (this.canSearch !== undefined);
    }

    clearSelected() {
        this.items.forEach((item) => {
            item.selected = false;
            const index = this.innerValue.indexOf(item.id);
            if (index > -1) {
                this.innerValue.splice(index, 1);
            }
        });

        // ensure 'change' will be triggered as checkboxes didn't fire click event
        this.items = this.items.map(obj => ({ ...obj }));
        this.updateModel();

        // break anchor nav event
        return false;
    }

    change(evt, item) {
        const index = this.innerValue.indexOf(item.id);
        if (index > -1) {
            this.innerValue.splice(index, 1);
        } else {
            this.innerValue.push(item.id);
        }

        item.selected = (this.innerValue.indexOf(item.id) > -1);
        this.updateModel();

        // break anchor nav event
        return false;
    }

    applyLocalFilter(value) {
        this.filter = value;

        this.items = this.itemsOrig.filter((item) => {
            return item.id.toLowerCase().includes(value.toLowerCase());
        });
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.id;
    }

    updateItems() {
        this.items.forEach((item) => {
            item.selected = (this.innerValue !== undefined) ? (this.innerValue.indexOf(item.id) > -1) : false;
        });

        // ensure 'change' will be triggered
        this.items = this.items.map(obj => ({ ...obj }));
    }

    updateLabel() {
        switch (this.innerValue && this.innerValue.length) {
            case 1: {
                this.textSelected = this.items.find(item => {
                    return (item.label.name || item.id) === this.innerValue[0];
                });
                // Aggregated MSelect items vs. simple list
                this.textSelected = this.textSelected.label.name || this.textSelected.label;
                break;
            }
            case 0:
            case this.items.length: {
                this.textSelected = this.LANG.search.Any;
                break;
            }
            default: {
                this.textSelected = this.innerValue.length + ' ' + this.LANG.search.selected;
                break;
            }
        }
    }

    updateModel() {
        // update the form
        this.updateLabel();
        this.onChangeCallback(this.innerValue);
    }

    ngOnChanges(changes: SimpleChanges): void {
        if (changes.itemsOrig) {
            this.items = changes.itemsOrig.currentValue.map(obj => ({ ...obj }));
            this.updateItems();
        }
    }

    /**
     * Write a new (model) value to the element.
     */
    writeValue(value: any) {
        if (value !== null) {
            this.innerValue = value;
            this.updateLabel();
            this.updateItems();
        }
    }

    /**
     * Set the function to be called
     * when the control receives a change event.
     */
    registerOnChange(fn) {
        this.onChangeCallback = fn;
    }

    /**
     * Set the function to be called
     * when the control receives a touch event.
     */
    registerOnTouched(fn: any): void {
        this.onTouchedCallback = fn;
    }
}

