import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxPagePlaceholderComponent } from './page-placeholder.component';

describe('NxPagePlaceholderComponent', () => {
  let component: NxPagePlaceholderComponent;
  let fixture: ComponentFixture<NxPagePlaceholderComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxPagePlaceholderComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxPagePlaceholderComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
