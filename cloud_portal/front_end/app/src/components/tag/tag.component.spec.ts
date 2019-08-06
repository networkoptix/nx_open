import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxTagComponent } from './tag.component';

describe('NxTagComponent', () => {
    let component: NxTagComponent;
    let fixture: ComponentFixture<NxTagComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
            declarations: [NxTagComponent]
        })
                .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxTagComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
