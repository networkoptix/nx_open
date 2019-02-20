import { Injectable } from '@angular/core';
import { NxRibbonComponent } from './ribbon.component';
import { BehaviorSubject } from 'rxjs';


@Injectable()
export class NxRibbonService {

    context = {
        visibility: false,
        message: '',
        text: '',
        url: ''
    };
    contextSubject = new BehaviorSubject(this.context);

    constructor() {
    }

    show(message, text, url) {
        this.context = {
            visibility: true,
            message,
            text,
            url
        };
        this.contextSubject.next(this.context);
    }

    hide() {
        this.context = {
            visibility: false,
            message: '',
            text: '',
            url: ''
        };
        this.contextSubject.next(this.context);
    }
}
