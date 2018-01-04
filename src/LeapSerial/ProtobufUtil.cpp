// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "ProtobufUtil.hpp"
#include "Archive.h"
#include "Descriptor.h"
#include <iomanip>
#include <sstream>

using namespace leap;
using leap::internal::protobuf::WireType;

WireType leap::internal::protobuf::ToWireType(serial_atom atom) {
  switch (atom) {
  case serial_atom::boolean:
  case serial_atom::i8:
  case serial_atom::ui8:
  case serial_atom::i16:
  case serial_atom::ui16:
  case serial_atom::i32:
  case serial_atom::ui32:
  case serial_atom::i64:
  case serial_atom::ui64:
    return WireType::Varint;
  case serial_atom::f32:
    return WireType::DoubleWord;
  case serial_atom::f64:
  case serial_atom::f80:
    return WireType::QuadWord;
  case serial_atom::reference:
  case serial_atom::array:
    return WireType::LenDelimit;
  case serial_atom::string:
    return WireType::LenDelimit;
  case serial_atom::map:
    break;
  case serial_atom::descriptor:
    return WireType::LenDelimit;
  case serial_atom::finalized_descriptor:
    break;
  case serial_atom::ignored:
    throw std::invalid_argument("serial_atom::ignored is a utility type and should not be used in ordinary operations");
  }
  throw std::invalid_argument("Attempted to find a wire type for an unrecognized serial atom type");
}

const char* leap::internal::protobuf::ToProtobufField(serial_atom atom) {
  switch (atom) {
  case serial_atom::boolean:
    return "bool";
  case serial_atom::i8:
  case serial_atom::i16:
  case serial_atom::i32:
    return "sint32";
  case serial_atom::i64:
    return "sint64";
  case serial_atom::ui8:
  case serial_atom::ui16:
  case serial_atom::ui32:
    return "int32";
  case serial_atom::ui64:
    return "int64";
  case serial_atom::f32:
    return "float";
  case serial_atom::f64:
  case serial_atom::f80:
    return "double";
  case serial_atom::reference:
    break;
  case serial_atom::array:
  case serial_atom::string:
    return "string";
  case serial_atom::map:
    break;
  case serial_atom::descriptor:
  case serial_atom::finalized_descriptor:
    break;
  case serial_atom::ignored:
    throw std::invalid_argument("Invalid serial atom type");
  }
  throw std::invalid_argument("Attempted to obtain the protobuf field of a non-value type");
}

static std::string FormatError(const descriptor& descriptor) {
  std::stringstream ss;
  ss << "The OArchiveProtobuf requires that all entries have identifiers" << std::endl
    << "Fields at the following offsets do not have identifiers:" << std::endl;
  for (const auto& field_descriptor : descriptor.field_descriptors)
    ss << "[ " << std::left << std::setw(8) << ToString(field_descriptor.serializer.type()) << " ] @+" << field_descriptor.offset << std::endl;
  return ss.str();
}

internal::protobuf::serialization_error::serialization_error(std::string&& err) :
  ::leap::serialization_error(std::move(err))
{}

internal::protobuf::serialization_error::serialization_error(const descriptor& descriptor) :
  ::leap::serialization_error(FormatError(descriptor))
{}
