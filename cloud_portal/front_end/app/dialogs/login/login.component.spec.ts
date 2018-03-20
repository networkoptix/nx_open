import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalLoginComponent } from './login.component';

describe('NxModalLoginComponent', () => {
  let component: NxModalLoginComponent;
  let fixture: ComponentFixture<NxModalLoginComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalLoginComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalLoginComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
