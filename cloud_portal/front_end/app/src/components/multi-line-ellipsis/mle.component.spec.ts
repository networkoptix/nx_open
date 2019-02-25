import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxMultiLineEllipsisComponent } from './mle.component';

describe('NxMultiLineEllipsisComponent', () => {
    let component: NxMultiLineEllipsisComponent;
    let fixture: ComponentFixture<NxMultiLineEllipsisComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
                declarations: [NxMultiLineEllipsisComponent ]
            })
            .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxMultiLineEllipsisComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });

});
