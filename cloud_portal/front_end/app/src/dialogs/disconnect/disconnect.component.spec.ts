import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalDisconnectComponent } from './disconnect.component';

describe('NxModalShareComponent', () => {
  let component: NxModalDisconnectComponent;
  let fixture: ComponentFixture<NxModalDisconnectComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalDisconnectComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalDisconnectComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
