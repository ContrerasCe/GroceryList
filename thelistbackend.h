// thelistbackend.h : Include file for standard system include files,
// or project specific include files.

#pragma once


#include <string>
#include <vector>
#include <crow.h>

struct Item {
    int id;
    std::string name;
    std::string quantity;
    std::string category;
    std::string status;
};


crow::json::wvalue itemToJson(const Item& item);
void addCorsHeaders(crow::response& response);
void saveItemsToFile(const std::string& fileName, const std::vector<Item>& items);
void loadItemsFromFile(const std::string& fileName, std::vector<Item>& items, int& nextId);
bool isReadOnlyMode(int argc, char* argv[]);