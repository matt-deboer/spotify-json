/*
 * Copyright (c) 2015-2016 Spotify AB
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

#pragma once

#include <array>
#include <deque>
#include <list>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include <spotify/json/decoding_context.hpp>
#include <spotify/json/default_codec.hpp>
#include <spotify/json/detail/decoding_helpers.hpp>
#include <spotify/json/encoding_context.hpp>

namespace spotify {
namespace json {
namespace detail {

struct SequenceInserter {
  using state = int;
  static const state init_state = 0;

  template <typename Container, typename Value>
  static state insert(
      decoding_context &, state, Container &container, Value &&value) {
    container.push_back(std::forward<Value>(value));
    return init_state;
  }

  template <typename Container>
  static void validate(decoding_context &, state, Container &) {
    // Nothing to validate
  }
};

struct FixedSizeSequenceInserter {
  using state = size_t;
  static const state init_state = 0;

  template <typename Container, typename Value>
  static state insert(
      decoding_context &context, state pos, Container &container, Value &&value) {
    fail_if(context, pos >= container.size(), "Too many elements in array");
    container[pos] = value;
    return pos + 1;
  }

  template <typename Container>
  static void validate(decoding_context &context, state pos, Container &container) {
    fail_if(context, pos != container.size(), "Too few elements in array");
  }
};

struct AssociativeInserter {
  using state = int;
  static const state init_state = 0;

  template <typename Container, typename Value>
  static state insert(
      decoding_context &, state, Container &container, Value &&value) {
    container.insert(std::forward<Value>(value));
    return init_state;
  }

  template <typename Container>
  static void validate(decoding_context &, state, Container &) {
    // Nothing to validate
  }
};

template <typename T> struct Inserter;

template <typename T>
struct Inserter<std::vector<T>> : public SequenceInserter {};

template <typename T>
struct Inserter<std::deque<T>> : public SequenceInserter {};

template <typename T>
struct Inserter<std::list<T>> : public SequenceInserter {};

template <typename T, size_t Size>
struct Inserter<std::array<T, Size>> : public FixedSizeSequenceInserter {};

template <typename T>
struct Inserter<std::set<T>> : public AssociativeInserter {};

template <typename T>
struct Inserter<std::unordered_set<T>> : public AssociativeInserter {};

}  // namespace detail

namespace codec {

template <typename T, typename InnerCodec, typename Inserter>
class array_t final {
 public:
  using object_type = T;

  static_assert(
      std::is_same<
          typename T::value_type,
          typename std::decay<InnerCodec>::type::object_type>::value,
      "Array container type must match inner codec type");

  explicit array_t(InnerCodec inner_codec)
      : _inner_codec(inner_codec) {}

  object_type decode(decoding_context &context) const {
    object_type output;
    typename Inserter::state state = Inserter::init_state;
    detail::advance_past_comma_separated(context, '[', ']', [&]{
      state = Inserter::insert(
          context, state, output, _inner_codec.decode(context));
    });
    Inserter::validate(context, state, output);
    return output;
  }

  void encode(encoding_context &context, const object_type &array) const {
    context.append('[');
    for (const auto &element : array) {
      if (json_likely(detail::should_encode(_inner_codec, element))) {
        _inner_codec.encode(context, element);
        context.append(',');
      }
    }
    context.append_or_replace(',', ']');
  }

 private:
  InnerCodec _inner_codec;
};

template <typename T, typename InnerCodec>
array_t<T, typename std::decay<InnerCodec>::type, detail::Inserter<T>> array(
    InnerCodec &&inner_codec) {
  return array_t<T, typename std::decay<InnerCodec>::type, detail::Inserter<T>>(
      std::forward<InnerCodec>(inner_codec));
}

}  // namespace codec

template <typename T>
struct default_codec_t<std::vector<T>> {
  static decltype(codec::array<std::vector<T>>(default_codec<T>())) codec() {
    return codec::array<std::vector<T>>(default_codec<T>());
  }
};

template <typename T>
struct default_codec_t<std::deque<T>> {
  static decltype(codec::array<std::deque<T>>(default_codec<T>())) codec() {
    return codec::array<std::deque<T>>(default_codec<T>());
  }
};

template <typename T>
struct default_codec_t<std::list<T>> {
  static decltype(codec::array<std::list<T>>(default_codec<T>())) codec() {
    return codec::array<std::list<T>>(default_codec<T>());
  }
};

template <typename T, size_t Size>
struct default_codec_t<std::array<T, Size>> {
  static decltype(codec::array<std::array<T, Size>>(default_codec<T>())) codec() {
    return codec::array<std::array<T, Size>>(default_codec<T>());
  }
};

template <typename T>
struct default_codec_t<std::set<T>> {
  static decltype(codec::array<std::set<T>>(default_codec<T>())) codec() {
    return codec::array<std::set<T>>(default_codec<T>());
  }
};

template <typename T>
struct default_codec_t<std::unordered_set<T>> {
  static decltype(codec::array<std::unordered_set<T>>(default_codec<T>())) codec() {
    return codec::array<std::unordered_set<T>>(default_codec<T>());
  }
};

}  // namespace json
}  // namespace spotify
