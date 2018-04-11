import { Component, OnInit, ChangeDetectorRef, Inject } from '@angular/core';
import { HttpErrorResponse } from "@angular/common/http";
import { HttpClient } from '@angular/common/http';

import { QuoteService }          from '../core/index';
import { NxModalLoginComponent } from "../../src/dialogs/login/login.component";

@Component({
    selector: 'bar-component',
    templateUrl: 'bar.component.html'
})

export class BarComponent implements OnInit {
    message: string;
    serviceMessage: string;
    languageMessage: any;
    title = 'bar';

    // cloudApi: any;
    // uuid2: any;
    // language: any;

    activeLanguage: any;

    constructor(@Inject('uuid2Service') private uuid2: any,
                @Inject('languageService') private language: any,
                @Inject('cloudApiService') private cloudApi: any,
                private http: HttpClient,
                private quoteService: QuoteService,
                private loginModal: NxModalLoginComponent,
                private changeDetector: ChangeDetectorRef,
    ){

        // this.uuid2 = uuid2;
        //this.language = language;
        //this.cloudApi = cloudApi;
    }

    // getLanguages(): any {
    //     return this.http.get('/static/languages.json', {});
    // }

    login () {
        // this.loginModal.open();
    }

    ngOnInit(): void {
        this.serviceMessage = this.uuid2.newguid();

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

        // this.getLanguages()
        //         .subscribe((data: any) => {
        //             console.log(data);
        //             this.activeLanguage = data;
        //         });

        this.cloudApi
                .getLanguages()
                .then((data) => {
                    // console.log('Data: ', data.data);
                    // console.log('Length: ', data.data.length);
                });

    }
}

