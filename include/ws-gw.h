#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>

#include <flatbuffers/flatbuffers.h>
#include <websocketpp/client.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

namespace WsGw {

class Service;
class Buffer;

class BufferView {
  friend class Service;
  std::basic_string_view<uint8_t> storage;

  BufferView &operator=(BufferView &rhs) = delete;

public:
  BufferView() {}
  BufferView(Buffer const &buffer);
  BufferView(uint8_t const *data, size_t len) : storage(data, len) {}
  BufferView(std::string const &data) : storage((uint8_t const *) data.data(), data.size()) {}
  BufferView(std::string_view data) : storage((uint8_t const *) data.data(), data.size()) {}
  BufferView(std::basic_string<uint8_t> const &data) : storage(data) {}
  BufferView(std::basic_string_view<uint8_t> const &data) : storage(data) {}
  BufferView(flatbuffers::FlatBufferBuilder const &builder) : storage(builder.GetBufferPointer(), builder.GetSize()) {}

  uint8_t const *data() const noexcept { return storage.data(); }
  size_t size() const noexcept { return storage.size(); }
  operator std::string() const noexcept { return {(char const *) data(), size()}; }
  operator std::basic_string<uint8_t>() const noexcept { return {data(), size()}; }
  operator std::basic_string_view<uint8_t>() const noexcept { return storage; }
  operator std::string_view() const noexcept { return {(char const *) storage.data(), storage.size()}; }
};

class Buffer;

class BufferImpl {
  friend class Buffer;

public:
  virtual ~BufferImpl() {}
  virtual uint8_t const *data() const noexcept = 0;
  virtual size_t size() const noexcept         = 0;
};

class BufferImplString : public BufferImpl {
  std::string storage;
  friend class Buffer;

public:
  BufferImplString(std::string data) : storage(std::move(data)) {}
  BufferImplString(char const *data, size_t len) : storage(data, len) {}

  uint8_t const *data() const noexcept { return (uint8_t const *) storage.data(); }

  size_t size() const noexcept { return storage.size(); }
};

class BufferImplUString : public BufferImpl {
  std::basic_string<uint8_t> storage;
  friend class Buffer;

public:
  BufferImplUString(std::basic_string<uint8_t> data) : storage(std::move(data)) {}
  BufferImplUString(uint8_t const *data, size_t len) : storage(data, len) {}

  uint8_t const *data() const noexcept { return (uint8_t const *) storage.data(); }

  size_t size() const noexcept { return storage.size(); }
};

class BufferImplBuilder : public BufferImpl {
  flatbuffers::FlatBufferBuilder storage;
  friend class Buffer;

public:
  BufferImplBuilder(flatbuffers::FlatBufferBuilder &&orig) : storage(std::move(orig)) {}

  uint8_t const *data() const noexcept { return storage.GetBufferPointer(); }

  size_t size() const noexcept { return storage.GetSize(); }
};

class Buffer {
  std::unique_ptr<BufferImpl> impl;

public:
  Buffer() : impl() {}
  Buffer(std::string str) : impl(std::make_unique<BufferImplString>(str)) {}
  Buffer(char const *data, size_t len) : impl(std::make_unique<BufferImplString>(data, len)) {}
  Buffer(std::basic_string<uint8_t> str) : impl(std::make_unique<BufferImplUString>(str)) {}
  Buffer(uint8_t const *data, size_t len) : impl(std::make_unique<BufferImplUString>(data, len)) {}
  Buffer(flatbuffers::FlatBufferBuilder &&builder) : impl(std::make_unique<BufferImplBuilder>(std::move(builder))) {}

  uint8_t const *data() const noexcept { return impl ? impl->data() : nullptr; }
  size_t size() const noexcept { return impl ? impl->size() : 0; }

  operator std::string() const noexcept { return {(char const *) data(), size()}; }
  operator std::basic_string<uint8_t>() const noexcept { return {data(), size()}; }
  operator std::basic_string_view<uint8_t>() const noexcept { return {data(), size()}; }
  operator std::string_view() const noexcept { return {(char const *) data(), size()}; }

  std::string str() { return *this; }
};

inline BufferView::BufferView(Buffer const &buf) : storage(buf.data(), buf.size()) {}

using Handler     = std::function<void(Buffer, std::function<void(std::exception_ptr ep, BufferView)>)>;
using SyncHandler = std::function<Buffer(BufferView const &)>;

struct MagicError : std::runtime_error {
  MagicError(char const *expected, char const *actual)
      : runtime_error("Expected magic " + (std::string) expected + ", got " + actual) {}
};
struct ConnectFailedError : std::runtime_error {
  ConnectFailedError() : runtime_error("Failed to connect") {}
};
struct DisconnectedError : std::runtime_error {
  DisconnectedError() : runtime_error("Disconnected") {}
};

struct ParseFailed : std::exception {
  websocketpp::lib::error_code ec;
  std::string msg;

  ParseFailed(websocketpp::lib::error_code ec) { msg = ec.message(); }

  const char *what() const noexcept override { return msg.c_str(); }
};

struct RemoteException : std::runtime_error {
  RemoteException(std::string data) : runtime_error(std::move(data)) {}
};

struct ServiceDesc {
  std::string name, identifier, version;
};

class Service {
  using client = websocketpp::client<websocketpp::config::asio_client>;
  client ws;
  Handler defaultHandler;
  std::map<std::string, Handler> mapped;
  std::atomic_int8_t flag = 0;
  std::mutex mtx;
  std::condition_variable cv;
  std::exception_ptr ep;
  websocketpp::connection_hdl conhdr;
  std::function<void(std::exception_ptr)> onstop;

  void OnMessage(websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg);

public:
  Service(Handler defaultHandler) : defaultHandler(defaultHandler) {}

  void RegisterHandler(std::string const &name, Handler handler) { mapped.emplace(name, handler); }
  void RegisterHandler(std::string const &name, SyncHandler handler) {
    mapped.emplace(name, [=](auto buffer, auto cb) {
      try {
        cb(nullptr, handler(buffer));
      } catch (std::exception const &ex) { cb(std::make_exception_ptr(ex), {}); }
    });
  }

  void OnStop(std::function<void(std::exception_ptr)> fn) { onstop = fn; }

  void Broadcast(std::string_view const &key, BufferView data);

  void Connect(std::string const &endpoint, ServiceDesc desc);

  void Wait();
};

} // namespace WsGw