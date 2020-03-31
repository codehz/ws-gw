#include "include/ws-gw.h"
#include <stdexcept>

int main() {
  WsGw::Service srv{[](WsGw::BufferView const &inp) -> WsGw::Buffer {
    throw new std::runtime_error("Not implemented");
  }};

  srv.RegisterHandler("tip", [](auto &inp) -> WsGw::Buffer { return {"test"}; });

  srv.Connect("ws://127.0.0.1:8818/");
}