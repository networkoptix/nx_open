import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalComponent } from './modal.component';

describe('ModalComponent', () => {
  let component: NxModalComponent;
  let fixture: ComponentFixture<NxModalComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
