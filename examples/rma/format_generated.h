// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_FORMAT_H_
#define FLATBUFFERS_GENERATED_FORMAT_H_

#include "flatbuffers/flatbuffers.h"

struct rma_msg;

struct rma_msg FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_BUFFER = 4,
    VT_SIZE = 6,
    VT_ID = 8,
    VT_RKEY = 10
  };
  uint64_t buffer() const {
    return GetField<uint64_t>(VT_BUFFER, 0);
  }
  uint64_t size() const {
    return GetField<int16_t>(VT_SIZE, 0);
  }
  int16_t id() const {
    return GetField<int16_t>(VT_ID, 0);
  }
  uint64_t rkey() const {
    return GetField<uint64_t>(VT_RKEY, 0);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<uint64_t>(verifier, VT_BUFFER) &&
           VerifyField<uint64_t>(verifier, VT_SIZE) &&
           VerifyField<int16_t>(verifier, VT_ID) &&
           VerifyField<uint64_t>(verifier, VT_RKEY) &&
           verifier.EndTable();
  }
};

struct rma_msgBuilder {
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_buffer(uint64_t buffer) {
    fbb_.AddElement<uint64_t>(rma_msg::VT_BUFFER, buffer, 0);
  }
  void add_size(uint64_t size) {
    fbb_.AddElement<uint64_t>(rma_msg::VT_SIZE, size, 0);
  }
  void add_id(int16_t id) {
    fbb_.AddElement<int16_t>(rma_msg::VT_ID, id, 0);
  }
  void add_rkey(uint64_t rkey) {
    fbb_.AddElement<uint64_t>(rma_msg::VT_RKEY, rkey, 0);
  }
  explicit rma_msgBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  rma_msgBuilder &operator=(const rma_msgBuilder &);
  flatbuffers::Offset<rma_msg> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<rma_msg>(end);
    return o;
  }
};

inline flatbuffers::Offset<rma_msg> Createrma_msg(
    flatbuffers::FlatBufferBuilder &_fbb,
    uint64_t buffer = 0,
    uint64_t size = 0,
    int16_t id = 0,
    uint64_t rkey = 0) {
  rma_msgBuilder builder_(_fbb);
  builder_.add_rkey(rkey);
  builder_.add_buffer(buffer);
  builder_.add_id(id);
  builder_.add_size(size);
  return builder_.Finish();
}

#endif  // FLATBUFFERS_GENERATED_FORMAT_H_