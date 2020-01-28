import { NgModule }                  from '@angular/core';
import { CommonModule }              from '@angular/common';

import { NxLanguageProviderService }     from './nx-language-provider';
import { NxConfigService }               from './nx-config';
import { NxAppStateService }             from './nx-app-state.service';
import { NxUtilsService }                from './utils.service';
import { NxPageService }                 from './page.service';
import { NxSystemsService }              from './systems.service';
import { NxAccountService }              from './account.service';
import { NxUrlProtocolService }          from './url-protocol.service';
import { NxApplyService }                from './apply.service';
import { NxHeaderService }               from './nx-header.service';
import { NxScrollMechanicsService }      from './scroll-mechanics.service';

@NgModule({
    imports        : [
        CommonModule,
    ],
    declarations   : [
    ],
    entryComponents: [
    ],
    providers      : [
        NxAppStateService,
        NxApplyService,
        NxLanguageProviderService,
        NxConfigService,
        NxUtilsService,
        NxPageService,
        NxSystemsService,
        NxAccountService,
        NxUrlProtocolService,
        NxHeaderService,
        NxScrollMechanicsService,
    ],
    exports        : []
})
export class ServiceModule {
}

