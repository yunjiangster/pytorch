#pragma once
#ifdef __OBJC__
#include <Metal/Metal.h>
typedef id<MTLLibrary> MTLLibrary_t;
typedef id<MTLFunction> MTLFunction_t;
typedef id<MTLComputePipelineState> MTLComputePipelineState_t;
typedef id<MTLComputeCommandEncoder> MTLComputeCommandEncoder_t;
#else
typedef void MTLCompileOptions;
typedef void* MTLLibrary_t;
typedef void* MTLFunction_t;
typedef void* MTLComputePipelineState_t;
typedef void* MTLComputeCommandEncoder_t;
#endif

#include <functional>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

// Forward declaration of TensorBase
namespace at {
class TensorBase;
}

namespace at::native::mps {

namespace detail {
template <typename T>
class has_size_type {
  template <typename U>
  static constexpr std::true_type check(typename U::size_type*);
  template <typename>
  static constexpr std::false_type check(...);

 public:
  static constexpr bool value = decltype(check<T>(nullptr))::value;
};

template <typename T>
constexpr bool has_size_type_v = has_size_type<T>::value;

} // namespace detail

class MetalKernelFunction {
 public:
  MetalKernelFunction(MTLComputePipelineState_t cps_);
  ~MetalKernelFunction();
  MetalKernelFunction(MetalKernelFunction&) = delete;
  // Shader properties
  uint64_t getMaxThreadsPerThreadgroup() const;
  uint64_t getThreadExecutionWidth() const;
  uint64_t getStaticThreadGroupMemoryLength() const;
  void runCommandBlock(std::function<void(void)> f);
  // Methods below should be called from runCommandBlock functionT
  void startEncoding();
  void setArg(unsigned idx, const at::TensorBase& t);
  void setArg(unsigned idx, const void* ptr, uint64_t size);
  template <
      typename T,
      typename = std::enable_if_t<
          std::is_integral_v<T> || std::is_same_v<T, float> ||
          (std::is_class_v<T> && std::is_trivially_copyable_v<T> &&
           !detail::has_size_type_v<T>)>>
  inline void setArg(unsigned idx, const T val) {
    setArg(idx, &val, sizeof(T));
  }

  template <
      typename Container,
      typename = std::enable_if_t<detail::has_size_type_v<Container>>>
  inline void setArg(unsigned idx, const Container& values) {
    setArg(
        idx,
        values.data(),
        values.size() * sizeof(typename Container::value_type));
  }
  void dispatch(
      uint64_t length,
      std::optional<uint64_t> groupSize = std::nullopt);
  void dispatch(
      std::array<uint64_t, 2> length,
      std::optional<std::array<uint64_t, 2>> groupSize = std::nullopt);

 private:
  MTLComputePipelineState_t cps;
  MTLComputeCommandEncoder_t encoder = nullptr;
};

class MetalShaderLibrary {
 public:
  MetalShaderLibrary(const std::string& src)
      : shaderSource(src), nparams(0), compile_options(nullptr) {}
  MetalShaderLibrary(const std::string& src, unsigned nparams_)
      : shaderSource(src), nparams(nparams_), compile_options(nullptr) {}
  MetalShaderLibrary(
      const std::string& src,
      unsigned nparams_,
      MTLCompileOptions* compile_options_)
      : shaderSource(src),
        nparams(nparams_),
        compile_options(compile_options_) {}
  MetalShaderLibrary(const MetalShaderLibrary&) = delete;
  virtual ~MetalShaderLibrary() = default;
  std::vector<std::string> getFunctionNames();
  std::shared_ptr<MetalKernelFunction> getKernelFunction(
      const std::string& name);
  inline MTLComputePipelineState_t getPipelineStateForFunc(
      const std::string& fname) {
    return getLibraryPipelineState(getLibrary(), fname).first;
  }
  MTLComputePipelineState_t getPipelineStateForFunc(
      const std::string& fname,
      const std::initializer_list<std::string>& params) {
    return getLibraryPipelineState(getLibrary(params), fname).first;
  }
  inline MTLFunction_t getMTLFunction(const std::string& fname) {
    return getLibraryPipelineState(getLibrary(), fname).second;
  }
  MTLFunction_t getMTLFunction(
      const std::string& fname,
      const std::initializer_list<std::string>& params) {
    return getLibraryPipelineState(getLibrary(params), fname).second;
  }
  static MetalShaderLibrary& getBundledLibrary();

 protected:
  virtual MTLLibrary_t getLibrary();
  virtual MTLLibrary_t getLibrary(
      const std::initializer_list<std::string>& params);
  MTLLibrary_t library = nullptr;

 private:
  std::pair<MTLComputePipelineState_t, MTLFunction_t> getLibraryPipelineState(
      MTLLibrary_t lib,
      const std::string& fname);
  MTLLibrary_t compileLibrary(const std::string& src);
  std::string shaderSource;
  unsigned nparams;
  MTLCompileOptions* compile_options;
  std::unordered_map<std::string, MTLLibrary_t> libMap;
  std::unordered_map<
      std::string,
      std::pair<MTLComputePipelineState_t, MTLFunction_t>>
      cplMap;
};

class DynamicMetalShaderLibrary : public MetalShaderLibrary {
 public:
  DynamicMetalShaderLibrary(const std::string& src) : MetalShaderLibrary(src) {
    // Compile right away
    getLibrary();
  }
  ~DynamicMetalShaderLibrary();
};

} // namespace at::native::mps
