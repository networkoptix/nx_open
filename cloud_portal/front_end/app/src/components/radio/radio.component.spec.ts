import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxRadioComponent } from './radio.component';

describe('NxRadioComponent', () => {
  let component: NxRadioComponent;
  let fixture: ComponentFixture<NxRadioComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxRadioComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxRadioComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
