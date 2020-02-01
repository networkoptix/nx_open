import { Inject, Injectable } from '@angular/core';
import { NxConfigService } from './nx-config';
import { BehaviorSubject } from 'rxjs';
import { WINDOW } from "./window-provider";

enum GRID_BREAKPOINTS {
    xs = 0,
    sm = 576,
    md = 768,
    lg = 992,
    xl = 1280,
    xxl = 1440,
    xxxl = 1600,
    xxxxl = 1920,
}

@Injectable({
    providedIn: 'root'
})
export class NxScrollMechanicsService {
    CONFIG: any;
    windowSizeSubject = new BehaviorSubject({height: 0, width: 0});
    windowScrollSubject = new BehaviorSubject(0);
    elementTableWidthSubject = new BehaviorSubject(0);
    elementViewWidthSubject = new BehaviorSubject(0);
    offsetSubject = new BehaviorSubject(undefined);
    panelSubject = new BehaviorSubject(false);

    public static SCROLL_OFFSET: number = 48 + 16; // header + padding
    public static MEDIA = GRID_BREAKPOINTS;

    constructor(
            private config: NxConfigService,
            @Inject(WINDOW) private window: Window,
    ) {

        this.CONFIG = this.config.getConfig();
    }

    setOffset(height: number) {
        this.offsetSubject.next(height);
    }

    setElementTableWidth(width: number) {
        this.elementTableWidthSubject.next(width);
    }

    setElementViewWidth(width: number) {
        this.elementViewWidthSubject.next(width);
    }

    setWindowSize(height, width) {
        this.windowSizeSubject.next({ height, width });
        // this.setMediaSize(width);
    }

    setWindowScroll(value) {
        this.windowScrollSubject.next(value);
    }

    getElementOffset(el) {
        const rect = el.getBoundingClientRect();

        return rect.top + window.pageYOffset;
    }

    panelVisible(value) {
        this.panelSubject.next(value);
    }

    mediaQueryMax(media) {
        return this.window.matchMedia('(max-width: ' + media + 'px)').matches;
    }

    mediaQueryMin(media) {
        return this.window.matchMedia('(min-width: ' + media + 'px)').matches;
    }
}
