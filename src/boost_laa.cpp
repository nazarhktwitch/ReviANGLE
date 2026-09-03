// Boost: Large Address Aware runtime patch
// Flips IMAGE_FILE_LARGE_ADDRESS_AWARE in the PE header of the loaded GD.exe.
// This lets the 32-bit process use up to 4 GB of virtual memory instead of 2
// GB. Note: requires /LARGEADDRESSAWARE in the OS boot config or 64-bit Windows
// (default).

#include "angle_loader.hpp"
#include "common/pe_utils.hpp"
#include "config.hpp"
#include <windows.h>


namespace boost_laa {

void apply() {
  if (!Config::get().large_address_aware)
    return;

  HMODULE gd = GetModuleHandleA(nullptr);
  auto *nt = pe::headers(gd);
  if (!nt) {
    angle::log("laa: couldn't read PE headers");
    return;
  }

  if (nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE) {
    angle::log("laa: already set");
    return;
  }

  if (pe::setCharacteristic(gd, IMAGE_FILE_LARGE_ADDRESS_AWARE, true)) {
    angle::log("laa: patched - 4 GB virtual memory enabled");
  } else {
    angle::log("laa: VirtualProtect failed, may need admin rights");
  }
}
} // namespace boost_laa
