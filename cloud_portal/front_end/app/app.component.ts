import { Component } from '@angular/core';
import {NxHeaderComponent} from './components/header/header.component';

@Component({
    selector: 'nx-app',
    template: `
        <nx-header></nx-header>
        <router-outlet></router-outlet>
        <div ng-view="" ng-model-options="{ updateOn: 'blur' }"></div>
    `,
})

export class AppComponent {
}
