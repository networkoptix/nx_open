import { Directive, ElementRef, EventEmitter, HostListener, Input, OnInit, Output, Renderer2 } from '@angular/core';
import { NxScrollMechanicsService }                                                            from '../services/scroll-mechanics.service';

@Directive({selector: '[nxScrollMechanics]'})
export class NxScrollMechanicsDirective implements OnInit {

    // elementWidth: any;

    constructor(
            private element: ElementRef,
            private renderer: Renderer2,
            private scrollMechanicsService: NxScrollMechanicsService,
    ) {}

    ngOnInit() {
        setTimeout(() => {
            this.renderer.setStyle(this.element.nativeElement, 'width', '100%');

            this.scrollMechanicsService
                .elementViewWidthSubject
                .subscribe(() => {
                    const width = this.scrollMechanicsService.elementViewWidthSubject.getValue();
                    this.renderer.setStyle(this.element.nativeElement, 'width', (width > 0) ? (width - 8 /* -gutter */) + 'px' : '100%');
                });
        });
    }
}
