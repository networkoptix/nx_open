import { NgModule }                          from '@angular/core';
import { CommonModule }                      from '@angular/common';
import { BrowserModule }                     from '@angular/platform-browser';
import { downgradeComponent, UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes }              from '@angular/router';
import { FormsModule }                       from '@angular/forms';
import { NgbModule }                         from '@ng-bootstrap/ng-bootstrap';

import { DirectivesModule }       from '../../../directives/directives.module';
import { NxSystemsListComponent } from './list.component';

import { TranslateModule }     from '@ngx-translate/core';
import { ComponentsModule }    from '../../../components/components.module';
import { NxSettingsModule }    from '../settings/settings.module';
import { AuthGuard }           from '../../../routeGuards/authGuard';

const appRoutes: Routes = [
    {
        path    : 'systems', component: NxSystemsListComponent, canActivate: [AuthGuard]
    }
];

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        RouterModule,
        FormsModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,
        DirectivesModule,

        RouterModule.forChild(appRoutes),
        NxSettingsModule
    ],
    providers      : [
    ],
    declarations   : [
        NxSystemsListComponent
    ],
    bootstrap      : [],
    entryComponents: [
    ],
    exports        : [
        NxSystemsListComponent
    ]
})
export class NxSystemsListModule {
}
