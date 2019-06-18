struct rvillgenrow {
    uint8_t  code = 0;
    uint8_t  level = 0;
    uint8_t  hour = 0;
    uint64_t v1 = 0;
    uint64_t v2 = 0;
};

struct [[eosio::table, eosio::contract(__MY__CONTRACT__)]] rvillgen {
    uint64_t id = 0;
    std::vector<rvillgenrow> rows;

    uint64_t primary_key() const {
        return id;
    }

    EOSLIB_SERIALIZE(
            rvillgen,
            (id)
            (rows)
    )
};

typedef eosio::multi_index< "rvillgen"_n, rvillgen> rvillgen_table;
