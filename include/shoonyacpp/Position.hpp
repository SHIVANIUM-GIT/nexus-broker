#pragma once
#include <cstdint>
#include <string>

namespace shoonyacpp {

// Data Transfer Object for a Position
struct Position {
  std::string prd;      // Product Type (e.g., "I", "C", "M")
  std::string exch;     // Exchange (e.g., "NSE", "BSE")
  std::string instname; // Instrument Name
  std::string symname;  // Symbol Name
  int32_t exd;          // Expiry Date (Epoch or YYYYMMDD as int)
  std::string optt;     // Option Type (CE, PE)
  double strprc;        // Strike Price
  int32_t buyqty;       // Buy Quantity
  int32_t sellqty;      // Sell Quantity
  int32_t netqty;       // Net Quantity
};

} // namespace shoonyacpp
