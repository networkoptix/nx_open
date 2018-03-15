import { Component, OnInit, ChangeDetectorRef, Inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { NgbDropdownModule } from '@ng-bootstrap/ng-bootstrap';

export interface activeLanguage {
    language: string;
    name: string;
}

@Component({
    selector: 'nx-language-select',
    templateUrl: './dropdown/language.component.html',
    styleUrls: ['./dropdown/language.component.scss'],
    inputs: ['accountMode'],
})

export class NxLanguageDropdown implements OnInit {
    accountMode: boolean;
    activeLanguage = {
        language: '',
        name: ''
    };
    languages = [];

    constructor(@Inject('cloudApiService') private cloudApi: any,
                @Inject('languageService') private language: any,
                private httpClient: HttpClient,
                private dropdown: NgbDropdownModule,
                private changeDetector: ChangeDetectorRef) {
    }

    changeLanguage(lang: activeLanguage){
        this.activeLanguage = lang;
    }

    ngOnInit(): void {
        this.accountMode = this.accountMode || false;

        this.cloudApi
                .getLanguages()
                .then((data: any) => {
                    this.languages = data.data;

                    this.activeLanguage = this.languages.find(lang => {
                        return (lang.language === this.language.lang.language);
                    });

                    if (!this.activeLanguage) {
                        this.activeLanguage = this.languages[0];
                    }
                });
    }
}
