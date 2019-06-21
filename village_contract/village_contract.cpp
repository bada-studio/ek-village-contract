#include <vector>
#include <map>
#include <string>
#include <eosio/eosio.hpp>
#include <eosio/symbol.hpp>
#include <eosio/name.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <eosio/transaction.hpp>

#define MAINTENANCE 0
#define __MY__CONTRACT__ "village_contract"

using eosio::asset;
using eosio::permission_level;
using eosio::action;
using eosio::name;

#include "util/random_val.hpp"
#include "util/time_util.hpp"
#include "table/admin/rversion.hpp"
#include "table/rule/rbdrecipe.hpp"
#include "table/rule/rvillgen.hpp"
#include "table/rule/ritem.hpp"
#include "table/rule/ritemlv.hpp"
#include "table/rule/rmaterial.hpp"
#include "table/user/village.hpp"
#include "table/user/player.hpp"
#include "table/user/item.hpp"
#include "table/user/material.hpp"
#include "contract/rule_controller.hpp"

class [[eosio::contract]] village_contract : public eosio::contract {
private:
    name self;
    building empty_building;
    itemrow empty_itemrow;

public:
    village_contract(name s, name code, eosio::datastream<const char*> ds)
    : contract(s, code, ds)
    , self(_self) {
    }

    [[eosio::action]]
    void cbdrecipe(std::vector<rbdreciperow> rules) {
        rule_controller<rbdrecipe, rbdrecipe_table> controller(self, "bdrecipe"_n);
        controller.truncate_rules(100);

        std::vector<rbdrecipe> ruleset;
        int code = rules[0].code;
        ruleset.push_back(rbdrecipe());
        ruleset[0].code = code;
        int at = 0;

        for (int index = 0; index < rules.size(); index++) {
            auto rule = rules[index];
            if (rule.code == code) {
                ruleset[at].rows.push_back(rule);
            } else {
                at++;
                code = rule.code;
                ruleset.push_back(rbdrecipe());
                ruleset[at].code = code;
                ruleset[at].rows.push_back(rule);
            }
        }

        controller.create_rules(ruleset, false);
    }

    [[eosio::action]]
    void cvillgen(std::vector<rvillgenrow> rules) {
        rule_controller<rvillgen, rvillgen_table> controller(self, "villgen"_n);
        controller.truncate_rules(100);

        std::vector<rvillgen> ruleset;
        ruleset.push_back(rvillgen());
        ruleset[0].id = 1;

        for (int index = 0; index < rules.size(); index++) {
            ruleset[0].rows.push_back(rules[index]);
        }

        controller.create_rules(ruleset, false);
    }

    [[eosio::action]]
    void trule(name table, uint16_t size) {
        if (table == "bdrecipe"_n) {
            rule_controller<rbdrecipe, rbdrecipe_table> controller(self, "rbdrecipe"_n);
            controller.truncate_rules(size);
        } else if (table == "villgen"_n) {
            rule_controller<rvillgen, rvillgen_table> controller(self, "rvillgen"_n);
            controller.truncate_rules(size);
        } else {
            eosio::check(false, "could not find table");
        }
    }

    [[eosio::action]]
    void initvill(name from) {
        require_auth(from);

        player_table players("eosknightsio"_n, "eosknightsio"_n.value);
        auto piter = players.find(from.value);
        eosio::check(piter != players.cend(), "signup ek first");

        village_table table(self, self.value);
        auto iter = table.find(from.value);
        eosio::check(iter == table.cend(), "you already have a village");

        table.emplace(self, [&](auto &target) {
            target.owner = from;
            add_obstacle(target);
        });
    }

    [[eosio::action]]
    void digbd(name from, int32_t pos, int32_t id, std::vector<int32_t> pickaxes) {
        require_auth(from);

        // read items
        item_table items("eosknightsio"_n, "eosknightsio"_n.value);
        auto iter = items.find(from.value);
        eosio::check(iter != items.cend(), "no items");
        const auto &rows = iter->rows;
        
        // calculate total attack
        int attack = 0;
        for (int index = 0; index < pickaxes.size(); index++) {
            auto &item = get_item(rows, pickaxes[index]);

            eosio::check(is_pickaxe(item.code), "wrong pickaxes");
            attack += get_attack(item);
        }

        // reduce hp
        village_table table(self, self.value);
        auto bditer = table.find(from.value);
        eosio::check(bditer != table.cend(), "you don't have a village");
        auto bd = get_building(bditer->rows, pos);
        eosio::check(bd.id == id, "id not match");

        if (bd.hp <= attack) {
            // remove building
            std::vector<uint32_t> positions;
            positions.push_back(pos);

            // find reward mw rule
            rbdrecipe_table recipet(self, self.value);
            auto riter = recipet.find(bd.code);
            eosio::check(riter != recipet.cend(), "can not found rule");
            int32_t mw = riter->rows[bd.level - 1].mw;

            // add reward mw
            action(permission_level{ self, "active"_n },
                   "eosknightsio"_n, "vmw"_n,
                   std::make_tuple(from, mw)
            ).send();

            remove_buildings(from, positions, 0, 0);
        } else {
            table.modify(bditer, self, [&](auto &target) {
                int index = get_building_idx(target.rows, pos);
                target.rows[index].hp -= attack;
            });
        }

        // remove pickaxes
        action(permission_level{ self, "active"_n },
               "eosknightsio"_n, "vrmitem"_n,
               std::make_tuple(from, pickaxes)
        ).send();
    }

private:
    bool is_pickaxe(int32_t code) {
        if (code == 206 || code == 217 || code == 227) {
            return true;
        }

        return false;
    }

