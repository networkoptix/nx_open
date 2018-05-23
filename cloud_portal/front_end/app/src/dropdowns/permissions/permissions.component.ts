import { Component, OnInit, Inject, ViewEncapsulation, Input, Output, EventEmitter } from '@angular/core';
import { NgbDropdownModule }                                                         from '@ng-bootstrap/ng-bootstrap';
import { TranslateService }                                                          from "@ngx-translate/core";

@Component({
    selector: 'nx-permissions-select',
    templateUrl: 'permissions.component.html',
    styleUrls: ['permissions.component.scss'],
    encapsulation: ViewEncapsulation.None,
})

export class NxPermissionsDropdown implements OnInit {
    @Input() roles: any;
    @Input() selected: any;
    @Output() onSelected = new EventEmitter<string>();

    selection: string;

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private translate: TranslateService) {


    }

    ngOnInit(): void {
        this.selection = (this.selected) ? this.selected : 'Please select...';
    }

    changePermission(role) {
        this.selection = role.optionLabel;
        this.onSelected.emit(this.selection);
    }
}
