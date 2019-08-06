import { TestBed, inject } from '@angular/core/testing';

import { NxRibbonService } from './ribbon.service';
import { BehaviorSubject } from 'rxjs';

describe('NxRibbonService', () => {
    beforeEach(() => {
        TestBed.configureTestingModule({
            providers: [NxRibbonService]
        });
    });

    it('should be created', inject([NxRibbonService], (service: NxRibbonService) => {
        expect(service).toBeTruthy();
    }));

    it('should be initialized', inject([NxRibbonService], (service: NxRibbonService) => {
        expect(service.context.visibility).toBeFalsy();
        expect(service.context.message).toBe('');
        expect(service.context.text).toBe('');
        expect(service.context.url).toBe('');
    }));

    it('show() should emit data to contextSubject',
            inject([NxRibbonService], (service: NxRibbonService) => {
                const context = {
                    visibility: true,
                    message: 'Alcohol! Because no great story started with someone eating a salad.',
                    text : 'Go back',
                    url    : '/admin/cms/product/'
                };
                service.contextSubject = new BehaviorSubject(context);

                service.show(context.message, context.text, context.url);

                service.contextSubject.subscribe((message) => {
                    expect(message).toBe(context);
                });
            }));

    it('hide() should emit data to contextSubject',
            inject([NxRibbonService], (service: NxRibbonService) => {
                const context = {
                    visibility: false,
                    message   : '',
                    text      : '',
                    url       : ''
                };
                service.contextSubject = new BehaviorSubject(context);

                service.hide();

                service.contextSubject.subscribe((message) => {
                    expect(message).toBe(context);
                });
            }));
});
