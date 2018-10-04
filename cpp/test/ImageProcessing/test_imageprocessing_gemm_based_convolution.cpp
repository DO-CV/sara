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

#define BOOST_TEST_MODULE "ImageProcessing/GEMM-based Convolution"

#include <DO/Sara/ImageProcessing/GemmBasedConvolution.hpp>

#include <boost/test/unit_test.hpp>

#include <iomanip>
#include <sstream>


using namespace std;
using namespace DO::Sara;


template <typename T>
void print_3d_array(const TensorView_<T, 3>& x)
{
  const auto max = x.flat_array().abs().maxCoeff();
  std::stringstream ss;
  ss << max;
  const auto pad_size = ss.str().size();


  cout << "[";
  for (auto i = 0; i < x.size(0); ++i)
  {
    cout << "[";
    for (auto j = 0; j < x.size(1); ++j)
    {
      cout << "[";
      for (auto k = 0; k < x.size(2); ++k)
      {
        cout << std::setw(pad_size) << x(i,j,k);
        if (k != x.size(2) - 1)
          cout << ", ";
      }
      cout << "]";

      if (j != x.size(1) - 1)
        cout << ", ";
      else
        cout << "]";
    }

    if (i != x.size(0) - 1)
      cout << ",\n ";
  }
  cout << "]" << endl;
}

template <typename T>
void print_3d_array(const TensorView_<float, 3>& x)
{
  cout << "[";
  for (auto i = 0; i < x.size(0); ++i)
  {
    cout << "[";
    for (auto j = 0; j < x.size(1); ++j)
    {
      cout << "[";
      for (auto k = 0; k < x.size(2); ++k)
      {
        cout << fixed << x(i,j,k);
        if (k != x.size(2) - 1)
          cout << ", ";
      }
      cout << "]";

      if (j != x.size(1) - 1)
        cout << ", ";
      else
        cout << "]";
    }

    if (i != x.size(0) - 1)
      cout << ",\n ";
  }
  cout << "]" << endl;
}


BOOST_AUTO_TEST_CASE(test_im2col_on_nhw_tensor)
{
  constexpr auto N = 3;
  constexpr auto H = 4;
  constexpr auto W = 3;
  auto x = Tensor_<float, 3>{{N, H, W}};

  auto plane = Tensor_<float, 2>{{H, 3}};
  plane.matrix() <<
    0, 1, 2,
    3, 4, 5,
    6, 7, 8,
    9,10,11;

  x.flat_array() <<            //
      1 * plane.flat_array(),  //
      2 * plane.flat_array(),  //
      3 * plane.flat_array();

  constexpr auto kH = 3;
  constexpr auto kW = 3;

  // Apply im2col on each data of the batch.
  auto im2col_iterated = Tensor_<float, 2>{{N * H * W, kH * kW}};
  auto im2col_out_as_3d =
    im2col_iterated.reshape(Vector3i{N, H * W, kH * kW});

  im2col_out_as_3d[0] = im2col(x[0], {kH, kW});
  im2col_out_as_3d[1] = im2col(x[1], {kH, kW});
  im2col_out_as_3d[2] = im2col(x[2], {kH, kW});

  // Apply im2col on the whole batch.
  auto im2col_batched = im2col(x, {1, kH, kW});

  BOOST_CHECK(im2col_iterated.sizes() == im2col_batched.sizes());
  BOOST_CHECK(im2col_iterated.matrix() == im2col_batched.matrix());

  // Check the reshaped im2col
  auto sizes_5d = Matrix<int, 5, 1>{};
  sizes_5d << N, H, W, kH, kW;
  auto phi_x_as_5d = im2col_batched.reshape(sizes_5d);

  MatrixXf true_neighborhood(kH, kW);
  true_neighborhood <<
    0, 0, 0,
    1, 2, 0,
    4, 5, 0;
  //                      n  y  x
  BOOST_CHECK(phi_x_as_5d[0][0][2].matrix() == true_neighborhood);
  //cout << phi_x_as_5d[0][0][2].matrix() << endl << endl;

  true_neighborhood <<
    0, 0, 0,
    0, 0, 2,
    0, 6, 8;
  BOOST_CHECK(phi_x_as_5d[1][0][0].matrix() == true_neighborhood);
  //cout << phi_x_as_5d[1][0][0].matrix() << endl << endl;

  true_neighborhood <<
    2 * 3, 2 * 4, 2 * 5,
      2 * 6, 2 * 7, 2 * 8,
      2 * 9, 2 *10, 2 *11;
  BOOST_CHECK(phi_x_as_5d[1][2][1].matrix() == true_neighborhood);
  //cout << phi_x_as_5d[1][2][1].matrix() << endl << endl;

  true_neighborhood <<
    3 * 3, 3 * 4, 3 * 5,
      3 * 6, 3 * 7, 3 * 8,
      3 * 9, 3 *10, 3 *11;
  BOOST_CHECK(phi_x_as_5d[2][2][1].matrix() == true_neighborhood);
  //cout << phi_x_as_5d[2][2][1].matrix() << endl << endl;
}

