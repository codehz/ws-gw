#include <exception>
#include <flatbuffers/flatbuffers.h>
#include <functional>

#include <stdexcept>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/logger/levels.hpp>

#include "../proto/service_generated.h"

#include "../include/ws-gw.h"

namespace WsGw {
using namespace std::placeholders;
namespace opcode = websocketpp::frame::opcode;

void Service::OnMessage(
    websocketpp::connection_hdl hdl,
    websocketpp::config::asio_client::message_type::ptr msg) {
  try {
    if (msg->get_opcode() == opcode::TEXT)
      throw RemoteException{msg->get_payload()};

    flatbuffers::Verifier verifier{(uint8_t const *)msg->get_payload().c_str(),
                                   msg->get_payload().size()};

    if (!booted) {
      auto resp = flatbuffers::GetRoot<proto::Service::HandshakeResponse>(
          msg->get_payload().c_str());
      if (!resp->Verify(verifier)) return;
      if (resp->magic()->string_view() != "WS-GATEWAY OK")
        throw MagicError{"WS-GATEWAY OK", resp->magic()->c_str()};
      booted = true;
    } else {
      auto recv = flatbuffers::GetRoot<proto::Service::Receive::ReceivePacket>(
          msg->get_payload().c_str());
      if (!recv->Verify(verifier)) return;
      auto req = recv->packet_as_Request();
      if (req) {
        auto id = req->id();
        auto key = req->key()->str();
        auto payload = req->payload();
        auto it = mapped.find(key);
        auto handler = it == mapped.end() ? defaultHandler : it->second;
        flatbuffers::FlatBufferBuilder buf{256};
        flatbuffers::Offset<proto::Service::Send::SendPacket> packet;
        try {
          auto ret = handler({payload->data(), payload->size()});
          auto payload = buf.CreateVector(ret.data(), ret.size());
          auto respobj = proto::Service::Send::CreateResponse(buf, id, payload);
          packet = proto::Service::Send::CreateSendPacket(
              buf, proto::Service::Send::Send_Response, respobj.Union());
        } catch (std::exception const &ex) {
          auto exinfo = proto::CreateExceptionInfoDirect(buf, ex.what());
          auto exobj = proto::Service::Send::CreateException(buf, id, exinfo);
          packet = proto::Service::Send::CreateSendPacket(
              buf, proto::Service::Send::Send_Exception, exobj.Union());
        }
        buf.Finish(packet);
        ws.send(hdl, buf.GetBufferPointer(), buf.GetSize(), opcode::BINARY);
      }
    }
  } catch (...) {
    ep = std::current_exception();
    ws.stop();
  }
}

void Service::Connect(const std::string &endpoint) {
  websocketpp::lib::error_code ec;
  ws.init_asio();
  ws.set_user_agent("ws-gw/0");
  ws.clear_access_channels(websocketpp::log::alevel::all);
  ws.clear_error_channels(websocketpp::log::elevel::all);
  ws.set_message_handler(std::bind(&Service::OnMessage, this, _1, _2));
  ws.set_open_handler([this](websocketpp::connection_hdl co) {
    conhdr = co;
    flatbuffers::FlatBufferBuilder buf{64};
    buf.Finish(proto::Service::CreateHandshakeDirect(buf, "WS-GATEWAY", 0,
                                                     "test", "test", "0.0.0"));
    auto data = buf.GetBufferPointer();
    auto size = buf.GetSize();
    try {
      ws.send(co, data, size, opcode::BINARY);
    } catch (...) {
      ep = std::current_exception();
      ws.stop();
    }
  });
  auto con = ws.get_connection(endpoint, ec);
  if (ec)
    throw ParseFailed(ec);
  ws.connect(con);
  ws.run();
  if (ep)
    std::rethrow_exception(ep);
}

void Service::Disconnect() {
  ws.stop();
  booted = false;
}

void Service::Broadcast(const std::string_view &key, BufferView data) {
  if (!booted)
    return;
  flatbuffers::FlatBufferBuilder buf{256};
  auto skey = buf.CreateString(key);
  auto payload = buf.CreateVector(data.data(), data.size());
  auto broad = proto::Service::Send::CreateBroadcast(buf, skey, payload);
  buf.Finish(broad);
  try {
    ws.send(conhdr, buf.GetBufferPointer(), buf.GetSize(), opcode::BINARY);
  } catch (...) {
    ep = std::current_exception();
    ws.stop();
  }
}

} // namespace WsGw