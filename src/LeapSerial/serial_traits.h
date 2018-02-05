// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include "Archive.h"
#include "Descriptor.h"
#include "field_serializer_t.h"
#include <array>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace leap {
  template<typename T>
  struct serial_traits;

  template<typename T, typename>
  struct field_serializer_t;

  template<typename T>
  struct serializer_is_irresponsible;

  template<typename T, typename = void>
  struct primitive_serial_traits:
    std::false_type
  {
    const bool valid = false;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::ignored;
    }
  };

  /// <summary>
  /// Holds true if T can be serialized
  /// </summary>
  template<typename T>
  struct has_serializer
  {
    static const bool value = !std::is_base_of<std::false_type, serial_traits<T>>::value;
  };

  /// <summary>
  /// Holds true if the referenced field is optional
  /// </summary>
  template<typename T, typename = void>
  struct is_optional
  {
    static const bool value = false;
  };
  template<typename T>
  struct is_optional<T, typename std::enable_if<serial_traits<T>::is_optional>::type> {
    static const bool value = true;
  };

  // Create/delete structure used to describe how to allocate and free a memory block
  struct create_delete {
    void* (*pfnAlloc)();
    void(*pfnFree)(void*);
  };

  // Specialization for anything that is a floating-point type.  These can be written directly to disk,
  // so we don't have to perform any kind of translation.  On big-endian systems, we will need to
  // perform byte order conversions.
  template<typename T>
  struct primitive_serial_traits<T, typename std::enable_if<std::is_floating_point<T>::value>::type>
  {
    static const bool is_optional = false;

    static ::leap::serial_atom type() {
      if (std::is_same<T, float>::value)
        return ::leap::serial_atom::f32;
      if (std::is_same<T, double>::value)
        return ::leap::serial_atom::f64;
      if (std::is_same<T, long double>::value) {
        if(sizeof(long double) == sizeof(double))
          // Special case for certain compilers which treat long double as a synonym for double
          return ::leap::serial_atom::f64;
        else
          return ::leap::serial_atom::f80;
      }
    }

    // Trivial serialization/deserialization operations
    static uint64_t size(const OArchive& ar, T val) {
      return ar.SizeFloat(val);
    }

    static void serialize(OArchive& ar, const T& obj) {
      ar.WriteFloat(obj);
    }

    static void deserialize(IArchive& ar, T& obj, uint64_t ncb) {
      ar.ReadFloat(obj);
    }
  };

  // Specialization for anything that is integral.
  template<typename T>
  struct primitive_serial_traits<T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value>::type>
  {
    static const bool is_optional = false;

    static ::leap::serial_atom type() {
      if (std::is_same<T, bool>::value)
        return ::leap::serial_atom::boolean;

      static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8, "Cannot serialize integral types larger than 8 bytes");
      switch (sizeof(T)) {
      case 1:
        return std::is_signed<T>::value ? ::leap::serial_atom::i8 : ::leap::serial_atom::ui8;
      case 2:
        return std::is_signed<T>::value ? ::leap::serial_atom::i16 : ::leap::serial_atom::ui16;
      case 4:
        return std::is_signed<T>::value ? ::leap::serial_atom::i32 : ::leap::serial_atom::ui32;
      case 8:
        return std::is_signed<T>::value ? ::leap::serial_atom::i64 : ::leap::serial_atom::ui64;
      }
      throw std::runtime_error("Integral type encountered with an unsupported field size");
    }

    // Trivial serialization/deserialization operations
    static uint64_t size(const OArchive& ar, T val) {
      return ar.SizeInteger((int64_t)val, sizeof(T));
    }

    static void serialize(OArchive& ar, T val) {
      ar.WriteInteger((int64_t)val, sizeof(T));
    }

    static void deserialize(IArchive& ar, T& val, uint64_t ncb) {
      val = static_cast<T>(ar.ReadInteger(sizeof(T)));
    }
  };

  template<typename T, size_t N>
  struct primitive_serial_traits<T[N], typename std::enable_if<has_serializer<T>::value>::type>
  {
    typedef T value_type;
    static const bool is_array = true;
    static const bool is_optional = false;

    struct CArrayImpl :
      public IArrayReader
    {
      CArrayImpl(const T* pAry) :
        IArrayReader(field_serializer_t<T, void>::GetDescriptor()),
        pAry(pAry)
      {}

      const T* const pAry;

      size_t immutable_size(void) const override { return std::is_arithmetic<T>::value ? sizeof(T) : 0; }
      const void* get(size_t i) const override { return pAry + i; }
      size_t size(void) const override { return N; }
    };

    struct ArrayImpl :
      public IArrayAppender
    {
      ArrayImpl(T* pAry) :
        IArrayAppender(field_serializer_t<T, void>::GetDescriptor()),
        pAry(pAry)
      {}

      T* const pAry;
      size_t i = 0;

      void reserve(size_t n) override {
        if (n != N)
          throw std::runtime_error("Incorrect deserialization attempt into a non-fixed-size space");
      }

      void* allocate(void) override {
        return pAry + i++;
      };
    };

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::array;
    }

    static uint64_t size(const OArchiveRegistry& ar, const T* pObj) {
      return ar.SizeArray(CArrayImpl{ pObj });
    }

    static void serialize(OArchiveRegistry& ar, const T* pObj) {
      ar.WriteArray(CArrayImpl{ pObj });
    }

    static void deserialize(IArchiveRegistry& ar, T* pObj, uint64_t ncb) {
      ar.ReadArray(ArrayImpl{ pObj });
    }
  };

  template<>
  struct primitive_serial_traits<bool, void>
  {
    static const bool is_optional = false;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::boolean;
    }

    static uint64_t size(const OArchive& ar, bool val) {
      return ar.SizeBool(val);
    }

    static void serialize(OArchive& ar, bool val) {
      ar.WriteBool(val);
    }

    static void deserialize(IArchive& ar, bool& val, uint64_t ncb) {
      val = ar.ReadBool();
    }
  };

  template<typename Rep, typename Period>
  struct primitive_serial_traits<std::chrono::duration<Rep, Period>, void>:
    primitive_serial_traits<Rep>
  {
    static_assert(std::is_arithmetic<Rep>::value, "LeapSerial presently can only serialize arithmetic duration types");
    static const bool is_irresponsible = false;
    static const bool is_optional = false;

    // Trivial serialization/deserialization operations
    static uint64_t size(const OArchive& ar, std::chrono::duration<Rep, Period> val) {
      return primitive_serial_traits<Rep>::size(ar, val.count());
    }

    static void serialize(OArchive& ar, std::chrono::duration<Rep, Period> val) {
      primitive_serial_traits<Rep>::serialize(ar, val.count());
    }

    static void deserialize(IArchive& ar, std::chrono::duration<Rep, Period>& val, uint64_t ncb) {
      Rep v = 0;
      primitive_serial_traits<Rep>::deserialize(ar, v, ncb);
      val = std::chrono::duration<Rep, Period>(v);
    }
  };

  // Convenience specialization for std::vector
  template<typename T, typename Alloc>
  struct primitive_serial_traits<std::vector<T, Alloc>, void> :
    std::integral_constant<bool, has_serializer<T>::value>
  {
    typedef std::vector<T, Alloc> serial_type;
    typedef T value_type;
    static const bool is_array = true;
    static const bool is_optional = true;

    struct CArrayImpl :
      IArrayReader
    {
      CArrayImpl(const serial_type& obj) :
        IArrayReader(field_serializer_t<T, void>::GetDescriptor()),
        obj(obj)
      {}

      const serial_type& obj;

      size_t immutable_size(void) const override { return std::is_arithmetic<T>::value ? sizeof(T) : 0; }
      const void* get(size_t i) const override { return &obj[i]; }
      size_t size(void) const override { return obj.size(); }
    };

    struct ArrayImpl :
      IArrayAppender
    {
      ArrayImpl(serial_type& obj) :
        IArrayAppender(field_serializer_t<T, void>::GetDescriptor()),
        obj(obj)
      {}

      serial_type& obj;

      void reserve(size_t n) override { obj.reserve(n); }
      void* allocate(void) override {
        obj.push_back({});
        return &obj.back();
      }
    };

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::array;
    }

    // We are irresponsible if and only if T is irresponsible
    static const bool is_irresponsible = serializer_is_irresponsible<T>::value;

    // If we're irresponsible, we need a more powerful archive registry as an input
    typedef typename std::conditional<is_irresponsible, IArchiveRegistry, IArchive>::type iarchive;

    static uint64_t size(const OArchiveRegistry& ar, const serial_type& v) {
      static_assert(has_serializer<T>::value, "Attempted to use serial traits for an std::vector<T>, but T is not serializable");
      return ar.SizeArray(CArrayImpl{ v });
    }

    static void serialize(OArchiveRegistry& ar, const serial_type& v) {
      ar.WriteArray(CArrayImpl{ v });
    }

    static void deserialize(iarchive& ar, serial_type& v, uint64_t ncb) {
      ar.ReadArray(ArrayImpl{ v });
    }
  };

  /// <summary>
  /// Mix-in type for all key/value container types
  /// </summary>
  template<typename Container>
  struct serial_traits_map_t:
    std::integral_constant<
      bool,
      has_serializer<typename Container::key_type>::value &&
      has_serializer<typename Container::mapped_type>::value
    >
  {
    static const bool is_map = true;
    static const bool is_optional = true;
    typedef typename Container::key_type key_type;
    typedef typename Container::mapped_type mapped_type;
    typedef serial_traits<key_type> key_traits;
    typedef serial_traits<mapped_type> mapped_traits;

    // Convenience static.  We need to make allocations if either our key or value type
    // needs to make allocations.
    static const bool sc_needsAllocation =
      serializer_is_irresponsible<key_type>::value ||
      serializer_is_irresponsible<mapped_type>::value;

    // We need to know if either the key type or the mapped type is irresponsible.
    // If it is, we're going to need to ensure we accept the correct input archive
    // type in order to ensure values are correctly propagated.
    typedef typename std::conditional<sc_needsAllocation, IArchiveRegistry, IArchive>::type iarchive;

    struct DictionaryReaderImpl :
      IDictionaryReader
    {
      DictionaryReaderImpl(const Container& container) :
        IDictionaryReader(
          field_serializer_t<key_type, void>::GetDescriptor(),
          field_serializer_t<mapped_type, void>::GetDescriptor()
        ),
        container(container),
        q(container.end())
      {}

      const Container& container;
      bool init = false;
      typename Container::const_iterator q;

      size_t size(void) const override { return container.size(); }
      bool next(void) override {
        if (init)
          ++q;
        else {
          init = true;
          q = container.begin();
        }
        return q != container.end();
      }
      const void* key(void) const override { return &q->first; }
      const void* value(void) const override { return &q->second; }
    };

    struct DictionaryInserterImpl :
      IDictionaryInserter
    {
      DictionaryInserterImpl(Container& container) :
        IDictionaryInserter(
          field_serializer_t<key_type, void>::GetDescriptor(),
          field_serializer_t<mapped_type, void>::GetDescriptor()
        ),
        container(container)
      {}

      Container& container;
      key_type m_key;

      void* key(void) override { return &m_key; }
      void* insert(void) override {
        void* retVal = &container[std::move(m_key)];
        m_key = key_type{};
        return retVal;
      }
    };

    static ::leap::serial_atom type() {
      static_assert(
        has_serializer<key_type>::value,
        "Attempted to use serial traits for Container<Key, Value>, but Key is not serializable"
      );
      static_assert(
        has_serializer<mapped_type>::value,
        "Attempted to use serial traits for Container<Key, Value>, but Value is not serializable"
      );

      return ::leap::serial_atom::map;
    }

    static uint64_t size(const OArchiveRegistry& ar, const Container& obj) {
      return ar.SizeDictionary(DictionaryReaderImpl{ obj });
    }

    static void serialize(OArchiveRegistry& ar, const Container& obj) {
      return ar.WriteDictionary(DictionaryReaderImpl{ obj });
    }

    static void deserialize(iarchive& ar, Container& obj, uint64_t ncb) {
      ar.ReadDictionary(DictionaryInserterImpl{ obj });
    }
  };

  // Associative array types:
  template<typename Key, typename Value>
  struct primitive_serial_traits<std::map<Key, Value>, void> :
    serial_traits_map_t<std::map<Key, Value>>
  {};

  template<typename Key, typename Value>
  struct primitive_serial_traits<std::unordered_map<Key, Value>, void> :
    serial_traits_map_t<std::unordered_map<Key, Value>>
  {};

  template<typename T>
  struct primitive_serial_traits<std::unique_ptr<T>, void> :
    std::integral_constant<bool, has_serializer<T>::value>
  {
    typedef std::unique_ptr<T> ptr_t;
    static const bool is_optional = true;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::reference;
    }

    static uint64_t size(const OArchiveRegistry& ar, const ptr_t& pObj) {
      return ar.SizeObjectReference(
        field_serializer_t<T,void>::GetDescriptor(),
        pObj.get()
      );
    }

    static void serialize(OArchiveRegistry& ar, const ptr_t& obj) {
      ar.WriteObjectReference(
        field_serializer_t<T, void>::GetDescriptor(),
        obj.get()
      );
    }

    static void deserialize(IArchive& ar, ptr_t& obj, uint64_t ncb) {
      auto released = ar.ReadObjectReferenceResponsible(
        [] {
          return IArchive::ReleasedMemory{new T, nullptr};
        },
        field_serializer_t<T, void>::GetDescriptor(),
        true
      );

      obj.reset(static_cast<T*>(released.pObject));
    }
  };

  template<typename T>
  struct primitive_serial_traits<std::shared_ptr<T>, void> :
    std::integral_constant<bool, has_serializer<T>::value>
  {
    typedef std::shared_ptr<T> ptr_t;
    static const bool is_optional = true;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::reference;
    }

    static uint64_t size(const OArchiveRegistry& ar, const ptr_t& pObj) {
      return ar.SizeObjectReference(field_serializer_t<T,void>::GetDescriptor(), pObj.get());
    }

    static void serialize(OArchiveRegistry& ar, const ptr_t& obj) {
      ar.WriteObjectReference(
        field_serializer_t<T, void>::GetDescriptor(),
        obj.get()
      );
    }

    static void deserialize(IArchive& ar, ptr_t& obj, uint64_t ncb) {
      auto released = ar.ReadObjectReferenceResponsible(
        [] {
          std::shared_ptr<T> retVal = std::shared_ptr<T>(new T());
          return IArchive::ReleasedMemory{ retVal.get(), retVal };
        },
        field_serializer_t<T, void>::GetDescriptor(),
        false
      );

      obj = std::static_pointer_cast<T>(released.pContext);
    }
  };

  template<typename T, size_t N>
  struct primitive_serial_traits<std::array<T, N>, void> :
    std::integral_constant<bool, has_serializer<T>::value>
  {
    typedef T value_type;
    static const bool is_array = true;
    static const bool is_optional = false;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::array;
    }

    static uint64_t size(const OArchiveRegistry& ar, const std::array<T, N>& v) {
      return serial_traits<T[N]>::size(ar, &v.front());
    }

    static void serialize(OArchiveRegistry& ar, const std::array<T, N>& v) {
      return serial_traits<T[N]>::serialize(ar, &v.front());
    }

    static void deserialize(IArchiveRegistry& ar, std::array<T, N>& v, uint64_t ncb) {
      return serial_traits<T[N]>::deserialize(ar, &v.front(), ncb);
    }
  };

  template<typename T>
  struct serial_traits:
    primitive_serial_traits<T>
  {};

  // If we have a pointer to another object, we have to write an ID out and not the entire object
  // The object will be serialized at some other time
  template<typename T>
  struct serial_traits<T*>
  {
    static const bool is_optional = true;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::reference;
    }

    static uint64_t size(const OArchiveRegistry& ar, typename std::add_const<T>::type* pObj) {
      return ar.SizeObjectReference(field_serializer_t<T, void>::GetDescriptor(), pObj);
    }

    static void serialize(OArchiveRegistry& ar, typename std::add_const<T>::type* pObj) {
      ar.WriteObjectReference(
        field_serializer_t<T, void>::GetDescriptor(),
        pObj
        );
    }

    static void deserialize(IArchiveRegistry& ar, T*& pObj, uint64_t ncb) {
      create_delete creatorDeletor = {
        []() -> void* { return new T; },
        [](void* ptr) { delete (T*)ptr; }
      };

      pObj = (T*)ar.ReadObjectReference(creatorDeletor, field_serializer_t<T, void>::GetDescriptor());
    }
  };

  // Const pointer serialization/deserialization rules are the same as pointer serialization rules,
  // but we act as though the type itself is non-const for the purpose of deserialization
  template<typename T>
  struct serial_traits<const T*> :
    serial_traits<T*>
  {
    static const bool is_optional = true;

    static void deserialize(IArchiveRegistry& ar, const T*& pObj, uint64_t ncb) {
      T* ptr;
      serial_traits<T*>::deserialize(ar, ptr, ncb);
      pObj = ptr;
    }
  };

  template<typename T>
  struct serial_traits<std::basic_string<T, std::char_traits<T>, std::allocator<T>>>
  {
    static const bool is_optional = true;

    typedef std::basic_string<T, std::char_traits<T>, std::allocator<T>> string_t;

    static ::leap::serial_atom type() {
      return ::leap::serial_atom::string;
    }

    static uint64_t size(const OArchive& ar, const string_t& pObj) {
      return ar.SizeString(pObj.data(), pObj.size(), sizeof(T));
    }

    static void serialize(OArchive& ar, const string_t& obj) {
      ar.WriteString(obj.data(), obj.size(), sizeof(T));
    }

    static void deserialize(IArchive& ar, string_t& obj, uint64_t ncb) {
      ar.ReadString(
        [&](uint64_t count) {
          obj.resize((uint32_t)count);
          return &obj[0];
        },
        sizeof(T),
        ncb
      );
    }
  };
}
