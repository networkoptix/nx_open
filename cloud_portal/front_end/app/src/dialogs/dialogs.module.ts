import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { ComponentsModule } from '../components/components.module';
import { DropdownsModule }  from '../dropdowns/dropdowns.module';

import { nxDialogsService }                                   from "./dialogs.service";
import { NxModalLoginComponent, LoginModalContent }           from "./login/login.component";
import { GenericModalContent, NxModalGenericComponent }       from "./generic/generic.component";
import { DisconnectModalContent, NxModalDisconnectComponent } from "./disconnect/disconnect.component";
import { RenameModalContent, NxModalRenameComponent }         from "./rename/rename.component";
import { ShareModalContent, NxModalShareComponent }           from "./share/share.component";
import { MergeModalContent, NxModalMergeComponent }           from "./merge/merge.component";
import { downgradeInjectable }                                from "@angular/upgrade/static";
import { FormsModule, EmailValidator }                        from "@angular/forms";
import { TranslateModule }                                    from '@ngx-translate/core';


@NgModule({
    imports: [
        CommonModule,
        FormsModule,
        TranslateModule,
        ComponentsModule,
        DropdownsModule
    ],
    declarations: [
        LoginModalContent, NxModalLoginComponent,
        GenericModalContent, NxModalGenericComponent,
        DisconnectModalContent, NxModalDisconnectComponent,
        RenameModalContent, NxModalRenameComponent,
        ShareModalContent, NxModalShareComponent,
        MergeModalContent, NxModalMergeComponent,
    ],
    entryComponents: [
        LoginModalContent, NxModalLoginComponent,
        GenericModalContent, NxModalGenericComponent,
        DisconnectModalContent, NxModalDisconnectComponent,
        RenameModalContent, NxModalRenameComponent,
        ShareModalContent, NxModalShareComponent,
        MergeModalContent, NxModalMergeComponent
    ],
    providers: [
        nxDialogsService,
        NxModalLoginComponent,
        NxModalGenericComponent,
        NxModalDisconnectComponent,
        NxModalRenameComponent,
        NxModalShareComponent,
        NxModalMergeComponent,
    ],
    exports: []
})
export class DialogsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.services')
    .service('nxDialogsService', downgradeInjectable(nxDialogsService));

