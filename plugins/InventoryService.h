#pragma once
#include <drogon/drogon.h>
#include "InventoryRepo.h"
class InventoryService: public drogon::Plugin<InventoryService>{
public:void initAndStart(const Json::Value&)override;void shutdown()override;
 drogon::Task<int64_t> createItem(const std::string&,const std::string&);drogon::Task<bool> updateItem(int64_t,const std::string&,const std::string&,bool);drogon::Task<std::vector<InventoryItem>> items();
 drogon::Task<int64_t> createLot(int64_t,double,int64_t,int64_t,int64_t,const std::string&);drogon::Task<bool> voidLot(int64_t,const std::string&);drogon::Task<std::vector<InventoryLot>> lots();
 drogon::Task<int64_t> createRecipe(int64_t,int64_t,double);drogon::Task<bool> updateRecipe(int64_t,double,bool);drogon::Task<std::vector<RecipeLink>> recipes(std::optional<int64_t>);
 drogon::Task<void> consumeOrder(int64_t,const std::vector<std::pair<int64_t,int>>&);
private:InventoryRepo repo_;
};
