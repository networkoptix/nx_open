import { BehaviorSubject } from 'rxjs';
import { Injectable } from '@angular/core';


@Injectable({
    providedIn: 'root'
})
export class NxQueryParamService {
    queryParamsSubject = new BehaviorSubject({});
    constructor() {}

    get queryParams() {
        return this.queryParamsSubject.getValue();
    }

    set queryParams(params) {
        if (params !== this.queryParams) {
            this.queryParamsSubject.next(params);
        }
    }
}