BOOST_AUTO_TEST_CASE(test_im2col_on_nhwc_tensor)
{
  constexpr auto N = 1;
  constexpr auto H = 4;
  constexpr auto W = 3;
  constexpr auto C = 3;
  auto x = Tensor_<float, 4>{{N, H, W, C}};

  x[0].flat_array() <<
    0,0,0,  1, 1, 1,  2, 2, 2,
    3,3,3,  4, 4, 4,  5, 5, 5,
    6,6,6,  7, 7, 7,  8, 8, 8,
    9,9,9, 10,10,10, 11,11,11;

  for (int i = 1; i < N; ++i)
    x[i].flat_array() = x[0].flat_array(); //(i + 1) * x[i - 1].flat_array();

  //print_3d_array(x[0]);

  constexpr auto kH = 3;
  constexpr auto kW = 3;
  constexpr auto kC = 3;

  // Apply im2col on the whole batch.
  //                             kernel_sizes     strides
  //                              N  H   W   C
  // Each row (y, x) describes a patch centered at (y, x) with sizes (3, 3, 3).
  //
  // 3D patch centered at (n=0, y=1, x=1).
  // ROW 0
  // [[(y-1, x-1, c-1), (y-1, x-1, c), (y-1, x-1, c+1)],
  //  [(y-1, x  , c-1), (y-1, x  , c), (y-1, x  , c+1)],
  //  [(y-1, x+1, c-1), (y-1, x+1, c), (y-1, x+1, c+1)]],
  //
  // ROW 1
  // [[(y  , x-1, c-1), (y  , x-1, c), (y  , x-1, c+1)],
  //  [(y  , x  , c-1), (y  , x  , c), (y  , x  , c+1)],
  //  [(y  , x+1, c-1), (y  , x+1, c), (y  , x+1, c+1)]],
  //
  // ROW 2
  // [[(y  , x-1, c-1), (y  , x-1, c), (y  , x-1, c+1)],
  //  [(y  , x  , c-1), (y  , x  , c), (y  , x  , c+1)],
  //  [(y  , x+1, c-1), (y  , x+1, c), (y  , x+1, c+1)]],
  //
  // Adding the non-zero shift (0, 0, 1) in im2col will do
  // ROW 0
  // [[(y-1, x-1, c), (y-1, x-1, c+1), (y-1, x-1, c+2)],
  //  [(y-1, x  , c), (y-1, x  , c+1), (y-1, x  , c+2)],
  //  [(y-1, x+1, c), (y-1, x+1, c+1), (y-1, x+1, c+2)]],
  //
  // ROW 1
  // [[(y  , x-1, c), (y  , x-1, c+1), (y  , x-1, c+2)],
  //  [(y  , x  , c), (y  , x  , c+1), (y  , x  , c+2)],
  //  [(y  , x+1, c), (y  , x+1, c+1), (y  , x+1, c+2)]],
  //
  // ROW 2
  // [[(y  , x-1, c), (y  , x-1, c+1), (y  , x-1, c+2)],
  //  [(y  , x  , c), (y  , x  , c+1), (y  , x  , c+2)],
  //  [(y  , x+1, c), (y  , x+1, c+1), (y  , x+1, c+2)]],
  auto phi_x = im2col(x, {1, kH, kW, kC}, {1, 1, 1, 3}, {0, 0, 0, 1});
  BOOST_CHECK(phi_x.sizes() == Vector2i(N * H * W, kH * kW * kC));

  auto sizes_6d = Matrix<int, 6, 1>{};
  sizes_6d << N, H, W, kH, kW, kC;
  auto phi_x_as_6d = phi_x.reshape(sizes_6d);

  auto true_neighborhood = Tensor_<float, 3>(kH, kW, kC);
  true_neighborhood.flat_array() <<
    0,0,0, 1,1,1, 2,2,2,
    3,3,3, 4,4,4, 5,5,5,
    6,6,6, 7,7,7, 8,8,8;
  BOOST_CHECK(phi_x_as_6d[0][1][1] == true_neighborhood);
  //print_3d_array(phi_x_as_6d[0][1][1]);

  true_neighborhood.flat_array() <<
    0,0,0, 0,0,0, 0,0,0,
    0,0,0, 0,0,0, 1,1,1,
    0,0,0, 3,3,3, 4,4,4;
  BOOST_CHECK(phi_x_as_6d[0][0][0] == true_neighborhood);
  //print_3d_array(phi_x_as_6d[0][0][0]);
}

