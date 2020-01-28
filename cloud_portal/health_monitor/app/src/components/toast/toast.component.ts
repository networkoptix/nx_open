import { Component, TemplateRef } from '@angular/core';

import { NxToastService }                      from '../../dialogs/toast.service';
import { animate, style, transition, trigger } from '@angular/animations';

@Component({
    selector: 'app-toasts',
    template: `
            <div *ngFor="let toast of toastService.toasts;" @fadeInOut>
                <ngb-toast
                    class="alert alert-{{toast.classname}} fade"
                    [ngClass]="{'alert-dismissible': !toast.autohide}"
                    [autohide]="toast.autohide"
                    [delay]="toast.delay"
                    (hide)="remove(toast)">
                    <ng-template [ngIf]="isTemplate(toast)" [ngIfElse]="text">
                        <ng-template [ngTemplateOutlet]="toast.textOrTpl"></ng-template>
                    </ng-template>
                    
                    <ng-template #text>
                        <span class="toast-content" [innerHTML]="toast.textOrTpl"></span>
                        <button type="button" class="close" data-dismiss="alert" aria-label="Close"
                                *ngIf="!toast.autohide" (click)="remove(toast)">
                            <span aria-hidden="true">&times;</span>
                        </button>
                    </ng-template>
                </ngb-toast>
            </div>`,
    styleUrls: ['toast.component.scss'],
    host    : { '[class.nx-toasts]': 'true' },
    animations: [
        trigger('fadeInOut', [
            transition(':enter', [
                style({ opacity: 0 }),
                animate('.2s ease-in', style({ opacity: 1 })),
            ]),
            transition(':leave', [
                animate('.5s ease-out', style({ opacity: 0 }))
            ])
        ]),
    ],
})
export class ToastsContainer {

    constructor(public toastService: NxToastService) {
    }

    isTemplate(toast) {
        return toast.textOrTpl instanceof TemplateRef;
    }

    remove(toast) {
        this.toastService.remove(toast);
    }
}