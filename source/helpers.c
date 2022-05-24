#include "helpers.h"

int float_direction(float value)
{
  int direction;

  if (value > 0.0f) {
    direction = 1;
  } else if (value < 0.0f) {
    direction = -1;
  } else {
    direction = 0;
  }

  return direction;
}

SVec2i helper_vector_direction(SVec2f vector)
{
  return (SVec2i) {
    float_direction(vector.x),
    float_direction(vector.y)
  };
}
