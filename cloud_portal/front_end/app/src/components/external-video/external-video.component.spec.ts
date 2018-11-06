import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxExternalVideoComponent } from './external-video.component';

describe('NxContentBlockComponent', () => {
    let component: NxExternalVideoComponent;
    let fixture: ComponentFixture<NxExternalVideoComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
            declarations: [NxExternalVideoComponent]
        })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxExternalVideoComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
