import {
    Component, OnInit, ViewEncapsulation,
    Input, Output, EventEmitter, SimpleChanges
}                                    from '@angular/core';
import { NxLanguageProviderService } from '../../../services/nx-language-provider';

@Component({
    selector     : 'nx-permissions-select',
    templateUrl  : 'permissions.component.html',
    styleUrls    : [ 'permissions.component.scss' ],
    encapsulation: ViewEncapsulation.None,
})

export class NxPermissionsDropdown implements OnInit {
    @Input() disabled: any;
    @Input() user: any;
    @Input() roles: any;
    @Input() system: any;
    @Input() selected: any;
    @Output() onSelected = new EventEmitter<string>();

    LANG: any;

    selection: string;
    message: string;
    show: boolean;
    accessRoles: any;
    differ: any;

    constructor(private language: NxLanguageProviderService,
    ) {
        this.LANG = this.language.getTranslations();
        this.accessRoles = [];
        this.show = false;
        this.message = this.LANG.pleaseSelect;
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
        this.processAccessRoles();
        const role = this.accessRoles.filter(x => x.name === this.selected.name)[ 0 ];
        this.selection = '';

        if (role) {
            this.selection = role.optionLabel || this.message;
            this.changePermission(role);
        }
    }

    processAccessRoles() {
        if (this.roles) {
            this.accessRoles = this.roles.filter((role) => {
                if (!(role.isOwner || role.isAdmin && !this.system.isMine)) {
                    role.optionLabel = this.LANG.accessRoles[ role.name ] ?
                            this.LANG.accessRoles[role.name].label :
                            role.name;

                    return role;
                }

                return false;
            });
        }
    }

    // Keeping this just for reference ... ngDoCheck runs often so it's unnecessary overhead
    // ngDoCheck() {
    //     const changes = this.differ.diff(this.system);
    //
    //     if (changes) {
    //         this.processAccessRoles();
    //         const role = this.roles.filter(x => x.name === this.selected.name)[ 0 ];
    //         this.selection = (role) ? role.optionLabel || this.message : '';
    //     }
    // }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.roles && changes.roles.currentValue && !this.system.pauseUpdate) {
            this.processAccessRoles();
            const role = this.accessRoles.filter(x => x.name === this.selected.name)[ 0 ];
            this.selection = '';

            if (role) {
                this.selection = role.optionLabel || this.message;
                this.changePermission(role);
            }
        }

        if (changes.selected && changes.selected.currentValue && !this.system.pauseUpdate) {
            this.selection = this.accessRoles.find(x => x.name === changes.selected.currentValue.name).optionLabel;
        }
    }

    changePermission(role) {
        this.selection = role.optionLabel;

        const selectedRole = this.accessRoles.filter((accessRole) => {
            if (accessRole.name === role.name) {
                return role;
            }
        })[ 0 ];
        this.onSelected.emit(selectedRole);

        return false; // return false so event will not bubble to HREF
    }
}
