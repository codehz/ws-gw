#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <flatbuffers/flatbuffers.h>

#include <websocketpp/close.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/logger/levels.hpp>

#include "../proto/service_generated.h"

#include "../include/ws-gw.h"

namespace WsGw {
using namespace std::placeholders;
namespace opcode       = websocketpp::frame::opcode;
namespace close_status = websocketpp::close::status;

void Service::OnMessage(websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
  try {
    if (msg->get_opcode() == opcode::TEXT) throw RemoteException{msg->get_payload()};

    flatbuffers::Verifier verifier{(uint8_t const *) msg->get_payload().c_str(), msg->get_payload().size()};

    if (flag == 1) {
      auto resp = flatbuffers::GetRoot<proto::Service::HandshakeResponse>(msg->get_payload().c_str());
      if (!resp->Verify(verifier)) return;
      if (resp->magic()->string_view() != "WS-GATEWAY OK") throw MagicError{"WS-GATEWAY OK", resp->magic()->c_str()};
      flag = 2;
      cv.notify_all();
    } else {
      auto recv = flatbuffers::GetRoot<proto::Service::Receive::ReceivePacket>(msg->get_payload().c_str());
      if (!recv->Verify(verifier)) return;
      auto req = recv->packet_as_Request();
      if (req) {
        auto id      = req->id();
        auto key     = req->key()->str();
        auto payload = req->payload();
        auto it      = mapped.find(key);
        auto handler = it == mapped.end() ? defaultHandler : it->second;
        handler({payload->data(), payload->size()}, [id, this](std::exception_ptr ep, BufferView view) {
          flatbuffers::FlatBufferBuilder buf{256};
          flatbuffers::Offset<proto::Service::Send::SendPacket> packet;
          if (ep) {
            try {
              std::rethrow_exception(ep);
            } catch (std::exception const &ex) {
              auto exinfo = proto::CreateExceptionInfoDirect(buf, ex.what());
              auto exobj  = proto::Service::Send::CreateException(buf, id, exinfo);
              packet = proto::Service::Send::CreateSendPacket(buf, proto::Service::Send::Send_Exception, exobj.Union());
            }
          } else {
            auto payload = buf.CreateVector(view.data(), view.size());
            auto respobj = proto::Service::Send::CreateResponse(buf, id, payload);
            packet = proto::Service::Send::CreateSendPacket(buf, proto::Service::Send::Send_Response, respobj.Union());
          }
          buf.Finish(packet);
          ws.send(conhdr, buf.GetBufferPointer(), buf.GetSize(), opcode::BINARY);
        });
      }
    }
  } catch (std::exception const &ex) {
    ep = std::make_exception_ptr(ex);
    ws.close(hdl, close_status::no_status, "");
  }
}

void Service::Connect(const std::string &endpoint, ServiceDesc desc) {
  websocketpp::lib::error_code ec;
  ws.init_asio();
  ws.set_user_agent("ws-gw/0");
  ws.clear_access_channels(websocketpp::log::alevel::all);
  ws.clear_error_channels(websocketpp::log::elevel::all);
  ws.set_message_handler(std::bind(&Service::OnMessage, this, _1, _2));
  ws.set_close_handler([this](auto) { ws.stop(); });
  ws.set_fail_handler([this](websocketpp::connection_hdl hdl) {
    ep = std::make_exception_ptr(ConnectFailedError{});
    ws.stop();
  });
  ws.set_open_handler([this, desc{std::move(desc)}](websocketpp::connection_hdl co) {
    conhdr = co;
    flatbuffers::FlatBufferBuilder buf{64};
    buf.Finish(proto::Service::CreateHandshakeDirect(
        buf, "WS-GATEWAY", 0, desc.name.c_str(), desc.identifier.c_str(), desc.version.c_str()));
    auto data = buf.GetBufferPointer();
    auto size = buf.GetSize();
    try {
      ws.send(co, data, size, opcode::BINARY);
    } catch (std::exception const &ex) {
      if (!ep) ep = std::make_exception_ptr(ex);
      ws.stop();
    }
  });
  auto con = ws.get_connection(endpoint, ec);
  if (ec) throw ParseFailed(ec);
  ws.connect(con);

  std::thread{[this] {
    {
      std::unique_lock lk{mtx};
      cv.wait(lk, [this] { return flag.load() == 1; });
    }

    ws.run();
    if (onstop) onstop(ep);
    if (!ep) ep = std::make_exception_ptr(DisconnectedError{});
    flag = -1;
    cv.notify_all();
  }}.detach();

  flag = 1;
  cv.notify_all();

  {
    std::unique_lock lk{mtx};
    cv.wait(lk, [this] { return flag.load() != 1; });
  }
  if (ep) {
    ws.close(con, close_status::abnormal_close, "");
    flag = 0;
    std::rethrow_exception(ep);
  }
}

void Service::Wait() {
  std::unique_lock lk{mtx};
  cv.wait(lk, [this] { return flag.load() == -1; });
  if (ep) std::rethrow_exception(ep);
}

void Service::Broadcast(const std::string_view &key, BufferView data) {
  if (flag != 2) return;
  flatbuffers::FlatBufferBuilder buf{256};
  auto skey    = buf.CreateString(key);
  auto payload = buf.CreateVector(data.data(), data.size());
  auto broad   = proto::Service::Send::CreateBroadcast(buf, skey, payload);
  auto packet  = proto::Service::Send::CreateSendPacket(buf, proto::Service::Send::Send_Broadcast, broad.Union());
  buf.Finish(packet);
  try {
    ws.send(conhdr, buf.GetBufferPointer(), buf.GetSize(), opcode::BINARY);
  } catch (std::exception const &ex) {
    ep = std::make_exception_ptr(ex);
    ws.close(conhdr, close_status::abnormal_close, "");
  }
}

} // namespace WsGw