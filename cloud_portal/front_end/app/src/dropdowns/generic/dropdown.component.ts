import { Component, OnInit, ViewEncapsulation, Input, forwardRef, EventEmitter, Output, OnChanges, SimpleChanges } from '@angular/core';
import { TranslateService }                                        from '@ngx-translate/core';
import { ControlValueAccessor, NG_VALUE_ACCESSOR }                 from '@angular/forms';

const noop = () => {
};

/* Usage
<nx-select name="permissions"
           [items]="accessRoles"
           label="optionLabel"          <- which property should be shown
           [(ngModel)]="user.role.name"
           required>
</nx-select>
*/

@Component({
    selector: 'nx-select',
    templateUrl: 'dropdown.component.html',
    styleUrls: ['dropdown.component.scss'],
    encapsulation: ViewEncapsulation.None,
    providers: [
        {
            provide: NG_VALUE_ACCESSOR,
            useExisting: forwardRef(() => NxGenericDropdown),
            multi: true
        }
    ]
})

export class NxGenericDropdown implements OnInit, ControlValueAccessor {
    // items should have at least name ex [{name: 'a', id: 1}, {name: 'b', id:3}]
    @Input() items: any;
    @Input() selected: any;
    @Output() onSelected = new EventEmitter<string>();

    // Placeholders for the callbacks which are later provided
    // by the Control Value Accessor
    private onTouchedCallback: () => void = noop;
    private onChangeCallback: (_: any) => void = noop;

    message: string;
    show: boolean;

    constructor(private translate: TranslateService) {
        this.show = false;

        translate.get('Please select...')
                .subscribe((res: string) => {
                    this.message = res;
                });
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
    }

    change(item) {
        this.selected = item;
        this.onSelected.emit(item);
        this.onChangeCallback(this.selected);
    }

    ngOnChanges(changes: SimpleChanges) {
        // detect changes in list of items and changes in selected to support clear option
        if (changes.items && changes.items.currentValue) {
            if (this.selected) {
                this.selected = changes.items.currentValue.filter(x => x.name === this.selected.name)[0];
            } else {
                this.selected = {name: this.message};
            }
        }

        if (changes.selected.currentValue) {
            this.selected = changes.selected.currentValue;
        } else {
            this.selected = {name: this.message};
        }
    }

    /**
     * Write a new (model) value to the element.
     */
    writeValue(value: any) {
        if (value !== null && typeof value !== 'undefined') {
            this.selected = value;
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

