struct building {
    uint32_t id = 0;
    uint16_t code = 0;
    uint8_t level = 0;
    uint16_t hp = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint64_t v1;

    uint32_t pos() const {
        return y * 10 + x;
    }
};

struct [[eosio::table, eosio::contract(__MY__CONTRACT__)]] village {
    name owner;
    uint32_t last_id;
    uint8_t expand;
    uint64_t last_mw_sec;
    uint32_t last_ost_no;
    uint64_t last_ost_sec;
    uint64_t v1;
    uint64_t v2;
    uint64_t v3;

    // ordered by position
    std::vector<building> rows;

    uint64_t primary_key() const {
        return owner.value;
    }
};

typedef eosio::multi_index<"village"_n, village> village_table;
