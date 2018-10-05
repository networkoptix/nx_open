import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxContentBlockSectionComponent } from './section.component';

describe('NxContentBlockSectionComponent', () => {
    let component: NxContentBlockSectionComponent;
    let fixture: ComponentFixture<NxContentBlockSectionComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
                declarations: [ NxContentBlockSectionComponent ]
            })
            .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxContentBlockSectionComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