    int32_t get_attack(const itemrow &item) {
        // read items
        ritem_table rules("eosknightsio"_n, "eosknightsio"_n.value);
        auto rule = rules.find(item.code);
        eosio::check(rule != rules.cend(), "can not found item rule");

        // pickaxe has attack stat on a stat1
        uint32_t dna = item.dna;
        uint32_t rate1 = dna & 0xFF; 
        uint32_t stat = rule->stat1 + get_variation_value(rule->stat1_rand_range, (int)rate1);

        if (item.level > 1) {
            ritemlv_table lv_rules("eosknightsio"_n, "eosknightsio"_n.value);
            auto lv_rule = lv_rules.find(item.level);
            eosio::check(lv_rule != lv_rules.cend(), "can not found item level rule");
            auto bonus = lv_rule->bonus;
            stat = apply_bonus_stat(stat, bonus);
        }

        return stat;
    }

    void add_obstacle(village &village) {
        rvillgen_table gent(self, self.value);
        rbdrecipe_table recipet(self, self.value);
        auto ruleset = *gent.cbegin();

        // add random position
        std::vector<int> positions;
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 7; x++) {
                positions.push_back(y * 10 + x);
            }
        }

        shuffle(positions);
        std::map<int, std::vector<rbdreciperow>> hp_cache;

        for (int index = 0; index < ruleset.rows.size(); index++) {
            auto rule = ruleset.rows[index];
            int pos = positions[index];

            building bd;
            bd.id = ++village.last_id;
            bd.code = rule.code;
            bd.level = rule.level;
            bd.x = pos % 10;
            bd.y = pos / 10;

            auto cache = hp_cache.find(rule.code);
            if (cache != hp_cache.cend()) {
                bd.hp = cache->second[rule.level-1].hp;
            } else {
                auto iter = recipet.find(rule.code);
                eosio::check(iter != recipet.cend(), "can not found rule");
                auto recipe = iter->rows[rule.level-1];

                bd.hp = recipe.hp;
                hp_cache[rule.code] = iter->rows;
            }


            add_building(village, bd);
        }
    }

    int get_building_idx(const std::vector<building> &rows, int pos) {
        // binary search
        int left = 0;
        int right = rows.size() - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (rows[mid].pos() < pos) {
                left = mid + 1;
            } else if (pos < rows[mid].pos()) {
                right = mid - 1;
            } else {
                return mid;
            }
        }

        eosio::check(false, "can not found building");
        return -1;
    }

    const building& get_building(const std::vector<building> &rows, int pos) {
        return rows[get_building_idx(rows, pos)];
    }

    void add_building(village &village, const building &bd) {
        int left = 0;
        int right = village.rows.size() - 1;
        int mid = 0;

        while (left <= right) {
            mid = left + (right - left) / 2;
            if (village.rows[mid].pos() < bd.pos()) {
                left = mid + 1;
            } else if (bd.pos() < village.rows[mid].pos()) {
                right = mid - 1;
            } else {
                eosio::check(false, "already has building on same position");
            }
        }
        
        village.rows.insert(village.rows.cbegin() + left, bd);
    }

    void remove_buildings(name from, 
                          const std::vector<uint32_t> &positions, 
                          int8_t code, 
                          int8_t level) {
        village_table table(self, self.value);
        auto iter = table.find(from.value);
        eosio::check(iter != table.cend(), "could not found item");

        int found = false;
        table.modify(iter, self, [&](auto& item){
            for (int index = 0; index < positions.size(); ++index) {
                found = false;
                int pos = positions[index];

                // binary search
                auto &rows = item.rows;
                int left = 0;
                int right = rows.size() - 1;
                while (left <= right) {
                    int mid = left + (right - left) / 2;
                    if (rows[mid].pos() < pos) {
                        left = mid + 1;
                    } else if (pos < rows[mid].pos()) {
                        right = mid - 1;
                    } else {
                        auto &target = item.rows[mid];
                        if (code > 0 && target.code != code) {
                            eosio::check(false, "code not match");
                        }

                        if (level > 0 && target.level != level) {
                            eosio::check(false, "level not match");
                        }

                        item.rows.erase(item.rows.begin() + mid);
                        found = true;
                        break;
                    }
                }

                if (found == false) {
                    break;
                }
            }
        });

        eosio::check(found, "could not found item");
    }

    const itemrow& get_item(const std::vector<itemrow> &rows, int id) {
        // binary search
        int left = 0;
        int right = rows.size() - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (rows[mid].id < id) {
                left = mid + 1;
            } else if (id < rows[mid].id) {
                right = mid - 1;
            } else {
                return rows[mid];
            }
        }
        
        eosio::check(false, "can not found item");
        return empty_itemrow;
    }

    int get_variation_value(int amount, int rate) {
        if (rate < 0) {
            rate = 0;
        }

        if (rate > 100) {
            rate = 100;
        }

        return -amount + (amount * 2) * rate / 100;
    }

    uint32_t apply_bonus_stat(uint32_t stat, uint32_t bonus) {
        return stat * (bonus + 100) / 100;
    }


    template<class T>
    void shuffle(std::vector<T> &data) {
        auto now = time_util::now();
        random_val random(now, 0);

        int n = data.size();  
        while (n > 1) {  
            n--;  
            int k = random.range(n + 1);
            T value = data[k];
            data[k] = data[n];  
            data[n] = value;  
        }  
    }
};
