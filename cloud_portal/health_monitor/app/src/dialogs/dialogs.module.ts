import { NgModule }     from '@angular/core';
import { CommonModule } from '@angular/common';

import { ComponentsModule } from '../components/components.module';
import { DirectivesModule } from '../directives/directives.module';
import { LoginModalContent }                            from './login/login.component';
import { DisconnectModalContent }                       from './disconnect/disconnect.component';
import { RenameModalContent }                           from './rename/rename.component';
import { AddUserModalContent, NxModalAddUserComponent } from './add-user/add-user.component';
import { RemoveUserModalContent, NxModalRemoveUserComponent } from './remove-user/remove-user.component';
import { MergeModalContent }                            from './merge/merge.component';
import { MessageModalContent }                          from './message/message.component';
import { EmbedModalContent }                            from './embed/embed.component';
import { FormsModule }                  from '@angular/forms';
import { TranslateModule }                              from '@ngx-translate/core';
import { ClipboardModule }                              from 'ngx-clipboard';

import { GenericModalContent, NxModalGenericComponent } from './generic/generic.component';
import { NxDialogsService }                             from './dialogs.service';
import { ApplyModalContent, NxModalApplyComponent }     from './apply/apply.component';
import { RouterModule }                                 from '@angular/router';

@NgModule({
    imports: [
        CommonModule,
        FormsModule,
        TranslateModule,
        ClipboardModule,
        ComponentsModule,
        DirectivesModule,
        RouterModule,
    ],
    declarations   : [
        LoginModalContent,
        DisconnectModalContent,
        RenameModalContent,
        AddUserModalContent,
        MergeModalContent,
        MessageModalContent,
        RemoveUserModalContent,
        EmbedModalContent,
        GenericModalContent,
        NxModalGenericComponent,
        NxModalAddUserComponent,
        NxModalRemoveUserComponent,
        ApplyModalContent,
        NxModalApplyComponent,
    ],
    entryComponents: [
        LoginModalContent,
        DisconnectModalContent,
        RenameModalContent,
        AddUserModalContent,
        MergeModalContent,
        MessageModalContent,
        RemoveUserModalContent,
        EmbedModalContent,
        GenericModalContent,
        NxModalGenericComponent,
        NxModalAddUserComponent,
        NxModalRemoveUserComponent,
        ApplyModalContent,
        NxModalApplyComponent,
    ],
    providers      : [
        NxDialogsService,
        NxModalGenericComponent,
        NxModalAddUserComponent,
        NxModalRemoveUserComponent,
        NxModalApplyComponent,
    ],
    exports        : []
})
export class DialogsModule {
}
