#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

#include "Utilities/DbUtils.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("dbToLinear: valores conhecidos", "[db]")
{
    CHECK_THAT (amp::dbToLinear (0.0), WithinAbs (1.0, 1e-12));
    CHECK_THAT (amp::dbToLinear (20.0), WithinAbs (10.0, 1e-9));
    CHECK_THAT (amp::dbToLinear (-20.0), WithinAbs (0.1, 1e-12));
    CHECK_THAT (amp::dbToLinear (-6.0206), WithinAbs (0.5, 1e-4));
}

TEST_CASE ("linearToDb: valores conhecidos", "[db]")
{
    CHECK_THAT (amp::linearToDb (1.0), WithinAbs (0.0, 1e-12));
    CHECK_THAT (amp::linearToDb (10.0), WithinAbs (20.0, 1e-9));
    CHECK_THAT (amp::linearToDb (0.5), WithinAbs (-6.0206, 1e-4));
}

TEST_CASE ("linearToDb: entradas invalidas nunca geram NaN/Inf", "[db]")
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    CHECK (amp::linearToDb (0.0) == -120.0);
    CHECK (amp::linearToDb (-1.0) == -120.0);
    CHECK (amp::linearToDb (nan) == -120.0);
    CHECK (std::isfinite (amp::linearToDb (1e-30)));
}

TEST_CASE ("ida e volta dB <-> linear", "[db]")
{
    for (double db = -60.0; db <= 24.0; db += 1.5)
        CHECK_THAT (amp::linearToDb (amp::dbToLinear (db)), WithinAbs (db, 1e-9));
}
