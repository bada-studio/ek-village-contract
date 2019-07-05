struct building {
    uint32_t id = 0;
    uint16_t code = 0;
    uint8_t level = 0;
    uint16_t hp = 0;
    uint8_t x = 0;
    uint8_t y = 0;
    uint64_t v1 = 0;

    uint32_t pos() const {
        return y * 10 + x;
    }
};

struct mvparam {
    uint32_t origin = 0;
    uint32_t moveto = 0;
    uint32_t bdid = 0;
};

struct [[eosio::table, eosio::contract(__MY__CONTRACT__)]] village {
    name owner;
    uint32_t last_id = 0;
    uint8_t expand = 0;
    uint64_t last_mw_sec = 0;
    uint32_t last_ost_no = 0;
    uint64_t last_ost_sec = 0;
    uint32_t vp = 0;
    uint32_t v1 = 0;
    uint64_t v2 = 0;
    uint64_t v3 = 0;

    // ordered by position
    std::vector<building> rows;

    uint64_t primary_key() const {
        return owner.value;
    }
};

typedef eosio::multi_index<"village"_n, village> village_table;
