#!/bin/bash
# Creates a realistic 30+ file project for context rotation testing
# This is a simplified web framework with models, controllers, middleware, utils
DIR="${1:-/tmp/cortex-large-project}"
rm -rf "$DIR"
mkdir -p "$DIR/src/models" "$DIR/src/controllers" "$DIR/src/middleware" "$DIR/src/utils" "$DIR/src/services"

# ─── Config ────────────────────────────────────────────────
cat > "$DIR/src/config.hpp" << 'EOF'
#pragma once
#include <string>
#include <unordered_map>
struct Config {
    std::string db_host = "localhost";
    int db_port = 5432;
    std::string db_name = "appdb";
    int server_port = 8080;
    int max_connections = 100;
    std::string log_level = "info";
    std::unordered_map<std::string, std::string> custom;
    std::string get(const std::string& key, const std::string& def = "") const {
        auto it = custom.find(key);
        return it != custom.end() ? it->second : def;
    }
};
EOF

# ─── Models ────────────────────────────────────────────────
cat > "$DIR/src/models/user.hpp" << 'EOF'
#pragma once
#include <string>
#include <vector>
struct User {
    int id;
    std::string username;
    std::string email;
    std::string password_hash;
    std::string role; // "admin", "user", "guest"
    bool active;
    std::string created_at;
};
EOF

cat > "$DIR/src/models/product.hpp" << 'EOF'
#pragma once
#include <string>
struct Product {
    int id;
    std::string name;
    std::string category;
    double price;
    int stock;
    std::string sku;
    bool available;
};
EOF

cat > "$DIR/src/models/order.hpp" << 'EOF'
#pragma once
#include <string>
#include <vector>
struct OrderItem {
    int product_id;
    int quantity;
    double unit_price;
    double total() const { return quantity * unit_price; }
};
struct Order {
    int id;
    int user_id;
    std::vector<OrderItem> items;
    std::string status; // "pending", "confirmed", "shipped", "delivered", "cancelled"
    std::string created_at;
    double total() const {
        double t = 0;
        for (const auto& i : items) t += i.total();
        return t;
    }
};
EOF

cat > "$DIR/src/models/review.hpp" << 'EOF'
#pragma once
#include <string>
struct Review {
    int id;
    int user_id;
    int product_id;
    int rating; // 1-5
    std::string comment;
    std::string created_at;
};
EOF

cat > "$DIR/src/models/category.hpp" << 'EOF'
#pragma once
#include <string>
#include <vector>
struct Category {
    int id;
    std::string name;
    std::string description;
    int parent_id; // -1 for root
    std::vector<int> product_ids;
};
EOF

# ─── Utils ─────────────────────────────────────────────────
cat > "$DIR/src/utils/string_utils.hpp" << 'EOF'
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
namespace utils {
inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    return s.substr(start, s.find_last_not_of(" \t\n\r") - start + 1);
}
inline std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> r;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, d)) r.push_back(tok);
    return r;
}
inline std::string to_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}
}
EOF

cat > "$DIR/src/utils/csv_parser.hpp" << 'EOF'
#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
namespace utils {
struct CsvRow {
    std::vector<std::string> fields;
    std::string get(int i) const { return i < fields.size() ? fields[i] : ""; }
};
inline std::vector<CsvRow> parse_csv(const std::string& path) {
    std::vector<CsvRow> rows;
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        CsvRow row;
        std::istringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) row.fields.push_back(field);
        rows.push_back(row);
    }
    return rows;
}
}
EOF

cat > "$DIR/src/utils/logger.hpp" << 'EOF'
#pragma once
#include <string>
#include <iostream>
#include <chrono>
namespace utils {
class Logger {
public:
    enum Level { DEBUG, INFO, WARN, ERROR };
    static void log(Level lvl, const std::string& msg) {
        const char* names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
        std::cout << "[" << names[lvl] << "] " << msg << std::endl;
    }
    static void info(const std::string& m) { log(INFO, m); }
    static void warn(const std::string& m) { log(WARN, m); }
    static void error(const std::string& m) { log(ERROR, m); }
};
}
EOF

cat > "$DIR/src/utils/validator.hpp" << 'EOF'
#pragma once
#include <string>
#include <regex>
namespace utils {
inline bool is_valid_email(const std::string& e) {
    std::regex r(R"(\w+@\w+\.\w+)");
    return std::regex_match(e, r);
}
inline bool is_valid_sku(const std::string& s) {
    return s.size() >= 4 && s.size() <= 20;
}
inline bool is_positive(double v) { return v > 0; }
inline bool in_range(int v, int lo, int hi) { return v >= lo && v <= hi; }
}
EOF

cat > "$DIR/src/utils/hash.hpp" << 'EOF'
#pragma once
#include <string>
#include <functional>
namespace utils {
inline std::string simple_hash(const std::string& input) {
    std::hash<std::string> h;
    return std::to_string(h(input));
}
}
EOF

