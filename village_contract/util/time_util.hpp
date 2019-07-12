#pragma once

class time_util {
public:
    static uint32_t origin;
    static uint32_t day;
    static uint32_t hour;
    static uint32_t min;

public:
    static uint32_t now_shifted() {
        return eosio::current_time_point().sec_since_epoch() - origin;
    }

    static uint64_t now() {
        return eosio::current_time_point().sec_since_epoch();
    }
};

uint32_t time_util::origin = 1500000000;
uint32_t time_util::day = 24 * 3600;
uint32_t time_util::hour = 3600;
uint32_t time_util::min = 60;