BOOST_AUTO_TEST_CASE(test_convolve_on_nhwc_tensor)
{
  constexpr auto N = 1;
  constexpr auto H = 4;
  constexpr auto W = 3;
  constexpr auto C = 3;
  auto x = Tensor_<float, 4>{{N, H, W, C}};

  x[0].flat_array() <<
    0,0,0,   1, 1, 1,   2, 2, 2,
    3,3,3,   4, 4, 4,   5, 5, 5,
    6,6,6,   7, 7, 7,   8, 8, 8,
    9,9,9,  10,10,10,  11,11,11;

  for (int i = 1; i < N; ++i)
    x[i].flat_array() = x[0].flat_array();

  constexpr auto kH = 3;
  constexpr auto kW = 3;
  constexpr auto kC = 3;

  auto phi_x = im2col(x, {1, kH, kW, kC}, {1, 1, 1, kC}, {0, 0, 0, 1});

  //                   kH x kW x kI  kO
  Tensor_<float, 2> k{{ 3 *  3 *  3,  3}};

  // Average on the R channel.
  k.matrix().col(0) <<
    1, 0, 0,  1, 0, 0,  1, 0, 0,
    1, 0, 0,  1, 0, 0,  1, 0, 0,
    1, 0, 0,  1, 0, 0,  1, 0, 0;

  // Average on the G channel.
  k.matrix().col(1) <<
    0, 1, 0,  0, 1, 0,  0, 1, 0,
    0, 1, 0,  0, 1, 0,  0, 1, 0,
    0, 1, 0,  0, 1, 0,  0, 1, 0;

  // Average on the B channel.
  k.matrix().col(2) <<
    0, 0, 1,  0, 0, 1,  0, 0, 1,
    0, 0, 1,  0, 0, 1,  0, 0, 1,
    0, 0, 1,  0, 0, 1,  0, 0, 1;

  auto y = Tensor_<float, 4>{{N, C, H, W}};
  y.flat_array() = (phi_x.matrix() * k.matrix()).array();

  /*
     0,0,0,   1, 1, 1,   2, 2, 2,
     3,3,3,   4, 4, 4,   5, 5, 5,
     6,6,6,   7, 7, 7,   8, 8, 8,
     9,9,9,  10,10,10,  11,11,11;
     */
  MatrixXf true_plane{H, W};
  true_plane.matrix() <<
    0+0+0 + 0+0+1 + 0+3+4, 0+0+0 + 0+1+2 + 3+4+5, 0+0+0 + 1+2+0 + 4+5+0,
    0+0+1 + 0+3+4 + 0+6+7, 0+1+2 + 3+4+5 + 6+7+8, 1+2+0 + 4+5+0 + 7+8+0,
    0+3+4 + 0+6+7 + 0+9+10, 3+4+5 + 6+7+8 + 9+10+11, 4+5+0 + 7+8+0 + 10+11+0,
    0+6+7 + 0+9+10 + 0+0+0, 6+7+8 + 9+10+11 + 0+0+0, 7+8+0 + 10+11+0 + 0+0+0;

  // Check each plane value.
  BOOST_CHECK(y[0][0].matrix() == true_plane);
  BOOST_CHECK(y[0][1].matrix() == true_plane);
  BOOST_CHECK(y[0][2].matrix() == true_plane);

  //auto yt = y.transpose({0, 2, 3, 1});
  //print_3d_array(y[0]);
  //print_3d_array(yt[0]);
}

