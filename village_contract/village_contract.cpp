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
#include "table/rule/rvillexpn.hpp"

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
    void cvillexpn( std::vector<rvillexpnrow> rules) {
        rule_controller<rvillexpn, rvillexpn_table> controller(self, "villexpn"_n);
        controller.truncate_rules(100);

        std::vector<rvillexpn> ruleset;
        ruleset.push_back(rvillexpn());
        ruleset[0].id = 1;

        for (int i = 0; i < rules.size(); i++) {
            ruleset[0].rows.push_back(rules[i]);
        }

        controller.create_rules(ruleset, false);
    }

    [[eosio::action]]
    void trule(name table, uint16_t size) {
        if (table == "bdrecipe"_n) {
            rule_controller<rbdrecipe, rbdrecipe_table> controller(self, "rbdrecipe"_n);
            controller.truncate_rules(size);
        } 
        else if (table == "villgen"_n) {
            rule_controller<rvillgen, rvillgen_table> controller(self, "rvillgen"_n);
            controller.truncate_rules(size);
        } 
        else if (table == "villexpn"_n) {
            rule_controller<rvillexpn, rvillexpn_table> controller(self, "villexpn"_n);
            controller.truncate_rules(size);
        } 
        else {
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

#ifdef TESTNET
    // todo remove it
    [[eosio::action]]
    void initvilltest(name from) {
        require_auth(from);

        player_table players("eosknightsio"_n, "eosknightsio"_n.value);
        auto piter = players.find(from.value);
        eosio::check(piter != players.cend(), "signup ek first");

        village_table table(self, self.value);
        auto iter = table.find(from.value);
        if (iter != table.cend()) {
            table.modify(iter, self, [&](auto &target) {
                target.rows.clear();
                add_obstacle(target);
            });
            return;
        }

        table.emplace(self, [&](auto &target) {
            target.owner = from;
            add_obstacle(target);
        });
    }
#endif

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

        gen_obstacle( from );

        // remove pickaxes
        action(permission_level{ self, "active"_n },
               "eosknightsio"_n, "vrmitem"_n,
               std::make_tuple(from, pickaxes)
        ).send();
    }

    [[eosio::action]]
    void newbd( name from, int32_t code, int32_t level, int32_t x, int32_t y,
                std::vector<int32_t> matids, std::vector<int32_t> itemids ) {
        require_auth(from);
        
        // read village
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "you don`t have a village");
        
        // check valid position
        uint8_t height = get_height( viter->expand );
        eosio::check( x >= 0 && x <= 6, "x position is wrong" );
        eosio::check( y >= 0 && y <= (height - 1), "y position is wrong" );

        // check empty position
        bool isEmpty = is_empty_pos( viter->rows, x, y );
        eosio::check( isEmpty, "dig the building first.");

        // get recipe
        rbdrecipe_table recipet(self, self.value);
        auto riter = recipet.find(code);
        eosio::check(riter != recipet.cend(), "can not found rule");
        auto recipe = riter->rows[level-1];
        eosio::check( recipe.buildable, "impossible to construct" );

        // check ingredient
        std::map<uint16_t, uint8_t> matigs;
        std::map<uint16_t, uint8_t> itemigs;
        std::map<uint16_t, uint8_t>::iterator igiter;

        if( igt_material == recipe.ig_type1 ) {
            igiter = matigs.find(recipe.ig_code1);
            if( igiter != matigs.end() ) {
                igiter->second += recipe.ig_count1;
            }
            else {
                matigs[recipe.ig_code1] = recipe.ig_count1;
            }
        }
        else if(igt_item == recipe.ig_type1 ) {
            igiter = itemigs.find(recipe.ig_code1);
            if( igiter != itemigs.end() ) {
                igiter->second += recipe.ig_count1;
            }
            else {
                itemigs[recipe.ig_code1] = recipe.ig_count1;
            }
        }

        if( igt_material == recipe.ig_type2 ) {
            igiter = matigs.find(recipe.ig_code2);
            if( igiter != matigs.end() ) {
                igiter->second += recipe.ig_count2;
            }
            else {
                matigs[recipe.ig_code2] = recipe.ig_count2;
            }
        }
        else if(igt_item == recipe.ig_type2 ) {
            igiter = itemigs.find(recipe.ig_code2);
            if( igiter != itemigs.end() ) {
                igiter->second += recipe.ig_count2;
            }
            else {
                itemigs[recipe.ig_code2] = recipe.ig_count2;
            }
        }

        if( igt_material == recipe.ig_type3 ) {
            igiter = matigs.find(recipe.ig_code3);
            if( igiter != matigs.end() ) {
                igiter->second += recipe.ig_count3;
            }
            else {
                matigs[recipe.ig_code3] = recipe.ig_count3;
            }
        }
        else if(igt_item == recipe.ig_type3 ) {
            igiter = itemigs.find(recipe.ig_code3);
            if( igiter != itemigs.end() ) {
                igiter->second += recipe.ig_count3;
            }
            else {
                itemigs[recipe.ig_code3] = recipe.ig_count3;
            }
        }

        if( 0 != matigs.size() ) {
            check_matigs(from, matigs, matids);
        }

        if( 0 != itemigs.size() ) {
            check_itemigs(from, itemigs, itemids);
        }

        send_stat( from, recipe );

        // add-building
        table.modify(viter, self, [&](auto& village){
            building bd;
            bd.id = ++village.last_id;
            bd.code = recipe.code;
            bd.level = recipe.level;
            bd.hp = recipe.hp;
            bd.x = x;
            bd.y = y;
            village.vp += recipe.point;
            add_building(village, bd);

            gen_obstacle( from );
        });
    }

    [[eosio::action]]
    void mergebd(name from, std::vector<int32_t> poses, std::vector<int32_t> bdids ) {
        require_auth(from);

        // check build count
        eosio::check( poses.size() >= 3 && bdids.size() >= 3, "at least three buildings are required" );

        // read building
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "you don't have a village");
        
        int code = 0; int level = 0; int pos = 0;
        std::vector<building> delbds;

        for (int i = 0; i < poses.size(); ++i) {
            auto bd = get_building(viter->rows, poses[i]);
            eosio::check( bd.id == bdids[i], "id is not match" );

            if( 0 == i ) {
                code = bd.code;
                level = bd.level;
                pos = bd.pos();
                delbds.push_back( bd );
                continue;
            }

            if( bd.code == code && bd.level == level ) {
                delbds.push_back( bd );
            }

            if( delbds.size() >= 3 ) {
                break;
            }
        }

        eosio::check( delbds.size() >= 3, "not enough building." );

        // find next level recipe
        rbdrecipe_table recipet(self, self.value);
        auto riter = recipet.find(code);
        eosio::check(riter != recipet.cend(), "can not found recipe rule");
        eosio::check( level < riter->rows.size(), "no next level" ); 
        auto recipe = riter->rows[level];

        // new building
        building bd;
        bd.code = recipe.code;
        bd.level = recipe.level;
        bd.hp = recipe.hp;
        bd.x = pos % 10;
        bd.y = pos / 10;

        std::vector<building> upbds;
        upbds.push_back( bd );

        send_stat( from, recipe );

        // update and delete
        uplete_buildings( from, upbds, delbds, true, recipe.point );
        
        gen_obstacle( from );
    }

    [[eosio::action]]
    void movebd( name from, std::vector<mvparam> mvlist ) {
        require_auth(from);
        
        // read village
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "you don`t have a village");

        uint8_t height = get_height(viter->expand);
        std::vector<building> delbds;
        std::vector<building> upbds;

        for( int i = 0; i < mvlist.size(); ++i ) {
            uint32_t origin = mvlist[i].origin;
            uint8_t movetox = mvlist[i].moveto % 10;
            uint8_t movetoy = mvlist[i].moveto / 10;
            uint32_t id = mvlist[i].bdid;
            
            // check valid to-pos
            eosio::check( movetox >= 0 && movetox <= 6, "x position is wrong" );
            eosio::check( movetoy >= 0 && movetoy <= height - 1, "y position is wrong" );

            // check empty position
            eosio::check( is_empty_pos( viter->rows, movetox, movetoy ), "dig the building first.");

            // get bd.
            auto bd = get_building( viter->rows, origin );
            eosio::check( bd.id == id, "id is not match");
            eosio::check( bd.code > 3, "obstacle can not move" );

            delbds.push_back( bd );

            building movebd;
            movebd.id = bd.id;
            movebd.code = bd.code;
            movebd.level = bd.level;
            movebd.hp = bd.hp;
            movebd.x = movetox;
            movebd.y = movetoy;
            upbds.push_back( movebd );
        }

        // update and delete
        uplete_buildings( from, upbds, delbds, false, 0 );

        gen_obstacle( from );
    }

    [[eosio::action]]
    void collectmw( name from ) {
        require_auth(from);

        rbdrecipe_table recipet(self, self.value);

        // read village
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "you don't have a village");

        uint64_t now = time_util::now();
        uint64_t diffmin = (now - viter->last_mw_sec) / time_util::min;
        int32_t totmw = 0;
        
        for( int i =0; i < viter->rows.size(); ++i ) {
            auto bd = viter->rows[i];

            if( 4 /*mw storage*/ != bd.code ) {
                continue;
            }

            // find reward mw rule
            auto riter = recipet.find(bd.code);
            eosio::check(riter != recipet.cend(), "can not found rule");
            uint32_t mwperhour = riter->rows[bd.level - 1].param1; // amount per hour 
            uint32_t mwmaxaccu = riter->rows[bd.level - 1].param2; // maximum accumulation amount

            uint32_t mw = (mwperhour / (float)time_util::min * diffmin);
            totmw += mw >= mwmaxaccu ? mwmaxaccu : mw;
        }

        gen_obstacle( from );

        if( totmw > 0 ) {
            table.modify(viter, self, [&](auto& village){
                village.last_mw_sec = now;
            });

            action(permission_level{ self, "active"_n },
                   "eosknightsio"_n, "vmw"_n,
                   std::make_tuple(from, totmw)
            ).send();
        }
    }
    
    [[eosio::action]]
    void expandvill( name from ) {
        require_auth(from);
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "you don't have a village");

        uint8_t expn = get_expand( viter->expand, viter->vp );
        if( viter->expand == expn ) {
            return;
        }

        table.modify(viter, self, [&](auto& village){
            village.expand = expn;
            gen_obstacle( from );
        });
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

    void remove_buildings(name from, const std::vector<uint32_t> &positions, int8_t code, int8_t level) {
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

    void uplete_buildings(name from, std::vector<building> &updatebds, 
                            const std::vector<building> &delbds, bool updateid, uint32_t vp) {
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "have no village");

        table.modify(viter, self, [&](auto &village) {
            int left = 0;
            int right = 0;
            int mid = 0;
            uint32_t pos = 0;

            // delete first
            bool found = false;
            for (int i = 0; i < delbds.size(); ++i) {
                auto &rows = village.rows;
                left = 0; 
                right = rows.size() - 1;
                pos = delbds[i].pos();

                while (left <= right) {
                    mid = left + (right - left) / 2;
                    if (rows[mid].pos() < pos) {
                        left = mid + 1;
                    } else if (pos < rows[mid].pos()) {
                        right = mid - 1;
                    } else {
                        auto &target = village.rows[mid];
                        if (delbds[i].code > 0 && target.code != delbds[i].code) {
                            eosio::check(false, "code not match");
                        }

                        if (delbds[i].level > 0 && target.level != delbds[i].level) {
                            eosio::check(false, "level not match");
                        }

                        village.rows.erase(village.rows.begin() + mid);
                        found = true;
                        break;
                    }
                }

                if (false == found) {
                    eosio::check(false, "delete build fail.");
                    break;
                }
            }

            // update
            for (int i = 0; i < updatebds.size(); ++i) {
                auto &rows = village.rows;
                left = 0;
                right = rows.size() - 1;
                pos = updatebds[i].pos();
                while (left <= right) {
                    mid = left + (right - left) / 2;
                    if (rows[mid].pos() < pos) {
                        left = mid + 1;
                    } else if (pos < rows[mid].pos()) {
                        right = mid - 1;
                    } else {
                        eosio::check(false, "update building fail. exist building");
                    }
                }
                if (true == updateid) {
                    updatebds[i].id = ++village.last_id;
                }
                village.rows.insert(village.rows.cbegin() + left, updatebds[i]);
            }

            village.vp += vp;
        });
    }

    void gen_obstacle(name from) {
        village_table table(self, self.value);
        auto viter = table.find(from.value);
        eosio::check(viter != table.cend(), "have no village");

        rvillgen_table gent(self, self.value);
        auto ruleset = *gent.cbegin();

        uint32_t nextno = viter->last_ost_no + 1;
        if (nextno >= ruleset.rows.size()) {
            nextno = 1;
        }
        auto rule = ruleset.rows[nextno - 1];

        // check time
        uint64_t now = time_util::now();
        uint16_t diffh = (now - viter->last_ost_sec) / time_util::hour;
        if (diffh < rule.hour) {
            return;
        }

        // find empty pos
        uint32_t pos = get_empty_pos(viter->rows, viter->expand);

        // find recipe
        rbdrecipe_table recipet(self, self.value);
        auto iter = recipet.find(rule.code);
        eosio::check(iter != recipet.cend(), "can not found rule");
        auto recipe = iter->rows[rule.level - 1];

        table.modify(viter, self, [ & ](auto & village) {
            building bd;
            bd.id = ++village.last_id;
            bd.code = rule.code;
            bd.level = rule.level;
            bd.hp = recipe.hp;
            bd.x = pos % 10;
            bd.y = pos / 10;

            village.last_ost_no = nextno;
            uint16_t remainhour = diffh - rule.hour >= 48 ? 48 : (diffh - rule.hour);
            village.last_ost_sec = now - remainhour * time_util::hour;
            add_building(village, bd);
        });
    }

    bool is_empty_pos(const std::vector<building> &rows, int32_t x, int32_t y ) {
        int pos = y * 10 + x;
        int left = 0;
        int right = rows.size() - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            if (rows[mid].pos() < pos) {
                left = mid + 1;
            } else if (pos < rows[mid].pos()) {
                right = mid - 1;
            } else {
                return false;
            }
        }
        return true;
    }

    void check_matigs( name from, std::map<uint16_t, uint8_t> &igs, const std::vector<int32_t> &matids ) {
        material_table mat("eosknightsio"_n, "eosknightsio"_n.value);
        auto imat = mat.find(from.value);
        eosio::check(imat != mat.cend(), "have no material");

        std::vector<int32_t> deleteids;
        for (int i = 0; i < matids.size(); i++) {
            auto &mat =  imat->get_material(matids[i]);
            auto it = igs.find(mat.code);
            if (it != igs.cend()) {
                eosio::check(mat.saleid == 0, "material is on sale");
                --it->second;
                if( it->second <= 0 ) {
                    igs.erase( it );
                }
                deleteids.push_back( matids[i] );
            }
        }
        eosio::check( igs.size() == 0, "not enough material" );

        // delete material
        if( deleteids.size() > 0 ) {
            action(permission_level{ self, "active"_n },
                "eosknightsio"_n, "vrmmat"_n,
                std::make_tuple(from, deleteids)
            ).send();
        }
    }

    void check_itemigs( name from, std::map<uint16_t, uint8_t> &igs, const std::vector<int32_t> &itemids ) {
        item_table items("eosknightsio"_n, "eosknightsio"_n.value);
        auto iter = items.find(from.value);
        eosio::check(iter != items.cend(), "have no items");
        const auto &rows = iter->rows;

        int attack = 0;
        std::vector<int32_t> deleteids;
        for (int i = 0; i < itemids.size(); i++) {
            auto &item = get_item(rows, itemids[i]);

            auto it = igs.find(item.code);
            if (it != igs.cend()) {
                --it->second;
                if( it->second <= 0 ) {
                    igs.erase( it );
                }
                deleteids.push_back( itemids[i] );
            }
        }
        eosio::check( igs.size() == 0, "not enough item" );

        // delete item
        if( deleteids.size() > 0 ) {
            action(permission_level{ self, "active"_n },
                "eosknightsio"_n, "vrmitem"_n,
                std::make_tuple(from, deleteids)
            ).send();
        }
    }

    uint8_t get_expand( uint8_t currexpn, uint32_t point ) {
        rvillexpn_table expt(self, self.value);
        auto ruleset = *expt.cbegin();

        if( currexpn + 1 >= ruleset.rows.size() ) {
            return currexpn;
        }

        if( point >= ruleset.rows[currexpn + 1].point ) {
            return currexpn + 1;
        }

        return currexpn;
    }

    uint8_t get_height( uint8_t expand ) {
        rvillexpn_table expt(self, self.value);
        auto ruleset = *expt.cbegin();
        eosio::check( expand < ruleset.rows.size(), "expand value is wrong." );
        return ruleset.rows[expand].height;
    }

    void send_stat(name from, const rbdreciperow &recipe) {
        int8_t knt = 1;

        if( bt_knight_house == recipe.type ) {
            knt = 1;
        }
        else if( bt_archer_house == recipe.type ) {
            knt = 2;
        }
        else if( bt_mage_house == recipe.type ) {
            knt = 3;
        }
        else {
            return;
        }

        action(permission_level{ self, "active"_n },
            "eosknightsio"_n, 
            "vstat"_n,
            std::make_tuple(from, knt, recipe.param1, recipe.param2)
        ).send();
    };

    uint32_t get_empty_pos( const std::vector<building> &rows, uint8_t expand ) {
        std::vector<uint32_t> empty_pos;
        int pos = 0;
        int left = 0;
        int right = 0;
        int mid = 0;
        uint8_t height = get_height(expand);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < 7; x++) {
                pos = y * 10 + x;
                left = 0;
                right = rows.size() - 1;

                while (left <= right) {
                    mid = left + (right - left) / 2;
                    if (rows[mid].pos() < pos) {
                        left = mid + 1;
                    } else if (pos < rows[mid].pos()) {
                        right = mid - 1;
                    } else {
                        empty_pos.push_back( pos );
                        break;
                    }
                }
            }
        }

        // get random pos
        auto now = time_util::now();
        random_val random(now, 0);
        int rp = random.range(empty_pos.size());

        return empty_pos[rp];
    };
};
