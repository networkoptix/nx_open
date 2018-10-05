import { NgModule }             from '@angular/core';
import { CommonModule }         from '@angular/common';
import { BrowserModule }        from '@angular/platform-browser';
import { UpgradeModule }        from '@angular/upgrade/static';
import { RouterModule, Routes } from '@angular/router';

import { NgbModule } from '@ng-bootstrap/ng-bootstrap';

import { NxMainComponent } from './main.component';

import { TranslateModule }          from '@ngx-translate/core';
import { ComponentsModule }         from '../../components/components.module';
import { NxServersDetailComponent } from '../details/servers/servers.component';
import { NxUsersDetailComponent }   from '../details/users/users.component';
import { NxOtherDetailsComponent }  from '../details/others/others.component';

const appRoutes: Routes = [
    {path: 'main', component: NxMainComponent,
        children: [
            {path: '', redirectTo: 'other', pathMatch: 'full'},
            {path: 'servers', component: NxServersDetailComponent},
            {path: 'users', component: NxUsersDetailComponent},
            {path: 'other', component: NxOtherDetailsComponent}
        ]}
];

// TODO: Remove it after test

@NgModule({
    imports: [
        CommonModule,
        BrowserModule,
        UpgradeModule,
        NgbModule,
        TranslateModule,
        ComponentsModule,

        RouterModule.forChild(appRoutes)
    ],
    providers: [],
    declarations: [
        NxMainComponent,
    ],
    bootstrap: [],
    entryComponents: [
        NxMainComponent
    ],
    exports: [
        NxMainComponent
    ]
})
export class MainModule {
}
