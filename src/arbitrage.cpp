#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

// Represents a bid or ask entry in an orderbook
struct Entry {
	std::string exchange;  // Exchange name (e.g., "ex1")
	double fee;            // Transaction fee as a decimal (e.g., 0.00024)
	double price;          // Price per unit
	long volume;           // Available volume at this price
};

// Represents an executed arbitrage trade
struct Trade {
	std::string buy_exchange;
	std::string sell_exchange;
	double buy_fee;
	double sell_fee;
	double buy_price;
	double sell_price;
	long volume;
	double profit_per_unit;
	double net_profit;
};

// Parses a key string "exchange-fee" into components
auto parse_key(const std::string &key) -> std::pair<std::string, double> {
	const auto pos = key.find('-');
	const auto exch = key.substr(0, pos);
	const auto fee = std::stod(key.substr(pos + 1));
	return {exch, fee};
}

// Converts raw map of string->(price, volume) into map of string->Entry struct
auto build_entries(
    const std::unordered_map<std::string, std::pair<double, long>> &raw)
    -> std::unordered_map<std::string, Entry> {
	auto entries = std::unordered_map<std::string, Entry>();
	for (const auto &[key, val] : raw) {
		const auto &[price, volume] = val;
		const auto &[exchange, fee] = parse_key(key);
		entries[key] = Entry{exchange, fee, price, volume};
	}
	return entries;
}

// Attempts to find the most profitable arbitrage opportunity and execute it,
// updating quantities accordingly. Repeats until no profitable arbitrage remains.
// Returns list of executed trades.
auto find_best_arbitrage(std::unordered_map<std::string, Entry> &bids,
                         std::unordered_map<std::string, Entry> &asks)
    -> std::vector<Trade> {
	auto trades = std::vector<Trade>();

	// Main loop: repeatedly find best profitable trade and execute it
	while (true) {
		auto best_trade = std::optional<Trade>();
		double best_profit_per_unit = 0.0;

		// Iterate over all ask entries (we buy from these)
		for (const auto &[a_key, ask] : asks) {
			if (ask.volume <= 0)  // skip exhausted asks
				continue;

			// Iterate over all bid entries (we sell to these)
			for (const auto &[b_key, bid] : bids) {
				if (bid.volume <= 0)  // skip exhausted bids
					continue;
				if (ask.exchange ==
				    bid.exchange)  // skip same-exchange arbitrage
					continue;

				// Calculate effective cost to buy: price + fee
				const auto effective_cost = ask.price * (1.0 + ask.fee);
				// Calculate proceeds from selling: price - fee
				const auto effective_proceed = bid.price * (1.0 - bid.fee);
				const auto profit_per_unit = effective_proceed - effective_cost;

				// If profitable and better than current best, update best_trade
				if (profit_per_unit > best_profit_per_unit) {
					const auto trade_qty = std::min(ask.volume, bid.volume);
					const auto net_profit = profit_per_unit * trade_qty;
					best_trade = {ask.exchange, bid.exchange,    ask.fee,
					              bid.fee,      ask.price,       bid.price,
					              trade_qty,    profit_per_unit, net_profit};
					best_profit_per_unit = profit_per_unit;
				}
			}
		}

		// No profitable trade found, stop
		if (!best_trade.has_value())
			break;

		// Record the best trade
		trades.push_back(best_trade.value());

		// Decrement quantities for matched entries
		auto update_volume = [](auto &book, const std::string &exchange,
		                        const double fee, const double price,
		                        const double trade_qty) {
			for (auto &[_, entry] : book) {
				if (entry.exchange == exchange && entry.fee == fee &&
				    entry.price == price) {
					entry.volume -= trade_qty;
					break;
				}
			}
		};

		update_volume(asks, best_trade->buy_exchange, best_trade->buy_fee,
		              best_trade->buy_price, best_trade->volume);
		update_volume(bids, best_trade->sell_exchange, best_trade->sell_fee,
		              best_trade->sell_price, best_trade->volume);
	}

	return trades;
}

// Prints executed trades nicely formatted to console
auto print_trades(const std::vector<Trade> &trades) -> void {
	double total_profit = 0.0;
	std::cout << std::fixed << std::setprecision(8);
	std::cout << "\nExecuted Arbitrage Trades:\n";
	for (const auto &t : trades) {
		std::ostringstream os;
		os << " Buy from " << t.buy_exchange << " @ " << t.buy_price
		   << " (fee=" << t.buy_fee << ")"
		   << ", sell to " << t.sell_exchange << " @ " << t.sell_price
		   << " (fee=" << t.sell_fee << ")"
		   << ", vol=" << t.volume << ", ppu=" << t.profit_per_unit
		   << ", net=" << t.net_profit << "\n";
		std::cout << os.str();
		total_profit += t.net_profit;
	}
	std::cout << "\nTotal Net Profit: " << total_profit << "\n";
}

auto print_order_book(const std::unordered_map<std::string, Entry> &book,
                      const std::string &label) -> void {
	std::cout << "\n" << label << ":\n";
	std::cout << std::fixed << std::setprecision(8);
	for (const auto &[key, entry] : book) {
		// print exchange (e.g., "ex1") instead of full key
		std::cout << "  " << entry.exchange << " -> price: " << entry.price
		          << ", fee: " << entry.fee << ", volume: " << entry.volume
		          << "\n";
	}
}

auto main() -> int {
	// Raw input: maps of string keys to (price, volume)
	std::unordered_map<std::string, std::pair<double, long>> raw_bids = {
	    {"ex1-0.00024", {0.95, 10}},
	    {"ex2-0.0005", {0.98, 10}},
	    {"ex3-0.0002", {1.00, 5}},
	    {"ex4-0.00025", {1.02, 4}},
	    {"ex5-0", {0.94, 11}}};

	std::unordered_map<std::string, std::pair<double, long>> raw_asks = {
	    {"ex1-0.00024", {0.96, 50}},
	    {"ex2-0.0005", {1.03, 8}},
	    {"ex3-0.0002", {1.01, 2}},
	    {"ex4-0.00025", {1.04, 5}},
	    {"ex5-0", {0.96, 3}}};

	// Convert raw data into entries with parsed keys
	auto bids = build_entries(raw_bids);
	auto asks = build_entries(raw_asks);

	// Print initial order books
	print_order_book(bids, "Initial Bids");
	print_order_book(asks, "Initial Asks");
	

	// Find and execute arbitrage trades
	const auto trades = find_best_arbitrage(bids, asks);
	
	// Print executed trades and remaining order books
	print_trades(trades);
	print_order_book(bids, "Remaining Bids");
	print_order_book(asks, "Remaining Asks");
	

	return 0;
}
