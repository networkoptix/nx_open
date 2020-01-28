import {
    Component, OnInit, Input,
    SimpleChanges, OnChanges
}                          from '@angular/core';
import { NxConfigService } from '../../../services/nx-config';
import { Router }          from '@angular/router';

@Component({
    selector: 'nx-active-system',
    templateUrl: 'active-system.component.html',
    styleUrls: ['active-system.component.scss']
})

export class NxActiveSystemDropdown implements OnInit, OnChanges {
    @Input() activeSystem: any;
    CONFIG: any;

    canViewInfo: boolean;
    params: any;
    show: boolean;
    active = {
        health: false,
        settings: false,
        view: false,
    };

    constructor(private config: NxConfigService,
                private router: Router,
    ) {
        this.CONFIG = this.config.getConfig();
        this.show = false;
    }

    private updateActive(endpoint = 'settings') {
        this.active.health = (endpoint === 'health');
        this.active.view = (endpoint === 'view');
        this.active.settings = (endpoint === 'settings');
        this.show = false;
    }

    ngOnInit(): void {
        this.updateActive(this.router.url.split('/')[2]);
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.activeSystem) {
            if (!('id' in changes.activeSystem.currentValue)) {
                this.activeSystem = {id: '0'}; // Avoid JS timing error (in console)
            } else if (changes.activeSystem.currentValue.id !== '0') {
                this.canViewInfo = this.CONFIG.accessRoles.adminAccess
                    .includes(changes.activeSystem.currentValue.accessRole.toLowerCase());
            }
        }
    }
}
