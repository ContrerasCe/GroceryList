#include <crow.h>
#include "thelistbackend.h"
#include <crow/middlewares/cors.h>

#ifdef DELETE //windows delete method bugfix
#undef DELETE
#endif

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>


void addCorsHeaders(crow::response& response) {
    response.set_header("Access-Control-Allow-Origin", "*");
    response.set_header("Access-Control-Allow-Methods", "GET, POST, PATCH, DELETE, OPTIONS");
    response.set_header("Access-Control-Allow-Headers", "Content-Type");
}


crow::json::wvalue itemToJson(const Item& item) {
    crow::json::wvalue json;

    json["id"] = item.id;
    json["name"] = item.name;
    json["quantity"] = item.quantity;
    json["category"] = item.category;
    json["status"] = item.status;

    return json;
}

// Saves our items to a json file for storage
// Currently only allows for a chosen list

void saveItemsToFile(const std::string& fileName, const std::vector<Item>& items) {
    crow::json::wvalue root; //crow json type
    root["items"] = crow::json::wvalue::list();

    int index = 0;

    for (const Item& item : items) {
        root["items"][index] = itemToJson(item);
        index++;
    }

    std::ofstream file(fileName);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << fileName << " for writing.\n";
        return;
    }

    file << root.dump();

    file.close();
}

// File opening logic
// File takes precedence so we delete whatever we have stored and prio the save.

