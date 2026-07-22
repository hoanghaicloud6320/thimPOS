#include "InventoryService.h"
#include <stdexcept>
using namespace drogon;
void InventoryService::initAndStart(const Json::Value&){repo_.init_db();LOG_INFO<<"[InventoryService] started";}void InventoryService::shutdown(){}
Task<int64_t> InventoryService::createItem(const std::string&n,const std::string&u){if(n.empty()||u.empty())throw std::invalid_argument("name and unit are required");co_return co_await repo_.createItem(n,u);}
Task<bool> InventoryService::updateItem(int64_t id,const std::string&n,const std::string&u,bool a){if(id<=0||n.empty()||u.empty())throw std::invalid_argument("invalid inventory item");co_return co_await repo_.updateItem(id,n,u,a);}
Task<std::vector<InventoryItem>> InventoryService::items(int l,int o,bool a){co_return co_await repo_.listItems(l,o,a);}
Task<int64_t> InventoryService::createLot(int64_t i,double q,int64_t c,int64_t b,int64_t e,const std::string&n){if(i<=0||q<=0||c<0||b<=0||e<=b)throw std::invalid_argument("invalid lot; expiry must be after purchase");co_return co_await repo_.createLot(i,q,c,b,e,n);}
Task<bool> InventoryService::voidLot(int64_t i,const std::string&n){co_return co_await repo_.voidLot(i,n);}
Task<std::vector<InventoryLot>> InventoryService::lots(int l,int o,std::optional<int64_t>f,std::optional<int64_t>t){co_return co_await repo_.listLots(l,o,f,t);}
Task<int64_t> InventoryService::createRecipe(int64_t p,int64_t i,double q){if(p<=0||i<=0||q<=0)throw std::invalid_argument("invalid recipe link");co_return co_await repo_.createRecipe(p,i,q);}
Task<bool> InventoryService::updateRecipe(int64_t i,double q,bool a){if(i<=0||q<=0)throw std::invalid_argument("invalid recipe link");co_return co_await repo_.updateRecipe(i,q,a);}
Task<std::vector<RecipeLink>> InventoryService::recipes(std::optional<int64_t>p,int l,int o,bool a){co_return co_await repo_.listRecipes(p,l,o,a);}
Task<std::vector<InventoryMovement>> InventoryService::movements(int l,int o,std::optional<int64_t>f,std::optional<int64_t>t,const std::optional<std::string>&k){co_return co_await repo_.listMovements(l,o,f,t,k);}
Task<void> InventoryService::consumeOrder(int64_t o,const std::vector<std::pair<int64_t,int>>&i){co_await repo_.consumeOrder(o,i);}
