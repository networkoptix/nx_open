import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxCheckboxComponent } from './checkbox.component';

describe('NxCheckboxComponent', () => {
  let component: NxCheckboxComponent;
  let fixture: ComponentFixture<NxCheckboxComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxCheckboxComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxCheckboxComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
