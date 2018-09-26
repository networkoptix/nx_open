import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxIntegrationsComponent } from './integrations.component';

import { TranslateModule }             from '@ngx-translate/core';
import { ComponentsModule }            from '../../components/components.module';
import { NxIntegrationsListComponent } from './list/list.component';
// import { NxUsersDetailComponent }   from '../details/users/users.component';
// import { NxOtherDetailsComponent }  from '../details/others/others.component';

const appRoutes: Routes = [
    {
        path    : 'integrations', component: NxIntegrationsComponent,
        children: [
            { path: '', redirectTo: 'list', pathMatch: 'full' },
            { path: 'list', component: NxIntegrationsListComponent },
            // {path: 'users', component: NxUsersDetailComponent},
            // {path: 'other', component: NxOtherDetailsComponent}
        ]
    }
];

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [],
    declarations   : [
        NxIntegrationsComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxIntegrationsComponent
    ],
    exports        : [
        NxIntegrationsComponent
    ]
})
export class IntegrationsModule {
}
