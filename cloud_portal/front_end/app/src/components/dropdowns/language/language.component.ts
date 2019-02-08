import { Component, OnInit, Inject, ViewEncapsulation, Input, Output, EventEmitter } from '@angular/core';
import { TranslateService }                                                          from '@ngx-translate/core';

@Component({
    selector: 'nx-language-select',
    templateUrl: 'language.component.html',
    styleUrls: ['language.component.scss'],
    encapsulation: ViewEncapsulation.None
})

export class NxLanguageDropdown implements OnInit {
    @Input() instantReload: any;
    @Input() dropup: any;
    @Input() short: any;
    @Output() onSelected = new EventEmitter<string>();

    show: boolean;
    direction: string;
    activeLanguage = {
        language: '',
        name: ''
    };
    languages = [];
    languagesCol1 = [];
    languagesCol2 = [];

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private translate: TranslateService) {

        this.show = false;
    }

    // TODO: Bind ngModel to the component and eliminate EventEmitter

    private splitLanguages() {
        if (this.languages.length > 12) {
            const halfWayThough = Math.ceil(this.languages.length / 2);

            this.languagesCol1 = this.languages.slice(0, halfWayThough);
            this.languagesCol2 = this.languages.slice(halfWayThough, this.languages.length);
        }
    }

    change(langCode: string) {
        if (this.activeLanguage.language !== langCode) {
            /*  TODO: Currently this is not needed because the language file will
            be loaded during page reload. Once we transfer everything to Angular 5
            we should use this for seamless change of language
            // this.translate.use(lang.replace('_', '-'));
            */

            this.activeLanguage = this.languages.find(lang => {
                return (lang.language === langCode);
            });
            this.onSelected.emit(langCode);

            if (this.instantReload) {
                this.cloudApi
                    .changeLanguage(langCode)
                    .then(() => {
                        window.location.reload();
                        return false; // return false so event will not bubble to HREF
                    });
            }
        }

        return false; // return false so event will not bubble to HREF
    }

    ngOnInit(): void {
        this.direction = this.dropup ? 'dropup' : '';

        this.cloudApi
            .getLanguages()
            .then((data: any) => {
                this.languages = data.data;
                this.splitLanguages();

                const browserLang = this.translate.getBrowserCultureLang().replace('-', '_');

                this.activeLanguage = this.languages.find(lang => {
                    return (lang.language === (this.language.lang.language || browserLang));
                });

                if (!this.activeLanguage) {
                    this.activeLanguage = this.languages[0];
                }
            });
    }
}
