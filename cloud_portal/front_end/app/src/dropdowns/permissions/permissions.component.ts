import {
    Component, OnInit, Inject, ViewEncapsulation,
    Input, Output, EventEmitter, SimpleChanges
}                           from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

@Component({
    selector     : 'nx-permissions-select',
    templateUrl  : 'permissions.component.html',
    styleUrls    : [ 'permissions.component.scss' ],
    encapsulation: ViewEncapsulation.None,
})

export class NxPermissionsDropdown implements OnInit {
    @Input() roles: any;
    @Input() selected: any;
    @Output() onSelected = new EventEmitter<string>();

    selection: string;
    message: string;
    show: boolean;

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private translate: TranslateService) {

        this.show = false;

        translate.get('Please select...')
            .subscribe((res: string) => {
                this.message = res;
            });
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.roles) {
            this.roles = changes.roles.currentValue;
            const role = this.roles.filter(x => x.name === this.selected.name)[ 0 ];
            this.selection = (role) ? role.optionLabel || this.message : '';
        }
    }

    changePermission(role) {
        this.selection = role.optionLabel;
        this.onSelected.emit(role.name);
        return false; // return false so event will not bubble to HREF
    }
}
