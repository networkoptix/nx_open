import { Injectable, OnDestroy } from '@angular/core';
import { BehaviorSubject }       from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class NxMenuService implements OnDestroy {
    selectedSectionSubject = new BehaviorSubject([]);
    selectedSubSectionSubject = new BehaviorSubject([]);
    selectedDetailsSection = new BehaviorSubject([]);

    constructor() {
    }

    setSection(section) {
        this.selectedSectionSubject.next(section);
    }

    setSubSection(section) {
        this.selectedSubSectionSubject.next(section);
    }

    setDetailsSection(section) {
        this.selectedDetailsSection.next(section);
    }

    ngOnDestroy() {
        this.selectedSectionSubject.unsubscribe();
        this.selectedSubSectionSubject.unsubscribe();
        this.selectedDetailsSection.unsubscribe();
    }
}