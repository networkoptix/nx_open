import { Component, Input }  from '@angular/core';
import { NxSettingsService } from '../pages/systems/settings/settings.service';

@Component({
    selector: 'nx-menu-button',
    template: `<button class="btn btn-menu btn-clear" 
                       [disabled]="button.disabled"
                       (click)="action()">{{button.label}}</button>`
})
export class NxMenuButtonComponent {
    @Input() button;

    constructor(private settingsService: NxSettingsService) {}

    action() {
        if (this.button.id === 'addUser') {
            // Handling promise to satisfy the linter.
            this.settingsService.addUser().then(() => {});
        }
    }
}
