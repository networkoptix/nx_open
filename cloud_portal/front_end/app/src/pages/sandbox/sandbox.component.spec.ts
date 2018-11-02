import { TestBed }           from '@angular/core/testing';
import { NxSandboxComponent } from './sandbox.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [NxSandboxComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(NxSandboxComponent);
    expect(fixture.componentInstance instanceof NxSandboxComponent).toBe(true, 'should create NxSandboxComponent');
  });
});
