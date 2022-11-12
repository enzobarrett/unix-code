#include "TimeZones.h"
#include <stdio.h>

static double TimeZones::getNext() {
  if (index == length - 2)
    index = 0;

  return zones[++index];
}

static double TimeZones::getCurrent() {
  return zones[index];
}

static int TimeZones::index = 0;
const static double TimeZones::zones[] {
  -12, -11, -10, -9.5, -9, -8, -7, -6, -5, -4, -3.5, -3, -2, -1, 0, 1, 2, 3,
  3.5, 4, 4.5, 5, 5.5, 5.75, 6, 6.5, 7, 8, 8.75, 9, 9.5, 10, 10.5,
  11, 12, 12.75, 13, 14
};
const static int TimeZones::length = sizeof(TimeZones::zones) / sizeof(double);
