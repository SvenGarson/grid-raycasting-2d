#ifndef DATATYPES_H
#define DATATYPES_H

#include <stdbool.h>

typedef struct {
  int x;
  int y;
} SVec2i;

typedef struct {
  float x;
  float y;
} SVec2f;

typedef struct {
  SVec2f min;
  SVec2f max;
} SAABB4f;

typedef struct {
  bool x;
  bool y;
} SVec2b;

typedef struct {
  float impact_time;
  SVec2f impact_point;
  SVec2i impact_tile;
} SImpactInformation;

#endif
