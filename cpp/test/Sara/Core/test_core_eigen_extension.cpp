// ========================================================================== //
// This file is part of Sara, a basic set of libraries in C++ for computer
// vision.
//
// Copyright (C) 2013-2016 David Ok <david.ok8@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.
// ========================================================================== //

#define BOOST_TEST_MODULE "Core/Extension of Eigen"

#include <boost/test/unit_test.hpp>

#include <DO/Sara/Core/EigenExtension.hpp>
#include <DO/Sara/Core/MultiArray.hpp>
#include <DO/Sara/Core/Tensor.hpp>


using namespace DO::Sara;
using namespace std;


BOOST_AUTO_TEST_SUITE(TestMultiArrayWithEigenExtension)

BOOST_AUTO_TEST_CASE(test_arithmetic_operation_with_nonscalar_element)
{
  using MatField = MultiArray<Matrix2f, 2>;

  const auto a = (Matrix2f{} << 1, 2, 3, 4).finished();

  const auto b = (Matrix2f{} << 1, 1, 2, 3).finished();

  auto m = MatField{2, 2};
  auto n = MatField{2, 2};

  // Initialize the matrices m and n.
  m.flat_array().fill(a);
  n.flat_array().fill(b);

  for (auto i = 0; i < m.rows(); ++i)
    for (auto j = 0; j < m.cols(); ++j)
      BOOST_CHECK(m(i, j) == a);

  for (auto i = 0; i < n.rows(); ++i)
    for (auto j = 0; j < n.cols(); ++j)
      BOOST_CHECK(n(i, j) == b);


  // Double that matrix
  m.flat_array() += n.flat_array();
  // Check that matrix
  for (auto i = 0; i < m.rows(); ++i)
    for (auto j = 0; j < m.cols(); ++j)
      BOOST_CHECK(m(i, j) == (a + b).eval());

  BOOST_CHECK(m(0, 0) * n(0, 0) == (a + b) * b);

  // Double that matrix
  m.flat_array() *= n.flat_array();
  // Check that matrix
  for (auto i = 0; i < m.rows(); ++i)
    for (auto j = 0; j < m.cols(); ++j)
      BOOST_CHECK(m(i, j) == (a + b) * b);

  m.matrix() += n.matrix();
  (tensor_view(m).flat_array() * tensor_view(n).flat_array()) +
      tensor_view(n).flat_array() / tensor_view(m).flat_array();
}

BOOST_AUTO_TEST_SUITE_END()
