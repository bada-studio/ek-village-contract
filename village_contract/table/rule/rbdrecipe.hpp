enum building_type {
    bt_none = 0,
    bt_obstacle,
    bt_magic_water,
    bt_knight_house,
    bt_archer_house,
    bt_mage_house,
    bt_deco,
};

enum ingredient_type {
    igt_none,
    igt_material,
    igt_item,
};

struct rbdreciperow {
    uint64_t code = 0;
    uint8_t  level = 0;
    uint16_t hp = 0;
    uint16_t mw = 0;
    bool     buildable = 0;
    uint32_t param1 = 0;
    uint32_t param2 = 0;
    uint32_t point = 0;
    uint8_t  type = 0;
    uint8_t  ig_type1 = 0;
    uint16_t ig_code1 = 0;
    uint8_t  ig_count1 = 0;
    uint8_t  ig_type2 = 0;
    uint16_t ig_code2 = 0;
    uint8_t  ig_count2 = 0;
    uint8_t  ig_type3 = 0;
    uint16_t ig_code3 = 0;
    uint8_t  ig_count3 = 0;
    uint64_t v1 = 0;
    uint64_t v2 = 0;
};


struct [[eosio::table, eosio::contract(__MY__CONTRACT__)]] rbdrecipe {
    uint64_t code = 0;
    std::vector<rbdreciperow> rows;

    uint64_t primary_key() const {
        return code;
    }

    EOSLIB_SERIALIZE(
            rbdrecipe,
            (code)
            (rows)
    )
};

typedef eosio::multi_index< "rbdrecipe"_n, rbdrecipe> rbdrecipe_table;


