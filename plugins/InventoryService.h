#pragma once
#include <drogon/drogon.h>
#include "InventoryRepo.h"
class InventoryService: public drogon::Plugin<InventoryService>{
public:void initAndStart(const Json::Value&)override;void shutdown()override;
 drogon::Task<int64_t> createItem(const std::string&,const std::string&);drogon::Task<bool> updateItem(int64_t,const std::string&,const std::string&,bool);drogon::Task<std::vector<InventoryItem>> items(int limit=100,int offset=0,bool activeOnly=false);
 drogon::Task<int64_t> createLot(int64_t,double,int64_t,int64_t,int64_t,const std::string&);drogon::Task<bool> voidLot(int64_t,const std::string&);drogon::Task<std::vector<InventoryLot>> lots(int limit=100,int offset=0,std::optional<int64_t> from=std::nullopt,std::optional<int64_t> to=std::nullopt);
 drogon::Task<int64_t> createRecipe(int64_t,int64_t,double);drogon::Task<bool> updateRecipe(int64_t,double,bool);drogon::Task<std::vector<RecipeLink>> recipes(std::optional<int64_t>,int limit=100,int offset=0,bool activeOnly=false);
 drogon::Task<std::vector<InventoryMovement>> movements(int limit,int offset,std::optional<int64_t> from,std::optional<int64_t> to,const std::optional<std::string>& kind);
 drogon::Task<void> consumeOrder(int64_t,const std::vector<std::pair<int64_t,int>>&);
private:InventoryRepo repo_;
};
