#include "image.h"
#include "test.h"
#include "performance.h"

namespace array {

template <typename T>
T pattern(int x, int y, int c) {
  return static_cast<T>(y * 10000 + x * 1000 + c);
}

template <typename T, typename Shape>
void fill_pattern(array<T, Shape>& a) {
  for_all_indices(a.shape(), [&](int x, int y, int c) {
    a(x, y, c) = pattern<T>(x, y, c);
  });
}

template <typename T, typename Shape>
void check_pattern(const array_ref<T, Shape>& a, int dx = 0, int dy = 0) {
  for_all_indices(a.shape(), [&](int x, int y, int c) {
    ASSERT_EQ(a(x, y, c), pattern<T>(x + dx, y + dy, c));
  });
}

template <typename T, typename Shape>
void check_pattern(const array<T, Shape>& a, int dx = 0, int dy = 0) {
  check_pattern(a.ref(), dx, dy);
}

template <typename Shape>
void test_crop() {
  array<int, Shape> base({100, 80, 3});
  fill_pattern(base);

  auto crop_xy = crop(base, 2, 1, 96, 77, crop_origin::crop);
  check_pattern(crop_xy);

  auto crop_zero_xy = crop(base, 3, 2, 95, 76, crop_origin::zero);
  check_pattern(crop_zero_xy, 3, 2);

  auto crop2_xy = crop(crop_xy, 4, 3, 92, 73, crop_origin::crop);
  check_pattern(crop2_xy);
}

TEST(image_crop) {
  test_crop<planar_image_shape>();
  test_crop<chunky_image_shape<3>>();
}


template <typename T, typename ShapeSrc, typename ShapeDest>
void test_copy(index_t channels) {
  array<T, ShapeSrc> src({40, 30, channels});
  fill_pattern(src);

  array<T, ShapeDest> dest({src.width(), src.height(), channels});
  copy(src, dest);
  check_pattern(dest);

  array<T, ShapeDest> dest_cropped({{5, 30}, {3, 20}, channels});
  copy(src, dest);
  check_pattern(dest);

  // If the src and dest shapes are the same, we should expect copies
  // to be about as fast as memcpy, even if the dest is cropped (so
  // some cleverness is required to copy rows at a time).
  // TODO: It would be nice if this were fast even for different shapes.
  // This may be impossible on x86. On ARM, using vstN/vldN, it may
  // be possible, but we need to find a way to sort dimensions by stride
  // while preserving the compile time constant extent.
#if 0
  if (std::is_same<ShapeSrc, ShapeDest>::value) {
    double copy_time = benchmark([&]() {
      copy(src, dest_cropped);
    });
    check_pattern(dest_cropped);

    array<T, ShapeDest> dest_memcpy(dest_cropped.shape());
    double memcpy_time = benchmark([&]() {
      memcpy(dest_memcpy.data(), src.data(), dest_cropped.size() * sizeof(T));
    });
    // This memcpy is *not* correct, but the performance of it
    // is optimistic.
    ASSERT(dest_memcpy != dest_cropped);

    ASSERT_LT(copy_time, memcpy_time * 1.2);
  }
#endif
}

template <typename ShapeSrc, typename ShapeDest>
void test_copy_all_types(index_t channels) {
  test_copy<int32_t, ShapeSrc, ShapeDest>(channels);
  test_copy<int16_t, ShapeSrc, ShapeDest>(channels);
  test_copy<int8_t, ShapeSrc, ShapeDest>(channels);
}

TEST(image_chunky_copy) {
  test_copy_all_types<chunky_image_shape<1>, chunky_image_shape<1>>(1);
  test_copy_all_types<chunky_image_shape<2>, chunky_image_shape<2>>(2);
  test_copy_all_types<chunky_image_shape<3>, chunky_image_shape<3>>(3);
  test_copy_all_types<chunky_image_shape<4>, chunky_image_shape<4>>(4);
}

TEST(image_planar_copy) {
  for (int i = 1; i <= 5; i++) {
    test_copy_all_types<planar_image_shape, planar_image_shape>(i);
  }
}

// Copying from planar to chunky is an "interleaving" operation.
TEST(image_interleave) {
  test_copy_all_types<planar_image_shape, chunky_image_shape<1>>(1);
  test_copy_all_types<planar_image_shape, chunky_image_shape<2>>(2);
  test_copy_all_types<planar_image_shape, chunky_image_shape<3>>(3);
  test_copy_all_types<planar_image_shape, chunky_image_shape<4>>(4);
}

// Copying from chunky to planar is a "deinterleaving" operation.
TEST(image_deinterleave) {
  test_copy_all_types<chunky_image_shape<1>, planar_image_shape>(1);
  test_copy_all_types<chunky_image_shape<2>, planar_image_shape>(2);
  test_copy_all_types<chunky_image_shape<3>, planar_image_shape>(3);
  test_copy_all_types<chunky_image_shape<4>, planar_image_shape>(4);
}

}  // namespace array