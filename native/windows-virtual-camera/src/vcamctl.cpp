#include "chinaski_virtual_camera.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
void PrintUsage() {
  std::cout << "Usage: chinaski-vcamctl <status|register|unregister>\n";
}

void PrintResult(const char* action, int code) {
  if (code == 0) {
    std::cout << action << " succeeded for Chinaski Virtual Camera"
              << " using backend " << GetChinaskiVirtualCameraBackend() << "\n";
  } else {
    std::cerr << action << " failed for Chinaski Virtual Camera with HRESULT/code 0x"
              << std::hex << static_cast<unsigned int>(code) << std::dec
              << " using backend " << GetChinaskiVirtualCameraBackend() << "\n";
  }
}
}

int main(int argc, char** argv) {
  if (argc != 2) {
    PrintUsage();
    return 64;
  }

  const std::string command = argv[1];

  if (command == "status") {
    const int code = CheckChinaskiVirtualCameraSupport();
    PrintResult("Virtual camera API support check", code);
    return code == 0 ? 0 : 2;
  }

  if (command == "register") {
    const int code = RegisterChinaskiVirtualCamera();
    PrintResult("Register", code);
    return code == 0 ? 0 : 3;
  }

  if (command == "unregister") {
    const int code = UnregisterChinaskiVirtualCamera();
    PrintResult("Unregister", code);
    return code == 0 ? 0 : 4;
  }

  PrintUsage();
  return 64;
}
