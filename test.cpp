#include "include/ws-gw.h"
#include <exception>
#include <iostream>
#include <stdexcept>

int main() {
  WsGw::Service srv{[](WsGw::BufferView const &inp) -> WsGw::Buffer {
    throw new std::runtime_error("Not implemented");
  }};

  srv.RegisterHandler("tip",
                      [](auto &inp) -> WsGw::Buffer { return {"test"}; });

  try {
    srv.Connect("ws://127.0.0.1:8818/");
    std::cout << "connected" << std::endl;
    try {
      srv.Wait();
    } catch (std::exception &ex) {
      std::cerr << "exception in wait: " << ex.what() << std::endl;
    }
  } catch (std::exception &ex) {
    std::cerr << "exception in connect: " << ex.what() << std::endl;
  }
}