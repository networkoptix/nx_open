import { NgModule }                          from '@angular/core';
import { CommonModule }                      from '@angular/common';
import { BrowserModule }                     from '@angular/platform-browser';
import { UpgradeModule } from '@angular/upgrade/static';
import { RouterModule, Routes }              from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxSystemSettingsComponent } from './settings.component';

import { TranslateModule }  from '@ngx-translate/core';
import { ComponentsModule } from '../../../components/components.module';

import { NxSystemAdminModule }       from './admin/admin.module';
import { NxSystemUsersModule }       from './users/users.module';
import { NxSystemMergeStatusModule } from './merge-status/merge-status.module';
import { NxSystemAdminComponent }    from './admin/admin.component';
import { NxSystemUsersComponent }    from './users/users.component';
import { NxNoSystemsComponent }      from '../no-systems/no-systems.component';
import { ApplyGuard }                from '../../../routeGuards/applyGuard';
import { AuthGuard }                 from '../../../routeGuards/authGuard';

const appRoutes: Routes = [
    // root path is handles by AJS for now
    {
        path    : 'systems/:systemId',
        component: NxSystemSettingsComponent,
        canActivate: [AuthGuard],
        children: [
            {
                path: '',
                component: NxSystemAdminComponent,
                canDeactivate: [ApplyGuard]
            },
            {
                path: 'share',
                component: NxSystemUsersComponent,
            },
            {
                path: 'users',
                component: NxSystemUsersComponent,
                canDeactivate: [ApplyGuard]
            },
            {
                path: 'users/:userId',
                component: NxSystemUsersComponent,
                canDeactivate: [ApplyGuard]
            }
        ]
    }
];

@NgModule({
    imports        : [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        RouterModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,
        NxSystemMergeStatusModule,
        NxSystemAdminModule,
        NxSystemUsersModule,

        RouterModule.forChild(appRoutes)
    ],
    providers      : [
        ApplyGuard,
    ],
    declarations   : [
        NxSystemSettingsComponent,
        NxNoSystemsComponent,
    ],
    bootstrap      : [],
    entryComponents: [
        NxSystemSettingsComponent
    ],
    exports: [
        NxSystemSettingsComponent,
        NxNoSystemsComponent
    ]
})
export class NxSettingsModule {
}
