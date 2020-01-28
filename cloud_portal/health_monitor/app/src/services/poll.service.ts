import { BehaviorSubject, concat, Observable, of } from 'rxjs';
import { concatMap, delay, skip, tap } from 'rxjs/operators';
import { Injectable } from '@angular/core';

@Injectable({
    providedIn: 'root'
})
/*
 * How to use the poll service.
 *
 * Declaring the poll.
 * const examplePoll = this.pollService.createPoll(this.cloudApi.systems(), 10000);
 * After the observable resolves it will make the call again in 10 seconds.
 *
 * To start the poll subscribe to it.
 * const currentSubscription = examplePoll.subscribe((data: any) => { console.log(data); });
 *
 * Stopping the poll.
 * currentSubscription.unsubscribe();
 *
 * To completely kill the poll.
 * examplePoll.unsubscribe();
 */
export class NxPollService {
    constructor() {}
    createPoll (apiCall: Observable<any>, intervalDelay: number) {
        const load$ = new BehaviorSubject('');
        const refresh$ = of('').pipe(
            delay(intervalDelay),
            tap(_ => load$.next('')),
            skip(1));

        const poll$ = concat(apiCall, refresh$);

        return load$.pipe(concatMap(_ => poll$));
    }
}
