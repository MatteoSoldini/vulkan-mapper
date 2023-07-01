#include "../../include/clamp.h"

double clamp(double in, double lo, double hi) {
	if (in < lo) return lo;
	if (in > hi) return hi;
	return in;
}