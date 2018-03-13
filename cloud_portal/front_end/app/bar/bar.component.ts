import { Component, OnInit, ChangeDetectorRef } from '@angular/core';
import { HttpErrorResponse } from "@angular/common/http";
import { HttpClient } from '@angular/common/http';

import { QuoteService } from '../../app/core/index';
import { uuid2Service } from '../../app/scripts/services/angular-uuid2';
import { languageService } from '../../app/scripts/services/language';

@Component({
    selector: 'bar-component',
    templateUrl: './bar/bar.component.html'
})
export class BarComponent implements OnInit {
    message: string;
    serviceMessage: string;
    languageMessage: any;
    title = 'bar';
    uuid2: any;
    language: any;
    activeLanguage: any;

    constructor(uuid2: uuid2Service,
                language: languageService,
                private http: HttpClient,
                private quoteService: QuoteService,
                private changeDetector: ChangeDetectorRef) {

        this.uuid2 = uuid2;
        this.language = language;
    }

    getLanguages(): any {
        return this.http.get('/static/languages.json', {});
    }

    ngOnInit(): void {
        this.serviceMessage = this.uuid2.newguid();
        this.languageMessage = this.language.getLanguage();

        this.quoteService
                .getRandomQuote({ category: 'dev' })
                .subscribe(
                        (data: any) => {
                            this.message = data.value;
                            // this.changeDetector.detectChanges();
                        },
                        (err: HttpErrorResponse) => {
                            console.log(err.error);
                            console.log(err.name);
                            console.log(err.message);
                            console.log(err.status);
                        });

        this.getLanguages()
                .subscribe((data: any) => {
                    this.activeLanguage = data.data;
                });
    }
}

