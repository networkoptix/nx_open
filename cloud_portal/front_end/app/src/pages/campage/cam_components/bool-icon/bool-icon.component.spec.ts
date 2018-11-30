import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { BoolIconComponent } from './bool-icon.component';

describe('BoolIconComponent', () => {
  let component: BoolIconComponent;
  let fixture: ComponentFixture<BoolIconComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ BoolIconComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(BoolIconComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
