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

//! @file

#pragma once

#include <DO/Sara/Core/Image.hpp>


namespace DO { namespace Sara {

  //! @brief Centered difference.
  class Centered
  {
    template <typename PaddedMultiArray>
    static inline auto centered(const PaddedMultiArray& u,
                                const typename PaddedMultiArray::vector_type& p,
                                int i)
    {
      using T = typename PaddedMultiArray::value_type;
      using vector_type = typename PaddedMultiArray::vector_type;
      const auto ei = vector_type::unit(i);
      return (u(p + ei) - u(p - ei)) / 2;
    }

    template <typename PaddedMultiArray>
    static inline auto forward(const PaddedMultiArray& u,
                               const typename PaddedMultiArray::vector_type& p,
                               int i)
    {
      return centered(u, p, i);
    }

    template <typename PaddedMultiArray>
    static inline auto backward(const PaddedMultiArray& u,
                                const typename PaddedMultiArray::vector_type& p,
                                int i)
    {
      return centered(u, p, i);
    }
  };


  //! @brief Upwind difference.
  class Upwind
  {
  public:
    template <typename PaddedMultiArray>
    static inline auto forward(const PaddedMultiArray& u,
                               const typename PaddedMultiArray::vector_type& p,
                               int i)
    {
      using T = typename PaddedMultiArray::value_type;
      using vector_type = typename PaddedMultiArray::vector_type;
      const auto ei = vector_type::unit(i);
      return u(p + ei) - u(p);
    }

    template <typename PaddedMultiArray>
    static inline auto backward(const PaddedMultiArray& u,
                                const typename PaddedMultiArray::vector_type& p,
                                int i)
    {
      using T = typename PaddedMultiArray::value_type;
      using vector_type = typename PaddedMultiArray::vector_type;
      const vector_type ei = vector_type::unit(i);
      return u(p) - u(p - ei);
    }
  };


  template <typename T>
  inline auto sqr(T x)
  {
    return x * x;
  };

  //! @brief WENO3.
  struct Weno3
  {
    static constexpr auto eps = 1e-12;

    template <typename T>
    static inline T combine(const T v1, const T v2, const T v3)
    {
      auto s = (T(eps) + sqr(v2 - v1)) / (T(eps) + sqr(v3 - v2));
      s = 1 / (1 + 2 * s * s);
      return (v2 + v3 - s * (v1 - 2 * v2 + v3)) / 2;
    }

    template <typename PaddedMultiArray>
    static inline auto forward(const PaddedMultiArray& u,
                               const typename PaddedMultiArray::vector_type& p,
                               int i)
    {
      using vector_type = typename PaddedMultiArray::vector_type;
      const vector_type ei = vector_type::unit(i);

      const auto us = std::array{u(p - ei), u(p), u(p + ei), u(p + 2 * ei)};

      auto dus = decltype(us){};
      std::adjacent_difference(std::begin(us), std::end(us), std::begin(dus));

      return combine(dus[3], dus[2], dus[1]);
    }

    template <typename PaddedMultiArray>
    static inline auto backward(const PaddedMultiArray& u,
                                const typename PaddedMultiArray::vector_type& p,
                                int i)
    {
      using vector_type = typename PaddedMultiArray::vector_type;
      const vector_type ei = vector_type::unit(i);

      const auto us = std::array{u(p - 2 * ei), u(p - ei), u(p), u(p + ei)};

      auto dus = decltype(us){};
      std::adjacent_difference(std::begin(us), std::end(us), std::begin(dus));

      return combine(dus[1], dus[2], dus[3]);
    }
  };


  //! @brief WENO5.
  struct Weno5
  {
    static constexpr auto eps = 1e-12;

    template <typename T>
    static inline T combine(const T v1, const T v2, const T v3, const T v4,
                            const T v5)
    {
      T s1 = 13 * sqr(v1 - 2 * v2 + v3) / 12 + sqr(v1 - 4 * v2 + 3 * v3) / 4;
      T s2 = 13 * sqr(v2 - 2 * v3 + v4) / 12 + sqr(v2 - v4) / 4;
      T s3 = 13 * sqr(v3 - 2 * v4 + v5) / 12 + sqr(3 * v3 - 4 * v4 + v5) / 4;

      s1 = 1 / sqr(T(eps) + s1);
      s2 = 6 / sqr(T(eps) + s2);
      s3 = 3 / sqr(T(eps) + s3);

      return (s1 * (2 * v1 - 7 * v2 + 11 * v3) + s2 * (-v2 + 5 * v3 + 2 * v4) +
              s3 * (2 * v3 + 5 * v4 - v5)) /
             6 / (s1 + s2 + s3);
    }

    template <typename PaddedMultiArray>
    static inline auto forward(const PaddedMultiArray& u,
                               const typename PaddedMultiArray::vector_type& p,
                               int i)
    {
      using vector_type = typename PaddedMultiArray::vector_type;
      const vector_type ei = vector_type::unit(i);

      const auto us = std::array{u(p - 2 * ei), u(p - ei),     u(p),
                                 u(p + ei),     u(p + 2 * ei), u(p + 3 * ei)};
      auto dus = decltype(us){};
      std::adjacent_difference(std::begin(us), std::end(us), std::begin(dus));

      return combine(dus[5], dus[4], dus[3], dus[2], dus[1]);
    }

    template <typename PaddedMultiArray>
    static inline auto backward(const PaddedMultiArray& u,
                                const typename PaddedMultiArray::vector_type& p,
                                int i)
    {
      using vector_type = typename PaddedMultiArray::vector_type;
      const vector_type ei = vector_type::unit(i);

      const auto us = std::array{u(p - 3 * ei), u(p - 2 * ei), u(p - ei),
                                 u(p),          u(p + ei),     u(p + 2 * ei)};

      auto dus = decltype(us){};
      std::adjacent_difference(std::begin(us), std::end(us), std::begin(dus));

      return combine(dus[1], dus[2], dus[3], dus[4], dus[5]);
    }
  };

}  // namespace Sara
}  // namespace DO