BOOST_AUTO_TEST_CASE(test_convolve_on_nchw_tensor)
{
  constexpr auto N = 1;
  constexpr auto H = 4;
  constexpr auto W = 3;
  constexpr auto C = 3;
  auto x = Tensor_<float, 4>{{N, C, H, W}};

  x[0].flat_array() <<
    0,   1,   2,
    3,   4,   5,
    6,   7,   8,
    9,  10,  11;

  for (int i = 1; i < C; ++i)
    x[0][i].flat_array() = x[0][0].flat_array();


  constexpr auto kH = 3;
  constexpr auto kW = 3;
  constexpr auto kC = 3;


  auto phi_x = im2col(x, {N, kC, kH, kW}, {1, kC, 1, 1}, {0, 1, 0, 0});
  // [N * C/kC * H/kH * W/kW, kC * kH * kW]
  // cout << phi_x.matrix() << endl;

  //                   kC x kH x kW  kO
  Tensor_<float, 2> k{{ 3 *  3 *  3,  3}};
  k.matrix().col(0) << VectorXf::Ones(9), VectorXf::Zero(9), VectorXf::Zero(9);
  k.matrix().col(1) << VectorXf::Zero(9), VectorXf::Ones(9), VectorXf::Zero(9);
  k.matrix().col(2) << VectorXf::Zero(9), VectorXf::Zero(9), VectorXf::Ones(9);

  //cout << "k = " << k.matrix().rows() << " " << k.matrix().cols()  << endl;
  //cout << k.matrix()  << endl;

  auto y = Tensor_<float, 4>{{N, C, H, W}};
  y.flat_array() = (phi_x.matrix() * k.matrix()).array();

  MatrixXf true_plane{H, W};
  true_plane.matrix() <<
    0+0+0 + 0+0+1 + 0+3+4, 0+0+0 + 0+1+2 + 3+4+5, 0+0+0 + 1+2+0 + 4+5+0,
    0+0+1 + 0+3+4 + 0+6+7, 0+1+2 + 3+4+5 + 6+7+8, 1+2+0 + 4+5+0 + 7+8+0,
    0+3+4 + 0+6+7 + 0+9+10, 3+4+5 + 6+7+8 + 9+10+11, 4+5+0 + 7+8+0 + 10+11+0,
    0+6+7 + 0+9+10 + 0+0+0, 6+7+8 + 9+10+11 + 0+0+0, 7+8+0 + 10+11+0 + 0+0+0;

  // Check each plane value.
  BOOST_CHECK(y[0][0].matrix() == true_plane);
  BOOST_CHECK(y[0][1].matrix() == true_plane);
  BOOST_CHECK(y[0][2].matrix() == true_plane);

  //y = y.transpose(Vector4i{0, 2, 3, 1});
  //print_3d_array(y[0]);
}

auto square_toeplitz(const std::vector<float>& a)
  -> MatrixXf
{
  const int m = a.size() / 2 + 1;
  const int n = a.size() / 2 + 1;
  MatrixXf A = MatrixXf::Zero(m, n);
  for (int i = 0; i < m; ++i)
    for (int j = 0; j < n; ++j)
    {
      A(i, j) = a[i - j + n - 1];
    }
  return A;
}

auto causal_toeplitz_coeff(const MatrixXf& kernel,  //
                           int h,
                           int w)  // Upper part is zero everywhere
  -> std::vector<float>
{
  auto kh = kernel.rows();
  auto kw = kernel.cols();

  auto a = std::vector<float>(h * w, 0.f);
  for (int y = 0; y < kh; ++y)
    for (int x = 0; x < kw; ++x)
      a[y*w + x] = kernel(y, x);

  return a;
}

auto dense_causal_toeplitz(const std::vector<float>& t, int m, int n)
  -> MatrixXf
{
  MatrixXf T = MatrixXf::Zero(m, n);
  for (int i = 0; i < m; ++i)
    for (int j = 0; j <= i; ++j)
      T(i, j) = t[i - j];

  return T;
}

