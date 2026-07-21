#pragma once
#include <drogon/drogon.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct InventoryItem { int64_t id{}; std::string name, unit; bool is_active{}; int64_t created_at{}, updated_at{}; double on_hand{}; };
struct InventoryLot { int64_t id{}, item_id{}; std::string item_name; double quantity{}, remaining{}; int64_t unit_cost{}, purchased_at{}, expires_at{}, created_at{}; bool is_void{}; };
struct RecipeLink { int64_t id{}, product_id{}, item_id{}; std::string item_name, unit; double quantity{}; bool is_active{}; };

class InventoryRepo {
public:
 void init_db();
 drogon::Task<int64_t> createItem(const std::string&, const std::string&);
 drogon::Task<bool> updateItem(int64_t,const std::string&,const std::string&,bool);
 drogon::Task<std::vector<InventoryItem>> listItems();
 drogon::Task<int64_t> createLot(int64_t,double,int64_t,int64_t,int64_t,const std::string&);
 drogon::Task<bool> voidLot(int64_t,const std::string&);
 drogon::Task<std::vector<InventoryLot>> listLots();
 drogon::Task<int64_t> createRecipe(int64_t,int64_t,double);
 drogon::Task<bool> updateRecipe(int64_t,double,bool);
 drogon::Task<std::vector<RecipeLink>> listRecipes(std::optional<int64_t>);
 drogon::Task<void> consumeOrder(int64_t,const std::vector<std::pair<int64_t,int>>&);
private: drogon::orm::DbClientPtr db() const;
};
