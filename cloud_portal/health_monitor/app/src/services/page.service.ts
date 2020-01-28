import { Injectable } from '@angular/core';
import { NxConfigService }    from './nx-config';
import { Title, Meta }              from '@angular/platform-browser';
import { NxLanguageProviderService } from './nx-language-provider';

@Injectable({
    providedIn: 'root'
})
export class NxPageService {
    CONFIG: any;
    LANG: any;

    constructor(private config: NxConfigService,
                private title: Title,
                private language: NxLanguageProviderService,
                private meta: Meta) {

        this.CONFIG = this.config.getConfig();
    }

    // called from app component
    setLanguage(lang) {
        this.LANG = lang;
    }

    setPageTitle(value: string, useAltTemplate?: boolean) {
        let title = value;
        if (this.LANG && title !== this.LANG.pageTitles.default) {
            title = this.LANG.pageTitles.template.replace('{{title}}', value);
            if (useAltTemplate) {
                title = title.replace('- ', '');
            }
        }
        this.title.setTitle(title);
    }

    setDefaultLayout() {
        this.meta.updateTag({name: 'viewport', content: this.CONFIG.meta.viewport.default});
    }

    setDesktopLayout() {
        this.meta.updateTag({name: 'viewport', content: this.CONFIG.meta.viewport.desktopLayout});
    }
}
