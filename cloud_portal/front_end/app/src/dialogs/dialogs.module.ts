import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { ComponentsModule } from '../components/components.module';
import { DirectivesModule } from '../directives/directives.module';

import { NxDialogsService }                                   from './dialogs.service';
import { NxModalLoginComponent, LoginModalContent }           from './login/login.component';
import { GenericModalContent, NxModalGenericComponent }       from './generic/generic.component';
import { DisconnectModalContent, NxModalDisconnectComponent } from './disconnect/disconnect.component';
import { RenameModalContent, NxModalRenameComponent }         from './rename/rename.component';
import { ShareModalContent, NxModalShareComponent }           from './share/share.component';
import { MergeModalContent, NxModalMergeComponent }           from './merge/merge.component';
import { MessageModalContent, NxModalMessageComponent }       from './message/message.component';
import { EmbedModalContent, NxModalEmbedComponent }           from './embed/embed.component';
import { downgradeInjectable }                                from '@angular/upgrade/static';
import { FormsModule, EmailValidator }                        from '@angular/forms';
import { TranslateModule }                                    from '@ngx-translate/core';
import { ClipboardModule }                                    from 'ngx-clipboard';


@NgModule({
    imports        : [
        CommonModule,
        FormsModule,
        TranslateModule,
        ClipboardModule,
        ComponentsModule,
        DirectivesModule,
    ],
    declarations   : [
        LoginModalContent, NxModalLoginComponent,
        GenericModalContent, NxModalGenericComponent,
        DisconnectModalContent, NxModalDisconnectComponent,
        RenameModalContent, NxModalRenameComponent,
        ShareModalContent, NxModalShareComponent,
        MergeModalContent, NxModalMergeComponent,
        MessageModalContent, NxModalMessageComponent,
        EmbedModalContent, NxModalEmbedComponent,
    ],
    entryComponents: [
        LoginModalContent, NxModalLoginComponent,
        GenericModalContent, NxModalGenericComponent,
        DisconnectModalContent, NxModalDisconnectComponent,
        RenameModalContent, NxModalRenameComponent,
        ShareModalContent, NxModalShareComponent,
        MergeModalContent, NxModalMergeComponent,
        MessageModalContent, NxModalMessageComponent,
        EmbedModalContent, NxModalEmbedComponent,
    ],
    providers      : [
        NxDialogsService,
        NxModalLoginComponent,
        NxModalGenericComponent,
        NxModalDisconnectComponent,
        NxModalRenameComponent,
        NxModalShareComponent,
        NxModalMergeComponent,
        NxModalMessageComponent,
        NxModalEmbedComponent,
    ],
    exports        : []
})
export class DialogsModule {
}

declare var angular: angular.IAngularStatic;
angular
    .module('cloudApp.services')
    .service('NxDialogsService', downgradeInjectable(NxDialogsService));

