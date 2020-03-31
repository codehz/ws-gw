// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SHARED_WSGW_PROTO_H_
#define FLATBUFFERS_GENERATED_SHARED_WSGW_PROTO_H_

#include "flatbuffers/flatbuffers.h"

namespace WsGw {
namespace proto {

struct ExceptionInfo;

struct ExceptionInfo FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_MESSAGE = 4
  };
  const flatbuffers::String *message() const {
    return GetPointer<const flatbuffers::String *>(VT_MESSAGE);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffsetRequired(verifier, VT_MESSAGE) &&
           verifier.VerifyString(message()) &&
           verifier.EndTable();
  }
};

struct ExceptionInfoBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_message(flatbuffers::Offset<flatbuffers::String> message) {
    fbb_.AddOffset(ExceptionInfo::VT_MESSAGE, message);
  }
  explicit ExceptionInfoBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ExceptionInfoBuilder &operator=(const ExceptionInfoBuilder &);
  flatbuffers::Offset<ExceptionInfo> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<ExceptionInfo>(end);
    fbb_.Required(o, ExceptionInfo::VT_MESSAGE);
    return o;
  }
};

inline flatbuffers::Offset<ExceptionInfo> CreateExceptionInfo(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::String> message = 0) {
  ExceptionInfoBuilder builder_(_fbb);
  builder_.add_message(message);
  return builder_.Finish();
}

inline flatbuffers::Offset<ExceptionInfo> CreateExceptionInfoDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const char *message = nullptr) {
  auto message__ = message ? _fbb.CreateString(message) : 0;
  return WsGw::proto::CreateExceptionInfo(
      _fbb,
      message__);
}

}  // namespace proto
}  // namespace WsGw

#endif  // FLATBUFFERS_GENERATED_SHARED_WSGW_PROTO_H_
