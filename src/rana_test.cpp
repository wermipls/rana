// https://github.com/doctest/doctest/pull/1079
// We need this to avoid symbol collisions on Clang 22 and above.
#pragma clang diagnostic ignored "-Wc2y-extensions"
#define DOCTEST_COUNTER __COUNTER__

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

// all headers with tests.
#include "audio/halfband.hpp"
#include "containers/bitmask_ringbuf.hpp"
#include "containers/linear_ringbuf.hpp"
#include "serializer.hpp"
#include "fast_math/exp.hpp"
#include "fast_math/log.hpp"
#include "fast_math/pow.hpp"
