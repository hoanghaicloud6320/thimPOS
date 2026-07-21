#pragma once
#include <drogon/HttpController.h>
using namespace drogon;
class InventoryController:public drogon::HttpController<InventoryController>{public:
 METHOD_LIST_BEGIN
 ADD_METHOD_TO(InventoryController::items,"/api/inventory/items",Get,"AuthNFilter","AuthZFilter"); ADD_METHOD_TO(InventoryController::createItem,"/api/inventory/items",Post,"AuthNFilter","AuthZFilter"); ADD_METHOD_TO(InventoryController::updateItem,"/api/inventory/items/{1}",Put,"AuthNFilter","AuthZFilter");
 ADD_METHOD_TO(InventoryController::lots,"/api/inventory/lots",Get,"AuthNFilter","AuthZFilter"); ADD_METHOD_TO(InventoryController::createLot,"/api/inventory/lots",Post,"AuthNFilter","AuthZFilter"); ADD_METHOD_TO(InventoryController::voidLot,"/api/inventory/lots/{1}",Delete,"AuthNFilter","AuthZFilter");
 ADD_METHOD_TO(InventoryController::recipes,"/api/inventory/recipes",Get,"AuthNFilter","AuthZFilter"); ADD_METHOD_TO(InventoryController::createRecipe,"/api/inventory/recipes",Post,"AuthNFilter","AuthZFilter"); ADD_METHOD_TO(InventoryController::updateRecipe,"/api/inventory/recipes/{1}",Put,"AuthNFilter","AuthZFilter");
 ADD_METHOD_TO(InventoryController::movements,"/api/inventory/movements",Get,"AuthNFilter","AuthZFilter");
 METHOD_LIST_END
 drogon::Task<drogon::HttpResponsePtr>items(drogon::HttpRequestPtr);drogon::Task<drogon::HttpResponsePtr>createItem(drogon::HttpRequestPtr);drogon::Task<drogon::HttpResponsePtr>updateItem(drogon::HttpRequestPtr,int64_t);drogon::Task<drogon::HttpResponsePtr>lots(drogon::HttpRequestPtr);drogon::Task<drogon::HttpResponsePtr>createLot(drogon::HttpRequestPtr);drogon::Task<drogon::HttpResponsePtr>voidLot(drogon::HttpRequestPtr,int64_t);drogon::Task<drogon::HttpResponsePtr>recipes(drogon::HttpRequestPtr);drogon::Task<drogon::HttpResponsePtr>createRecipe(drogon::HttpRequestPtr);drogon::Task<drogon::HttpResponsePtr>updateRecipe(drogon::HttpRequestPtr,int64_t);drogon::Task<drogon::HttpResponsePtr>movements(drogon::HttpRequestPtr);
};
