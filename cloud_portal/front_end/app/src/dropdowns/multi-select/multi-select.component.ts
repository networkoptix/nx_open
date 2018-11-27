import {
    Component, OnInit, ViewEncapsulation,
    Input, forwardRef, OnChanges, SimpleChanges, ViewChild, ElementRef
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
     [(ngModel)]="[1, 3]"       <- selected items id's
     required>
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
    // @Output() onSelected = new EventEmitter<string>();

    private items: any;
    private message: string;
    private show: boolean;
    private numSelected: string;
    private innerValue: any;

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback: () => void = noop;
    private onChangeCallback: (_: any) => void = noop;

    constructor(private translate: TranslateService) {
        this.show = false;

        translate.get('Please select...')
                 .subscribe((res: string) => {
                     this.message = res;
                 });
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

        this.updateModel();

        // break anchor nav event
        return false;
    }

    change(item) {
        if (!item.selected) {
            const index = this.innerValue.indexOf(item.id);
            if (index > -1) {
                this.innerValue.splice(index, 1);
            }
        } else {
            this.innerValue.push(item.id);
        }

        this.updateModel();
    }

    updateItems() {
        this.items.forEach((item) => {
            item.selected = (this.innerValue !== undefined) ? (this.innerValue.indexOf(item.id) > -1) : false;
        });
    }

    updateModel() {
        // update the form
        this.numSelected = this.innerValue.length || 'None';
        this.onTouchedCallback();
        this.onChangeCallback(this.innerValue);
        // this.onSelected.emit(this.innerValue);
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
            this.numSelected = this.innerValue.length || 'None';
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

