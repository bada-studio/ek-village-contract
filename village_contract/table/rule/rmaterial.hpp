struct [[eosio::table, eosio::contract("eosknightsio")]] rmaterial {
    uint64_t code = 0;
    uint8_t type = 0;
    uint8_t grade = 0;
    uint32_t relative_drop_rate = 0;
    uint16_t powder = 0;

    rmaterial() {
    }

    uint64_t primary_key() const {
        return code;
    }

    EOSLIB_SERIALIZE(
            rmaterial,
            (code)
            (type)
            (grade)
            (relative_drop_rate)
            (powder)
    )
};

typedef eosio::multi_index<"rmaterial"_n, rmaterial> rmaterial_table;
