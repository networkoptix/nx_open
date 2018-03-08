import { Component } from '@angular/core';

@Component({
    selector: 'nx-app',
    template: `
        <router-outlet></router-outlet>
        <div ng-view="" ng-model-options="{ updateOn: 'blur' }"></div>
    `,
})

export class AppComponent {
}