template <typename DstArrayView, typename SrcArrayView>
void safe_crop_generic(DstArrayView& dst, const SrcArrayView& src,
                       const typename SrcArrayView::vector_type& begin,
                       const typename SrcArrayView::vector_type& end)
{
  if (dst.sizes() != end - begin)
    throw std::domain_error{"Invalid destination sizes!"};

  auto src_i = src.begin_subarray(begin, end);

  for (auto dst_i = dst.begin(); dst_i != dst.end(); ++src_i, ++dst_i)
    *dst_i = *src_i;
}

BOOST_AUTO_TEST_CASE(test_toeplitz_matrix_construction)
{
  const std::vector<float> a = {-4, -3, -2, -1, 0, 1, 2, 3, 4};
  const auto A = square_toeplitz(a);

  const auto h = 3;
  const auto w = 3;

  const auto kh = 2;
  const auto kw = 2;

  const auto padded_h = 2 * (kh - 1) + h;
  const auto padded_w = 2 * (kw - 1) + w;

  const auto f = 2;
  const auto nh = h * f;
  const auto nw = w * f;

  MatrixXf K = MatrixXf::Ones(kh, kw);
  std::cout << "K =\n" << K << std::endl;

  auto Tk_coeff = causal_toeplitz_coeff(K, padded_h, padded_w);
  std::cout << "sparse_toeplitz(K) =\n" << std::endl;
  for (const auto t : Tk_coeff)
    std::cout << t << " ";
  std::cout << std::endl;

  auto Tk = dense_causal_toeplitz(Tk_coeff, padded_h * padded_w, nh * nw);
  std::cout << "Dense Toeplitz(k) =" << std::endl;
  std::cout << Tk << std::endl;

  Tk /= kh * kw;

  Tensor_<float, 2> x(h, w);
  x.matrix() <<
    1, 2, 3,
    4, 5, 6,
    7, 8, 9;
  std::cout << "x =\n" << x.matrix() << std::endl;

  //auto infx = make_infinite(x, make_constant_padding(0.f));
  //auto beg = Vector2i{-kh + 1, -kw + 1};
  //auto end = Vector2i{h + kh, w + kw};
  //auto padded_x =
  //    Tensor_<float, 2>{infx.begin_stepped_subarray(beg, end, Vector2i::Ones())
  //                          .stepped_subarray_sizes()};
  //safe_crop_generic(padded_x, infx, beg, end);
  //std::cout << "padded_x =\n" << padded_x.matrix() << std::endl;
  // TODO: create special padding.
  Tensor_<float, 2> padded_x{h + (kh - 1) * 2, w + (kw - 1) * 2};
  padded_x.matrix() <<
    1, 1, 2, 3, 3,
    1, 1, 2, 3, 3,
    4, 4, 5, 6, 6,
    7, 7, 8, 9, 9,
    7, 7, 8, 9, 9;
  std::cout << "padded_x =\n" << padded_x.matrix() << std::endl;


  //auto vec_x = vec(padded_x);
  //std::cout << "vec_x =\n" << vec_x << std::endl;

  //RowVectorXf y = vec_x.transpose() * Tk;
  //std::cout << "y=\n" << y << std::endl;

  //Map<MatrixXf> Y(y.data(), nh, nw);
  //std::cout << "Y=\n" << Y << std::endl;
}

