import { Component, OnInit, ViewEncapsulation, Input, forwardRef, EventEmitter, Output } from '@angular/core';
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

export class NxGenericDropdown implements OnInit {
    // items should have at least name ex [{name: 'a', id: 1}, {name: 'b', id:3}]
    @Input() items: any;
    @Input() selected: any;
    @Output() onSelected = new EventEmitter<string>();

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
        const selected = this.items.filter(x => x.name === this.selected.name)[0];
        if (!this.selected) {
            this.selected = selected || {name: this.message};
        }
    }

    change(item) {
        this.selected = item;
        this.onSelected.emit(item);
    }
}

