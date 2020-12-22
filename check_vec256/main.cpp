// STD
#include <iostream>
#include <chrono>
#include <thread>

// Torch
#include <ATen/ATen.h>
#include <ATen/cpu/vec256/vec256.h>


using namespace std::chrono_literals;


template <typename scalar_t, typename Op>
inline scalar_t vec_reduce_all(
    const Op& vec_fun,
    at::vec256::Vec256<scalar_t> acc_vec,
    int64_t size) {

  using Vec = at::vec256::Vec256<scalar_t>;
    
  auto sz = Vec::size();
  scalar_t acc_arr[sz];
  acc_vec.store(acc_arr, sz);

  for (int64_t i = 1; i < size; i++) {
    std::array<scalar_t, sz> acc_arr_next = {0};
    acc_arr_next[0] = acc_arr[i];
    Vec acc_vec_next = Vec::loadu(acc_arr_next.data());
    acc_vec = vec_fun(acc_vec, acc_vec_next);
  }

  acc_vec.store(acc_arr, sz);
  return acc_arr[0];
}


template <typename scalar_t, typename Op>
inline scalar_t reduce_all(const Op& vec_fun, const scalar_t* data, int64_t size) {

  using Vec = at::vec256::Vec256<scalar_t>;

  if (size < Vec::size())
    return vec_reduce_all(vec_fun, Vec::loadu(data, size), size);

  int64_t d = Vec::size();
  Vec acc_vec = Vec::loadu(data);
  for (; d < size - (size % Vec::size()); d += Vec::size()) {
    Vec data_vec = Vec::loadu(data + d);
    acc_vec = vec_fun(acc_vec, data_vec);
  }
  if (size - d > 0) {
    Vec data_vec = Vec::loadu(data + d, size - d);
    acc_vec = Vec::set(acc_vec, vec_fun(acc_vec, data_vec), size - d);
  }
  return vec_reduce_all(vec_fun, acc_vec, Vec::size());
}


inline at::Tensor prod(const at::Tensor & x, const at::Tensor & y) {
    using Vec = at::vec256::Vec256<float>;
    auto x_ptr = (float *) x.data_ptr();
    auto y_ptr = (float *) x.data_ptr();

    auto output = at::empty_like(x);
    auto out_ptr = (float *) output.data_ptr();

    auto size = x.numel();
    auto step = Vec::size();
    int64_t d = 0;
    Vec vec_x, vec_y, vec_output;
    for (; d < size - (size % step); d+=step) {
        vec_x = Vec::loadu(x_ptr + d, step);
        vec_y = Vec::loadu(y_ptr + d, step);
        vec_output = vec_x + vec_y;
        vec_output.store(out_ptr + d, step);
    }

    if (size - d > 0) {
        vec_x = Vec::loadu(x_ptr + d, size - d);
        vec_y = Vec::loadu(y_ptr + d, size - d);
        vec_output = vec_x * vec_y;
        vec_output.store(out_ptr + d, size - d);
    }
    return output;
}


inline at::Tensor prod_interleaved(const at::Tensor & x, const at::Tensor & y) {
    using Vec = at::vec256::Vec256<float>;

    auto output = at::empty_like(x);

    auto x_ptr = (float *) x.data_ptr();
    auto y_ptr = (float *) y.data_ptr();
    auto output_ptr = (float *) output.data_ptr();

    auto size = x.numel();
    auto step = Vec::size();
    int64_t d = 0;
    Vec vec_x, vec_y, vec_output;
    for (; d < size - (size % step); d+=step) {
        vec_x = Vec::loadu(x_ptr + d, step);
        vec_y = Vec::loadu(y_ptr + d, step);
        vec_output = vec_x + vec_y;
        vec_output.store(out_ptr + d, step);
    }

    if (size - d > 0) {
        vec_x = Vec::loadu(x_ptr + d, size - d);
        vec_y = Vec::loadu(y_ptr + d, size - d);
        vec_output = vec_x * vec_y;
        vec_output.store(out_ptr + d, size - d);
    }

    return output;
}


int main(int argc, char** argv)
{

#if 0
    std::cout << "- at::vec256::Vec256<float>::size() = " << at::vec256::Vec256<float>::size() << std::endl;
    std::cout << "- at::vec256::Vec256<int>::size() = " << at::vec256::Vec256<int>::size() << std::endl;
    std::cout << "- at::vec256::Vec256<double>::size() = " << at::vec256::Vec256<double>::size() << std::endl;
    std::cout << "- at::vec256::Vec256<uint8_t>::size() = " << at::vec256::Vec256<uint8_t>::size() << std::endl;

    auto sz = at::vec256::Vec256<float>::size();
    float acc_arr[sz];
    at::vec256::Vec256<float> acc_vec {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::cout << "- before: " << acc_vec << std::endl;
    for (int i=0; i<sz; i++)
        std::cout << acc_arr[i] << " ";
    std::cout << std::endl;
    acc_vec.store(acc_arr, sz);
    std::cout << "- after: " << acc_vec << std::endl;
    for (int i=0; i<sz; i++)
        std::cout << acc_arr[i] << " ";
    std::cout << std::endl;

#endif

    int64_t size = 10000;
    auto x = at::arange(0, size, at::CPU(at::kFloat));
    auto y = at::arange(0, size, at::CPU(at::kFloat));
    
    auto z = sum(x, y);
    assert (z.allclose(x + y));

    int n = 100000;
    {
        auto timer = std::chrono::steady_clock::now();
        for (int j=0; j<n; ++j)
        {
            prod(x, y);
        }

        auto end = std::chrono::steady_clock::now();
        auto duration = end - timer;
        std::cout << "- prod(x, y) took " << duration.count() / n
                  << " microseconds to complete." << std::endl;
    }
    // {
    //     auto timer = std::chrono::steady_clock::now();
    //     for (int j=0; j<n; ++j)
    //     {
    //         prod_interleaved(x, y);
    //     }

    //     auto end = std::chrono::steady_clock::now();
    //     auto duration = end - timer;
    //     std::cout << "- prod_interleaved(x, y) took " << duration.count() / n
    //               << " microseconds to complete." << std::endl;
    // }

#if 0
    auto t_input = at::arange(0, 100, at::CPU(at::kFloat));
    std::cout << t_input.dtype() << std::endl;
    std::cout << t_input.sum() << std::endl;

    using Vecf = at::vec256::Vec256<float>;

    auto res = reduce_all<float>(
        [](Vecf& x, Vecf& y) { return x + y; },
        (float *) t_input.data_ptr(),
        100
    );

    std::cout << res << std::endl;
#endif

    return 0;    
}