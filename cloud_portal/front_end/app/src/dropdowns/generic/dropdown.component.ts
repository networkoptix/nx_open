import { Component, OnInit, ViewEncapsulation, Input, forwardRef } from '@angular/core';
import { TranslateService }                                        from '@ngx-translate/core';
import { ControlValueAccessor, NG_VALUE_ACCESSOR }                 from '@angular/forms';


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
    @Input() items: any;
    @Input() label: string;
    @Input() selected: any;

    selection: string;
    message: string;

    constructor(private translate: TranslateService) {
        translate.get('Please select...')
                 .subscribe((res: string) => {
                     this.message = res;
                 });
    }

    propagateChange = (_: any) => {
    };

    ngOnInit(): void {
    }

    change(item) {
        this.selection = item[this.label];
        this.propagateChange(this.selection);
    }

    writeValue(value: any) {
        this.selection = (value) ? value : this.message;
    }

    registerOnChange(fn) {
        this.propagateChange = fn;
    }

    registerOnTouched() {
    }
}
