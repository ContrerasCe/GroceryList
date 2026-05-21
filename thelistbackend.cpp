#include <crow.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

struct Item {
    int id;
    std::string name;
    std::string quantity;
    std::string category;
    std::string status;
};

crow::json::wvalue itemToJson(const Item& item) {
    crow::json::wvalue json;

    json["id"] = item.id;
    json["name"] = item.name;
    json["quantity"] = item.quantity;
    json["category"] = item.category;
    json["status"] = item.status;

    return json;
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")([]() {
        return "The list test";
        });

    CROW_ROUTE(app, "/health")([]() {
        crow::json::wvalue response;
        response["status"] = "ok";
        response["message"] = "Crow server is working";
        return response;
        });

    app.port(8080).run();

    return 0;
}