BOOST_AUTO_TEST_CASE(test_transpose_convolution_naive)
{
  MatrixXf x(2, 2);
  x <<
    1, 2,
    3, 4;

  MatrixXf px(3, 3);
  px <<
    1, 2, 2,
    3, 4, 4,
    3, 4, 4;

  const auto kh = 2;
  const auto kw = 2;
  auto k_base = Tensor_<float, 4>{Vector4i{kh, kw, kh, kw}};
  k_base[0][0].matrix() <<
    1, 0,
    0, 0;
  k_base[0][1].matrix() <<
    0.5, 0.5,
    0.0, 0.0;
  k_base[1][0].matrix() <<
    0.5, 0.0,
    0.5, 0.0;
  k_base[1][1].matrix() <<
    0.25, 0.25,
    0.25, 0.25;


  auto k_base_padded_shape = Vector4i{kh, kw, px.rows(), px.cols()};
  auto k_base_padded = Tensor_<float, 4>{k_base_padded_shape};
  for (auto i = 0; i < kh; ++i)
    for (auto j = 0; j < kw; ++j)
      k_base_padded[i][j].matrix().bottomLeftCorner(kh, kw) =
          k_base[i][j].matrix();


  MatrixXf true_y(kh * x.rows(), kw * x.cols());
  true_y <<
    1.0, 1.5, 2.0, 2.0,
    2.0, 2.5, 3.0, 3.0,
    3.0, 3.5, 4.0, 4.0,
    3.0, 3.5, 4.0, 4.0;

  // Manual construction of the kernel.
  //
  // MatrixXf k(h_k, w_k);
  // k <<
  // //r=0
  //   1.00, 0.00, 0.00,   0.00, 0.00, 0.00,   0.00, 0.00, 0.00,  // 1.
  //   0.50, 0.50, 0.00,   0.00, 0.00, 0.00,   0.00, 0.00, 0.00,  // 1.5
  //   0.00, 1.00, 0.00,   0.00, 0.00, 0.00,   0.00, 0.00, 0.00,  // 2.
  //   0.00, 0.50, 0.50,   0.00, 0.00, 0.00,   0.00, 0.00, 0.00,  // 2.
  // //r=1
  //   0.50, 0.00, 0.00,   0.50, 0.00, 0.00,   0.00, 0.00, 0.00,  // 2.
  //   0.25, 0.25, 0.00,   0.25, 0.25, 0.00,   0.00, 0.00, 0.00,  // 2.5
  //   0.00, 0.50, 0.00,   0.00, 0.50, 0.00,   0.00, 0.00, 0.00,  // 3.
  //   0.00, 0.25, 0.25,   0.00, 0.25, 0.25,   0.00, 0.00, 0.00,  // 2.5

  // //r=2
  //   0.00, 0.00, 0.00,   1.00, 0.00, 0.00,   0.00, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.50, 0.50, 0.00,   0.00, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 1.00, 0.00,   0.00, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 0.50, 0.50,   0.00, 0.00, 0.00,
  // //r=3
  //   0.00, 0.00, 0.00,   0.50, 0.00, 0.00,   0.50, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.25, 0.25, 0.00,   0.25, 0.25, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 0.50, 0.00,   0.00, 0.50, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 0.25, 0.25,   0.00, 0.25, 0.25;

  // Better way of constructing the upsampling kernel.
  auto K_shape = Vector4i{kh * x.rows(), kw * x.cols(), px.rows(), px.cols()};
  auto K = Tensor_<float, 4>{K_shape};
  K.flat_array().fill(0);
  // Whole kernel.
  for (int i = 0; i < kh * x.rows(); ++i)
  {
    for (int j = 0; j < kw * x.cols(); ++j)
    {
      K[i][j].matrix().block(i / kh, j / kw, kh, kw) =
          k_base[i % kh][j % kw].matrix();
    }
  }

  // Now the problem with the expanded kernel above is that it may take too much
  // space. So we need to only multiply the kernel k_base_padded with the
  // appropriate row.
  // Easily parallelisable in CUDA and with OpenMP.
  // y[kw * yw * y + kw * x, kw * yw * y + kw * w + kw] =
  //  k_base_padded.matrix() * vec(padded_x.block(y, x, px.rows(), px.cols()))

  auto h_K = kh * x.rows() * kw * x.cols();
  auto w_K = px.rows() * px.cols();

  auto k = K.reshape(Vector2i{h_K, w_K}).matrix();
  std::cout << k << std::endl;


  // TODO: For the transpose convolution.
  // - we need an offset parameter to place correctly the kernel. here the
  // offset is 0.
  // - we may need a stride parameter.
  MatrixXf out;
  out = k * Map<MatrixXf>(px.data(), px.rows() * px.cols(), 1);

  auto out_reshaped = Map<MatrixXf>(out.data(), kh * x.rows(), kw * x.cols());
  std::cout << "out_reshaped=\n" << out_reshaped << std::endl;

}

