import { Inject, Injectable } from '@angular/core';
import { LocalStorageService }       from 'ngx-store';
import { ReplaySubject } from 'rxjs';
import { WINDOW } from './window-provider';

@Injectable({
    providedIn: 'root'
})
export class NxSessionService {
    loginStateSubject = new ReplaySubject(0);
    session: any;

    constructor(private localStorageService: LocalStorageService,
                @Inject(WINDOW) private window: Window) {
        this.session = this.localStorageService;
        this.loginStateSubject.next(this.loginState || '');

        // Listens to changes from other browser tabs.
        this.window.addEventListener('storage', (event) => {
            if (event.key === 'ngx_loginState') {
                this.window.location.reload();
            }
        });
    }

    invalidateSession() {
        this.session.set('loginState', undefined);
        this.loginStateSubject.next(this.loginState);
    }

    get email() {
        return this.session.get('email') || '';
    }

    set email(email: string) {
        this.session.set('email', email);
    }

    get loginState() {
        return this.session.get('loginState');
    }

    set loginState(email: string) {
        this.session.set('loginState', email);
        this.loginStateSubject.next(email);
    }
}