# ─── Services ──────────────────────────────────────────────
cat > "$DIR/src/services/user_service.hpp" << 'EOF'
#pragma once
#include "../models/user.hpp"
#include "../utils/hash.hpp"
#include <vector>
#include <optional>
class UserService {
    std::vector<User> users_;
    int next_id_ = 1;
public:
    int create(const std::string& name, const std::string& email, const std::string& pass) {
        User u{next_id_++, name, email, utils::simple_hash(pass), "user", true, "now"};
        users_.push_back(u);
        return u.id;
    }
    std::optional<User> find_by_id(int id) const {
        for (const auto& u : users_) if (u.id == id) return u;
        return std::nullopt;
    }
    std::optional<User> find_by_email(const std::string& email) const {
        for (const auto& u : users_) if (u.email == email) return u;
        return std::nullopt;
    }
    std::vector<User> list_active() const {
        std::vector<User> r;
        for (const auto& u : users_) if (u.active) r.push_back(u);
        return r;
    }
};
EOF

cat > "$DIR/src/services/product_service.hpp" << 'EOF'
#pragma once
#include "../models/product.hpp"
#include <vector>
#include <optional>
class ProductService {
    std::vector<Product> products_;
    int next_id_ = 1;
public:
    int add(const std::string& name, const std::string& cat, double price, int stock, const std::string& sku) {
        Product p{next_id_++, name, cat, price, stock, sku, stock > 0};
        products_.push_back(p);
        return p.id;
    }
    std::optional<Product> find(int id) const {
        for (const auto& p : products_) if (p.id == id) return p;
        return std::nullopt;
    }
    std::vector<Product> by_category(const std::string& cat) const {
        std::vector<Product> r;
        for (const auto& p : products_) if (p.category == cat) r.push_back(p);
        return r;
    }
    bool update_stock(int id, int delta) {
        for (auto& p : products_) {
            if (p.id == id) { p.stock += delta; p.available = p.stock > 0; return true; }
        }
        return false;
    }
};
EOF

cat > "$DIR/src/services/order_service.hpp" << 'EOF'
#pragma once
#include "../models/order.hpp"
#include "product_service.hpp"
#include <vector>
#include <optional>
class OrderService {
    std::vector<Order> orders_;
    int next_id_ = 1;
    ProductService& products_;
public:
    OrderService(ProductService& ps) : products_(ps) {}
    int create(int user_id, const std::vector<OrderItem>& items) {
        Order o{next_id_++, user_id, items, "pending", "now"};
        for (const auto& item : items) products_.update_stock(item.product_id, -item.quantity);
        orders_.push_back(o);
        return o.id;
    }
    std::optional<Order> find(int id) const {
        for (const auto& o : orders_) if (o.id == id) return o;
        return std::nullopt;
    }
    std::vector<Order> by_user(int user_id) const {
        std::vector<Order> r;
        for (const auto& o : orders_) if (o.user_id == user_id) r.push_back(o);
        return r;
    }
    bool update_status(int id, const std::string& status) {
        for (auto& o : orders_) { if (o.id == id) { o.status = status; return true; } }
        return false;
    }
};
EOF

cat > "$DIR/src/services/review_service.hpp" << 'EOF'
#pragma once
#include "../models/review.hpp"
#include <vector>
class ReviewService {
    std::vector<Review> reviews_;
    int next_id_ = 1;
public:
    int add(int user_id, int product_id, int rating, const std::string& comment) {
        Review r{next_id_++, user_id, product_id, rating, comment, "now"};
        reviews_.push_back(r);
        return r.id;
    }
    std::vector<Review> for_product(int pid) const {
        std::vector<Review> r;
        for (const auto& rv : reviews_) if (rv.product_id == pid) r.push_back(rv);
        return r;
    }
    double avg_rating(int pid) const {
        double sum = 0; int count = 0;
        for (const auto& r : reviews_) if (r.product_id == pid) { sum += r.rating; count++; }
        return count > 0 ? sum / count : 0;
    }
};
EOF

# ─── Middleware ─────────────────────────────────────────────
cat > "$DIR/src/middleware/auth.hpp" << 'EOF'
#pragma once
#include "../models/user.hpp"
#include <string>
namespace middleware {
struct AuthResult {
    bool authenticated;
    int user_id;
    std::string role;
    std::string error;
};
inline AuthResult check_auth(const std::string& token) {
    if (token.empty()) return {false, -1, "", "No token"};
    if (token == "admin-token") return {true, 1, "admin", ""};
    if (token.substr(0, 5) == "user-") return {true, 2, "user", ""};
    return {false, -1, "", "Invalid token"};
}
inline bool require_role(const AuthResult& auth, const std::string& role) {
    return auth.authenticated && auth.role == role;
}
}
EOF

