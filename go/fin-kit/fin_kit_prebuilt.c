/**
 * @file fin_kit_prebuilt.c
 * @brief Minimal CGO wrapper for prebuilt library mode
 *
 * This file contains only the minimal stub needed to link against
 * the prebuilt fin-kit library. All actual implementations come from
 * the precompiled static library.
 */

#include "fin_kit.h"

/* These functions are already defined in the prebuilt library.
 * This file is just here to satisfy cgo's requirement for at least
 * one C file when using import "C".
 */


