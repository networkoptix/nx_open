import { Component, OnInit, ChangeDetectorRef } from '@angular/core';
import { HttpErrorResponse } from "@angular/common/http";

import { QuoteService } from '../../app/core/index';

@Component({
    selector: 'bar-component',
    templateUrl: 'bar.component.html'
})
export class BarComponent implements OnInit {
    message: string;
    title = 'bar';

    constructor(private quoteService: QuoteService, private changeDetector: ChangeDetectorRef) {
    }

    ngOnInit(): void {
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

    }
}

