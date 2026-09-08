// orderbook.hpp
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>

struct Order {
        uint64_t id;
        bool is_buy;
        uint32_t price; // cents
        uint32_t quantity;
};

template <bool Verbose = true>
class OrderBook {
    public:
        void add_order(bool is_buy, uint32_t price, uint32_t quantity);
        void status();

    private:
        using OrderList = std::list<Order>;

        void match_buy(uint32_t price, uint32_t& quantity);
        void match_sell(uint32_t price, uint32_t& quantity);
        void rest(bool is_buy, uint32_t price, uint32_t quantity);

        std::map<uint32_t, OrderList> asks_;
        std::map<uint32_t, OrderList> bids_;

        uint32_t order_id = 0;
};

template <bool Verbose>
void OrderBook<Verbose>::add_order(bool is_buy, uint32_t price, uint32_t quantity)
{
        if (is_buy) {
                match_buy(price, quantity);
                rest(true, price, quantity);
        } else {
                match_sell(price, quantity);
                rest(false, price, quantity);
        }
}

template <bool Verbose>
void OrderBook<Verbose>::status()
{
        const int limit = 10;
        std::cout << "asks_\n";
        int shown = 0;
        for (auto it = asks_.begin(); it != asks_.end() && shown < limit; ++it) {
                for (const Order& order : it->second) {
                        if (shown++ >= limit)
                                break;
                        std::cout << "{" << order.price << " " << order.quantity << "}\n";
                }
        }

        std::cout << "bids_\n";
        shown = 0;
        for (auto it = bids_.rbegin(); it != bids_.rend() && shown < limit; ++it) {
                for (const Order& order : it->second) {
                        if (shown++ >= limit)
                                break;
                        std::cout << "{" << order.price << " " << order.quantity << "}\n";
                }
        }
}

template <bool Verbose>
void OrderBook<Verbose>::match_buy(uint32_t price, uint32_t& quantity)
{
        while (quantity > 0 && !asks_.empty()) {
                auto level = asks_.begin();
                if (price < level->first)
                        break;

                OrderList& orders = level->second;
                while (quantity > 0 && !orders.empty()) {
                        Order& best = orders.front();
                        uint32_t trade_quantity = std::min(best.quantity, quantity);
                        best.quantity -= trade_quantity;
                        quantity -= trade_quantity;

                        if constexpr (Verbose) {
                                std::cout << trade_quantity << " units at " << best.price
                                          << " filled\n";
                        }

                        if (best.quantity == 0) {
                                orders.pop_front();
                        } else {
                                break;
                        }
                }

                if (orders.empty()) {
                        asks_.erase(level);
                }
        }
}

template <bool Verbose>
void OrderBook<Verbose>::match_sell(uint32_t price, uint32_t& quantity)
{
        while (quantity > 0 && !bids_.empty()) {
                auto level = bids_.end();
                --level;
                if (price > level->first)
                        break;

                OrderList& orders = level->second;
                while (quantity > 0 && !orders.empty()) {
                        Order& best = orders.front();
                        uint32_t trade_quantity = std::min(best.quantity, quantity);
                        best.quantity -= trade_quantity;
                        quantity -= trade_quantity;

                        if constexpr (Verbose) {
                                std::cout << trade_quantity << " units at " << best.price
                                          << " filled\n";
                        }

                        if (best.quantity == 0) {
                                orders.pop_front();
                        } else {
                                break;
                        }
                }

                if (orders.empty()) {
                        bids_.erase(level);
                }
        }
}

template <bool Verbose>
void OrderBook<Verbose>::rest(bool is_buy, uint32_t price, uint32_t quantity)
{
        if (quantity == 0)
                return;

        Order new_order { .id = ++order_id, .is_buy = is_buy, .price = price, .quantity = quantity };
        if (is_buy) {
                bids_[price].push_back(new_order);
        } else {
                asks_[price].push_back(new_order);
        }
}
