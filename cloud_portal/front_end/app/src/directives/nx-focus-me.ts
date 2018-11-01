import { Directive, ElementRef, OnInit } from '@angular/core';

@Directive({selector: '[nxFocusMe]'})
export class NxFocusMeDirective implements OnInit {

    constructor(private _elementRef: ElementRef) {
    }

    ngOnInit() {
        setTimeout(() => {
            this._elementRef.nativeElement.focus();
        });
    }
}
