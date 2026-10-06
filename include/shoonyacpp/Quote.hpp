#pragma once
#include <cstdint>
#include <string>

namespace shoonyacpp {

/// Market quote snapshot for an instrument (from /GetQuotes)
struct Quote {
  std::string exchange;       // Exchange (e.g. "NSE", "BSE", "NFO")
  std::string trading_symbol; // Trading symbol (tsym)
  std::string token;          // Exchange token (e.g. "26000")

  // Prices (using double for financial calculations)
  double lp = 0.0;            // Last Traded Price (LTP)
  double open = 0.0;          // Day open price (o)
  double high = 0.0;          // Day high price (h)
  double low = 0.0;           // Day low price (l)
  double close = 0.0;         // Previous close price (c)
  double change = 0.0;        // Absolute price change (cng)
  double net_change_pct = 0.0;// Percentage change (nc)

  // Discrete counts (using integers)
  int64_t volume = 0;         // Cumulative daily volume (v)
  int64_t open_interest = 0;  // Open interest for derivatives (oi)
  
  std::string expiry_date;    // Expiry date (exd) for derivatives!

  // Level-1 Market Depth (Best Bid / Best Ask)
  double best_bid_price = 0.0;// Best buy price (bp1)
  int32_t best_bid_qty = 0;   // Best buy quantity (bq1)
  double best_ask_price = 0.0;// Best sell price (sp1)
  int32_t best_ask_qty = 0;   // Best sell quantity (sq1)
};

} // namespace shoonyacpp
