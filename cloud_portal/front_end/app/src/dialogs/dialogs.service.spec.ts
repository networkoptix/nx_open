import { TestBed, inject } from '@angular/core/testing';

import { NxDialogsService } from './dialogs.service';

describe('NxDialogsService', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [NxDialogsService]
    });
  });

  it('should be created', inject([NxDialogsService], (service: NxDialogsService) => {
    expect(service).toBeTruthy();
  }));
});
