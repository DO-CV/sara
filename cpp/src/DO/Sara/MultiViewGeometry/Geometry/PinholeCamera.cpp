// ========================================================================== //
// This file is part of Sara, a basic set of libraries in C++ for computer
// vision.
//
// Copyright (C) 2019 David Ok <david.ok8@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.
// ========================================================================== //

#include <DO/Sara/MultiViewGeometry/Geometry/PinholeCamera.hpp>


namespace DO::Sara {

auto cheirality_predicate(const MatrixXd& X, const Matrix34d& P) -> bool
{
  return ((P * X).row(2).array() > 0).redux([](bool a, bool b) {
    return a && b;
  });
}

} /* namespace DO::Sara */