cat > "$DIR/src/middleware/rate_limiter.hpp" << 'EOF'
#pragma once
#include <unordered_map>
#include <chrono>
namespace middleware {
class RateLimiter {
    std::unordered_map<std::string, int> counts_;
    int max_requests_;
public:
    RateLimiter(int max_per_window = 100) : max_requests_(max_per_window) {}
    bool allow(const std::string& client_id) {
        if (counts_[client_id] >= max_requests_) return false;
        counts_[client_id]++;
        return true;
    }
    void reset() { counts_.clear(); }
};
}
EOF

cat > "$DIR/src/middleware/cors.hpp" << 'EOF'
#pragma once
#include <string>
#include <vector>
namespace middleware {
struct CorsConfig {
    std::vector<std::string> allowed_origins = {"*"};
    std::vector<std::string> allowed_methods = {"GET", "POST", "PUT", "DELETE"};
    bool allow_credentials = false;
};
inline bool is_origin_allowed(const CorsConfig& cfg, const std::string& origin) {
    for (const auto& o : cfg.allowed_origins) {
        if (o == "*" || o == origin) return true;
    }
    return false;
}
}
EOF

# ─── Controllers ───────────────────────────────────────────
cat > "$DIR/src/controllers/user_controller.hpp" << 'EOF'
#pragma once
#include "../services/user_service.hpp"
#include "../middleware/auth.hpp"
#include "../utils/validator.hpp"
#include "../utils/logger.hpp"
#include <string>
class UserController {
    UserService& service_;
public:
    UserController(UserService& s) : service_(s) {}
    std::string create_user(const std::string& name, const std::string& email, const std::string& pass) {
        if (!utils::is_valid_email(email)) return "ERROR: Invalid email";
        auto existing = service_.find_by_email(email);
        if (existing) return "ERROR: Email already exists";
        int id = service_.create(name, email, pass);
        utils::Logger::info("Created user: " + name);
        return "OK: User " + std::to_string(id) + " created";
    }
    std::string get_user(int id) {
        auto u = service_.find_by_id(id);
        if (!u) return "ERROR: User not found";
        return "OK: " + u->username + " (" + u->email + ")";
    }
};
EOF

cat > "$DIR/src/controllers/product_controller.hpp" << 'EOF'
#pragma once
#include "../services/product_service.hpp"
#include "../utils/validator.hpp"
#include "../utils/logger.hpp"
class ProductController {
    ProductService& service_;
public:
    ProductController(ProductService& s) : service_(s) {}
    std::string add_product(const std::string& name, const std::string& cat, double price, int stock, const std::string& sku) {
        if (!utils::is_positive(price)) return "ERROR: Price must be positive";
        if (!utils::is_valid_sku(sku)) return "ERROR: Invalid SKU";
        int id = service_.add(name, cat, price, stock, sku);
        utils::Logger::info("Added product: " + name);
        return "OK: Product " + std::to_string(id);
    }
    std::string list_by_category(const std::string& cat) {
        auto products = service_.by_category(cat);
        std::string result = "Products in " + cat + ":\n";
        for (const auto& p : products) result += "  " + p.name + " $" + std::to_string(p.price) + "\n";
        return result;
    }
};
EOF

cat > "$DIR/src/controllers/order_controller.hpp" << 'EOF'
#pragma once
#include "../services/order_service.hpp"
#include "../middleware/auth.hpp"
#include "../utils/logger.hpp"
class OrderController {
    OrderService& service_;
public:
    OrderController(OrderService& s) : service_(s) {}
    std::string place_order(int user_id, const std::vector<OrderItem>& items, const std::string& token) {
        auto auth = middleware::check_auth(token);
        if (!auth.authenticated) return "ERROR: " + auth.error;
        int id = service_.create(user_id, items);
        utils::Logger::info("Order placed: #" + std::to_string(id));
        return "OK: Order #" + std::to_string(id);
    }
    std::string get_order(int id) {
        auto o = service_.find(id);
        if (!o) return "ERROR: Order not found";
        return "OK: Order #" + std::to_string(o->id) + " status=" + o->status + " total=$" + std::to_string(o->total());
    }
};
EOF

# ─── Main (minimal) ───────────────────────────────────────
cat > "$DIR/main.cpp" << 'EOF'
#include <iostream>
#include "src/services/user_service.hpp"
#include "src/services/product_service.hpp"

int main() {
    UserService users;
    ProductService products;

    users.create("alice", "alice@test.com", "pass123");
    products.add("Widget", "tools", 9.99, 100, "WDG-001");

    std::cout << "App initialized." << std::endl;
    return 0;
}
EOF

echo "Created $(find "$DIR" -name "*.hpp" -o -name "*.cpp" | wc -l) files in $DIR"
find "$DIR" -name "*.hpp" -o -name "*.cpp" | sort
