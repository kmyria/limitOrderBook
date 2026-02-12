// orderbook.hpp
#pragma once

#include <cstdint>
#include <vector>

class OrderBook {
public:
    void add_order(bool is_buy, double price, uint32_t quantity);
    // for testing
    void status();

private:
    struct Order {
        uint64_t id;
        double price;
        uint32_t quantity;
        // Side side; // implied by which vector it's in
    };

    void add_order_impl(bool is_buy, double price, uint32_t quantity);
    uint32_t fill_order(std::vector<Order>& vec_, double& price,
        uint32_t& quantity, bool& is_buy);
    void clean_order(std::vector<Order>& vec_, uint32_t& orders_filled);
    void insert_order(std::vector<Order>& vec_, double& price,
        uint32_t& quantity, bool& is_buy);

    std::vector<Order> bids_;
    std::vector<Order> asks_;

    uint32_t order_id = 0;
};