void loadItemsFromFile(const std::string& fileName, std::vector<Item>& items, int& nextId) {
    std::ifstream file(fileName);

    if (!file.is_open()) {
        std::cout << "No saved items file found. Starting with an empty list.\n";
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    auto savedData = crow::json::load(buffer.str());

    if (!savedData || !savedData.has("items")) {
        std::cerr << "Error: " << fileName << " is missing the items array.\n";
        return;
    }

    items.clear();

    int largestId = 0;

    for (int i = 0; savedData["items"].size() > i ; i++) {
        auto itemJson = savedData["items"][i];

        if (!itemJson.has("id") || !itemJson.has("name")) {
            continue;
        }

        Item item;
        item.id = static_cast<int>(itemJson["id"].i());
        item.name = std::string(itemJson["name"].s());
        item.quantity = itemJson.has("quantity") ? std::string(itemJson["quantity"].s()) : "";
        item.category = itemJson.has("category") ? std::string(itemJson["category"].s()) : "";
        item.status = itemJson.has("status") ? std::string(itemJson["status"].s()) : "needed";

        items.push_back(item);

        if (item.id > largestId) {
            largestId = item.id;
        }
    }

    nextId = largestId + 1;

    std::cout << "Loaded " << items.size() << " item(s) from " << fileName << ".\n";
}

std::string readTextFile(const std::string& fileName) {std::ifstream file(fileName);

    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

int main() {
    crow::App<crow::CORSHandler> app;
    std::vector<Item> items;
    std::mutex itemsMutex;
    int nextId = 1;
    const std::string fileName = "items.json";

    loadItemsFromFile(fileName, items, nextId);

    CROW_ROUTE(app, "/")
        ([]() {
        std::string html = readTextFile("C:/c++/TheGroceryList/thelistbackend/html/index.html");

        if (html == "") {
            crow::response response(404);
            response.body = "index.html not found";
            return response;
        }

        crow::response response(200);
        response.set_header("Content-Type", "text/html");
        response.body = html;

        return response;
            });

    CROW_ROUTE(app, "/health")([]() {
        crow::json::wvalue response;
        response["status"] = "ok";
        response["message"] = "Crow server is working";
        return response;
        });
 

    CROW_ROUTE(app, "/items").methods(crow::HTTPMethod::GET)
        ([&items, &itemsMutex]() {
        std::lock_guard<std::mutex> lock(itemsMutex);

        crow::json::wvalue jsonResponse;
        jsonResponse["items"] = crow::json::wvalue::list();

        int index = 0;

        for (const Item& item : items) {
            jsonResponse["items"][index] = itemToJson(item);
            index++;
        }

        crow::response response(200);
        response.set_header("Content-Type", "application/json");      
        response.body = jsonResponse.dump();

        return response;
            });
    
    CROW_ROUTE(app, "/items").methods(crow::HTTPMethod::POST)([&items, &itemsMutex, &nextId, &fileName](const crow::request& request) {
        auto body = crow::json::load(request.body);

        if (!body || !body.has("name")) {
            crow::response response(400);
            addCorsHeaders(response);
            response.body = "Missing required field: name";
            return response;
        }

        Item newItem;
        
        newItem.name = body["name"].s();
        newItem.quantity = body.has("quantity") ? std::string(body["quantity"].s()) : "";
        newItem.category = body.has("category") ? std::string(body["category"].s()) : "";
        newItem.status = body.has("status") ? std::string(body["status"].s()) : "needed";

        {
            std::lock_guard<std::mutex> lock(itemsMutex);
            newItem.id = nextId++;
            items.push_back(newItem);
            saveItemsToFile(fileName, items);
        }

        crow::json::wvalue json = itemToJson(newItem);

        crow::response response(201);
        response.set_header("Content-Type", "application/json");
 
        response.body = json.dump();

        return response;
            });

    CROW_ROUTE(app, "/items/<int>/status").methods(crow::HTTPMethod::PATCH) ([&items, &itemsMutex, &fileName](const crow::request& request, int id) {
        auto body = crow::json::load(request.body);

        if (!body || !body.has("status")) {
            crow::response response(400);
            response.body = "Missing required field: status";
            return response;
        }

        std::string newStatus = body["status"].s();

        std::lock_guard<std::mutex> lock(itemsMutex);

        for (Item& item : items) {
            if (item.id == id) {
                item.status = newStatus;
                saveItemsToFile(fileName, items);

                crow::json::wvalue json = itemToJson(item);

                crow::response response(200);
                response.set_header("Content-Type", "application/json");
                response.body = json.dump();

                return response;
            }
        }

        crow::response response(404);
        response.body = "Item not found";

        return response;
            });

    CROW_ROUTE(app, "/items/<int>/quantity").methods(crow::HTTPMethod::PATCH)
        ([&items, &itemsMutex, &fileName](const crow::request& request, int id) {
        auto body = crow::json::load(request.body);

        if (!body || !body.has("quantity")) {
            crow::response response(400);
            addCorsHeaders(response);
            response.body = "Missing required field: quantity";
            return response;
        }

        std::string newQuantity = body["quantity"].s();

        std::lock_guard<std::mutex> lock(itemsMutex);

        for (Item& item : items) {
            if (item.id == id) {
                item.quantity = newQuantity;

                saveItemsToFile(fileName, items);

                crow::json::wvalue json = itemToJson(item);

                crow::response response(200);
                response.set_header("Content-Type", "application/json");
                addCorsHeaders(response);
                response.body = json.dump();

                return response;
            }
        }

        crow::response response(404);      
        response.body = "Item not found";
        return response;
            });

    CROW_ROUTE(app, "/items/<int>").methods(crow::HTTPMethod::DELETE)([&items, &itemsMutex, &fileName](int id) {
        std::lock_guard<std::mutex> lock(itemsMutex);

        auto itemPosition = std::find_if(items.begin(), items.end(),
            [id](const Item& item) {
                return item.id == id;
            }
        );

        if (itemPosition == items.end()) {
            crow::response response(404);
            response.body = "Item not found";
            return response;
        }

        items.erase(itemPosition);
        saveItemsToFile(fileName, items);

        crow::json::wvalue json;
        json["message"] = "Item deleted";
        json["id"] = id;

        crow::response response(200);
        response.set_header("Content-Type", "application/json");
        response.body = json.dump();

        return response;
            });





    app.port(8080).run();

    return 0;
}