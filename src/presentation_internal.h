#ifndef FOUNDATION_PRESENTATION_INTERNAL_H
#define FOUNDATION_PRESENTATION_INTERNAL_H

#include "foundation/presentation.h"

/* Test-only allocation-failure seam; SIZE_MAX disables failure injection. */
void factory_presentation_test_fail_allocations_after(size_t successes);

#endif
