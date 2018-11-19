import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxCarouselComponent } from './carousel.component';

describe('NxCarouselComponent', () => {
    let component: NxCarouselComponent;
    let fixture: ComponentFixture<NxCarouselComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
            declarations: [NxCarouselComponent]
        })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxCarouselComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
