import {
    Component, OnInit, ViewEncapsulation, Input,
    forwardRef, EventEmitter, Output, OnChanges, SimpleChanges } from '@angular/core';
import { ControlValueAccessor, NG_VALUE_ACCESSOR }               from '@angular/forms';
import { NxLanguageProviderService }                             from '../../../services/nx-language-provider';

const noop = () => {
};

/* Usage
<nx-select name="permissions"
           [items]="accessRoles"
           label="optionLabel"          <- which property should be shown
           [(ngModel)]="user.role.name"
           (ngModelChange)="onModelChange($event)"
           [selected]="user.role.name ? user.role.name : null"
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

    LANG: any = {};

    message: string;
    show: boolean;

    constructor(private language: NxLanguageProviderService) {
        this.show = false;
        this.LANG = this.language.getTranslations();
        this.message = this.LANG.pleaseSelect;
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
    }

    trackItem(index, item) {
        if (!item) {
            return undefined;
        }
        return item.value;
    }

    change(item) {
        this.selected = item;
        this.onSelected.emit(item);
        this.onChangeCallback(this.selected);
    }

    ngOnChanges(changes: SimpleChanges) {
        // detect changes in list of items and changes in selected to support clear option
        if (changes.selected.currentValue) {
            this.selected = changes.selected.currentValue;
        } else if (!this.selected) {
            this.selected = { name: this.message, value: '0' };
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