BOOST_AUTO_TEST_CASE(test_transpose_convolution_optimized)
{
  MatrixXf x(2, 2);
  x <<
    1, 2,
    3, 4;

  MatrixXf px(3, 3);
  px <<
    1, 2, 2,
    3, 4, 4,
    3, 4, 4;

  const auto kh = 2;
  const auto kw = 2;
  auto k_base = Tensor_<float, 4>{Vector4i{kh, kw, kh, kw}};
  k_base[0][0].matrix() <<
    1, 0,
    0, 0;
  k_base[0][1].matrix() <<
    0.5, 0.5,
    0.0, 0.0;
  k_base[1][0].matrix() <<
    0.5, 0.0,
    0.5, 0.0;
  k_base[1][1].matrix() <<
    0.25, 0.25,
    0.25, 0.25;


  auto k_base_padded_shape = Vector4i{kh, kw, px.rows(), px.cols()};
  auto k_base_padded = Tensor_<float, 4>{k_base_padded_shape};
  for (auto i = 0; i < kh; ++i)
    for (auto j = 0; j < kw; ++j)
      k_base_padded[i][j].matrix().bottomLeftCorner(kh, kw) =
          k_base[i][j].matrix();


  MatrixXf true_y(kh * x.rows(), kw * x.cols());
  true_y <<
    1.0, 1.5, 2.0, 2.0,
    2.0, 2.5, 3.0, 3.0,
    3.0, 3.5, 4.0, 4.0,
    3.0, 3.5, 4.0, 4.0;

  // Manual construction of the kernel.
  //
  // MatrixXf k(h_k, w_k);
  // k <<
  //
  // |-----------------------------------------|
  // | //r=0                                   |
  // |   1.00, 0.00, 0.00,   0.00, 0.00, 0.00, |  0.00, 0.00, 0.00,  // 1.
  // |   0.50, 0.50, 0.00,   0.00, 0.00, 0.00, |  0.00, 0.00, 0.00,  // 1.5
  // |   0.00, 1.00, 0.00,   0.00, 0.00, 0.00, |  0.00, 0.00, 0.00,  // 2.
  // |   0.00, 0.50, 0.50,   0.00, 0.00, 0.00, |  0.00, 0.00, 0.00,  // 2.
  // | //r=1                                   |
  // |   0.50, 0.00, 0.00,   0.50, 0.00, 0.00, |  0.00, 0.00, 0.00,  // 2.
  // |   0.25, 0.25, 0.00,   0.25, 0.25, 0.00, |  0.00, 0.00, 0.00,  // 2.5
  // |   0.00, 0.50, 0.00,   0.00, 0.50, 0.00, |  0.00, 0.00, 0.00,  // 3.
  // |   0.00, 0.25, 0.25,   0.00, 0.25, 0.25, |  0.00, 0.00, 0.00,  // 2.5
  // |-----------------------------------------|
  //
  // //r=2
  //   0.00, 0.00, 0.00,   1.00, 0.00, 0.00,   0.00, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.50, 0.50, 0.00,   0.00, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 1.00, 0.00,   0.00, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 0.50, 0.50,   0.00, 0.00, 0.00,
  // //r=3
  //   0.00, 0.00, 0.00,   0.50, 0.00, 0.00,   0.50, 0.00, 0.00,
  //   0.00, 0.00, 0.00,   0.25, 0.25, 0.00,   0.25, 0.25, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 0.50, 0.00,   0.00, 0.50, 0.00,
  //   0.00, 0.00, 0.00,   0.00, 0.25, 0.25,   0.00, 0.25, 0.25;

  // Better way of constructing the upsampling kernel.
  auto K_block_shape = Vector2i{kh * px.rows(), kw * px.cols()};  // (2 * (h + pad_h), 2 * (w + pad_w))
  auto K_block = Tensor_<float, 2>{K_block_shape};

  // Now the problem with the expanded kernel above is that it may take too much
  // space. So we need to only multiply the kernel k_base_padded with the
  // appropriate row.
  // Easily parallelisable in CUDA and with OpenMP.
  // y[kw * yw * y + kw * x, kw * yw * y + kw * w + kw] =
  //  k_base_padded.matrix() * vec(padded_x.block(y, x, px.rows(), px.cols()))



}
