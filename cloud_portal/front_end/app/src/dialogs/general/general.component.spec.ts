import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import {NxModalGeneralComponent } from './general.component';

describe('NxModalGeneralComponent', () => {
  let component: NxModalGeneralComponent;
  let fixture: ComponentFixture<NxModalGeneralComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalGeneralComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalGeneralComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
