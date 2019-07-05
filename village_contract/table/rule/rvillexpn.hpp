struct rvillexpnrow {
    uint8_t expand = 0;
    uint8_t height = 0;
    uint32_t point = 0;
};

struct [[eosio::table, eosio::contract(__MY__CONTRACT__)]] rvillexpn {
    uint64_t id =0;
    std::vector<rvillexpnrow> rows;

    uint64_t primary_key() const {
        return id;
    }

    EOSLIB_SERIALIZE(
            rvillexpn,
            (id)
            (rows)
    )
};

typedef eosio::multi_index<"rvillexpn"_n, rvillexpn> rvillexpn_table;