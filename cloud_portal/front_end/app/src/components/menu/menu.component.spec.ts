import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxMenuComponent } from './menu.component';

describe('NxMenuComponent', () => {
  let component: NxMenuComponent;
  let fixture: ComponentFixture<NxMenuComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxMenuComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxMenuComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
