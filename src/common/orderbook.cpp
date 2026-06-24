// orderbook.cpp

#include "orderbook.hpp"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <ranges>

void OrderBook::add_order(bool is_buy, double price, uint32_t quantity)
{
        // TODO: add fill and kill / good till cancel;
        add_order_impl(is_buy, price, quantity);
}

void OrderBook::status()
{
        const int limit = 10;
        std::cout << "asks_\n";
        for (const auto& i : asks_ | std::views::take(limit)) {
                std::cout << "{" << i.price << " " << i.quantity << "}\n";
        }

        std::cout << "bids_\n";
        for (const auto& i : bids_ | std::views::take(limit)) {
                std::cout << "{" << i.price << " " << i.quantity << "}\n";
        }
}

void OrderBook::add_order_impl(bool is_buy, double price, uint32_t quantity)
{
        if (is_buy) {
                uint32_t orders_filled = fill_order(asks_, price, quantity, is_buy);
                clean_order(asks_, orders_filled);
                insert_order(bids_, price, quantity, is_buy);
        } else {
                uint32_t orders_filled = fill_order(bids_, price, quantity, is_buy);
                clean_order(bids_, orders_filled);
                insert_order(asks_, price, quantity, is_buy);
        }
}

uint32_t OrderBook::fill_order(
    std::vector<Order>& vec_, double& price, uint32_t& quantity, bool& is_buy)
{
        auto best_ = vec_.begin();
        size_t orders_filled { 0 };
        while (!vec_.empty() && quantity > 0) {
                if (is_buy) {
                        if (price < best_->price) {
                                break;
                        }
                } else {
                        if (price > best_->price) {
                                break;
                        }
                }

                uint32_t trade_quantity = std::min(best_->quantity, quantity);
                best_->quantity -= trade_quantity;
                quantity -= trade_quantity;

                std::cout << trade_quantity << " units at " << best_->price << " filled\n";

                if (best_->quantity) {
                        break;
                } else {
                        ++orders_filled;
                        ++best_;
                        if (best_ == vec_.end())
                                break;
                }
        }
        return orders_filled;
}

void OrderBook::clean_order(std::vector<Order>& vec_, uint32_t& orders_filled)
{
        if (orders_filled > 0) {
                vec_.erase(vec_.begin(), vec_.begin() + orders_filled);
        }
}

void OrderBook::insert_order(
    std::vector<Order>& vec_, double& price, uint32_t& quantity, bool& is_buy)
{
        if (quantity > 0) {
                // upper bound as order's should be first come first serve,
                // older orders have priority
                auto it_vec_ = std::upper_bound(
                    vec_.begin(), vec_.end(), price, [is_buy](double price, const Order& order) {
                            return (is_buy ? (order.price < price) : (order.price > price));
                    });
                Order new_order { .id = ++order_id, .price = price, .quantity = quantity };

                vec_.insert(it_vec_, new_order);
        }
}
