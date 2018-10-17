import {
    Component, OnInit, Inject, ViewEncapsulation,
    Input, Output, EventEmitter, SimpleChanges
} from '@angular/core';
import { TranslateService } from '@ngx-translate/core';

@Component({
    selector     : 'nx-permissions-select',
    templateUrl  : 'permissions.component.html',
    styleUrls    : [ 'permissions.component.scss' ],
    encapsulation: ViewEncapsulation.None,
})

export class NxPermissionsDropdown implements OnInit {
    @Input() user: any;
    @Input() roles: any;
    @Input() system: any;
    @Input() selected: any;
    @Output() onSelected = new EventEmitter<string>();

    selection: string;
    message: string;
    show: boolean;
    accessRoles: any;
    differ: any;

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                // private differs: KeyValueDiffers,
                private translate: TranslateService) {

        this.accessRoles = [];
        this.show = false;

        // Keeping this just for reference ... ngDoCheck runs often so it's unnecessary overhead
        // this.differ = this.differs.find({}).create();

        translate.get('Please select...')
            .subscribe((res: string) => {
                this.message = res;
            });
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    ngOnInit(): void {
    }

    processAccessRoles() {
        // const roles = this.system.accessRoles || this.configService.config.accessRoles.predefinedRoles;
        if (this.roles) {
            this.accessRoles = this.roles.filter((role) => {
                if (!(role.isOwner || role.isAdmin && !this.system.isMine)) {
                    role.optionLabel = this.language.lang.accessRoles[ role.name ].label || role.name;
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
        if (changes.roles && changes.roles.currentValue) {
            this.processAccessRoles();
            const role = this.accessRoles.filter(x => x.name === this.selected.name)[ 0 ];
            this.selection = '';

            if (role) {
                this.selection = role.optionLabel || this.message;
                this.changePermission(role);
            }
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
