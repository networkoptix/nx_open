import {
    Component, OnInit, ViewEncapsulation,
    Input, forwardRef, OnChanges, SimpleChanges, ViewChild, ElementRef, Output, EventEmitter
} from '@angular/core';
import { TranslateService }                        from '@ngx-translate/core';
import { ControlValueAccessor, NG_VALUE_ACCESSOR } from '@angular/forms';

const noop = () => {
};

/* Usage
 <nx-multi-select name="permissions"
     canSelectAll?
     description="Roles"
     [items]="[{label: 'a', id: 1}, {label: 'b', id:3}]"
     [ngModel]="[1, 3]"       <- selected items id's
     (ngModelChanged)="onChange(result)">
 </nx-select>
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

    private items: any;
    private show: boolean;
    private numSelected: string;
    private innerValue: any;

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback: () => void = noop;
    private onChangeCallback: (_: any) => void = noop;

    constructor(private translate: TranslateService) {
        this.show = false;
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
        this.canSelectAll = (this.canSelectAll !== undefined);
    }

    selectAll() {
        this.items.forEach((item) => {
            item.selected = true;
            const index = this.innerValue.indexOf(item.id);
            if (index === -1) {
                this.innerValue.push(item.id);
            }
        });

        // ensure 'change' will be triggered as checkboxes didn't fire click event
        this.items = this.items.map(obj => ({ ...obj }));
        this.updateModel();

        // break anchor nav event
        return false;
    }

    change(evt, item) {
        if (!evt.target.checked) {
            const index = this.innerValue.indexOf(item.id);
            if (index > -1) {
                this.innerValue.splice(index, 1);
            }
        } else {
            this.innerValue.push(item.id);
        }

        this.updateModel();

        // break anchor nav event
        return false;
    }

    updateItems() {
        this.items.forEach((item) => {
            item.selected = (this.innerValue !== undefined) ? (this.innerValue.indexOf(item.id) > -1) : false;
        });

        // ensure 'change' will be triggered
        this.items = this.items.map(obj => ({ ...obj }));
    }

    updateLabel() {
        switch (this.innerValue.length) {
            case 0: {
                this.numSelected = 'None selected';
                break;
            }
            case this.items.length: {
                this.numSelected = 'All selected';
                break;
            }
            default: {
                this.numSelected = this.innerValue.length + ' Selected';
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

