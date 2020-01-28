import { Component, OnInit, ViewEncapsulation } from '@angular/core';

import { NxLanguageProviderService } from '../../../services/nx-language-provider';
import { NxPageService }             from '../../../services/page.service';

@Component({
    selector   : 'nx-no-systems',
    templateUrl: 'no-systems.component.html',
    styleUrls  : ['no-systems.component.scss'],
    encapsulation: ViewEncapsulation.None
})

export class NxNoSystemsComponent implements OnInit {
    LANG: any = {};

    private setupDefaults() {
        this.LANG = this.language.getTranslations();

        this.pageService.setPageTitle(this.LANG.pageTitles.systems);
    }

    constructor(private language: NxLanguageProviderService,
                private pageService: NxPageService,
    ) {
        this.setupDefaults();
    }

    ngOnInit(): void {
    }
}

