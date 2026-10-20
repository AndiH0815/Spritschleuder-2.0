#include "sdkconfig.h"
#include "cocktail_server.h"
#include "access_control.h"
#include "arduino_comm.h"
#include "cJSON.h"
#include "config.h"
#include "display_manager.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "pump_settings.h"
#include "bar_settings.h"
#include "photo_store.h"
#include <cstdio>
#include <cstdlib>
#include "recipes.h"
#include "wifi_manager.h"
#include <cmath>
#include <cstring>
#include <memory>
extern const uint8_t page_start[] asm("_binary_web_html_gz_start");
extern const uint8_t page_end[] asm("_binary_web_html_gz_end");
namespace {
uint32_t boot_id = 0;
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
cJSON *field(cJSON *r, const char *k) { return cJSON_GetObjectItemCaseSensitive(r, k); }
bool number(cJSON *r, const char *k, double &v, double min, double max, bool integer = false) {
    auto p = field(r, k);
    if (!cJSON_IsNumber(p) || !std::isfinite(p->valuedouble) || p->valuedouble < min ||
        p->valuedouble > max || (integer && std::floor(p->valuedouble) != p->valuedouble))
        return false;
    v = p->valuedouble;
    return true;
}
std::string string(cJSON *r, const char *k) {
    auto p = field(r, k);
    return cJSON_IsString(p) ? p->valuestring : "";
}
esp_err_t reply(httpd_req_t *r, cJSON *o, const char *status = "200 OK") {
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "application/json");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    char *text = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!text)
        return ESP_ERR_NO_MEM;
    auto e = httpd_resp_sendstr(r, text);
    cJSON_free(text);
    return e;
}
esp_err_t error(httpd_req_t *r, const char *text, const char *status = "400 Bad Request") {
    auto o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "error", text);
    return reply(r, o, status);
}
esp_err_t ok(httpd_req_t *r) { return reply(r, cJSON_CreateObject()); }
Json body(httpd_req_t *r) {
    if (r->content_len <= 0 || r->content_len > 8192)
        return Json(nullptr, cJSON_Delete);
    char type[64];
    if (httpd_req_get_hdr_value_str(r, "Content-Type", type, sizeof(type)) != ESP_OK ||
        strncmp(type, "application/json", 16) != 0)
        return Json(nullptr, cJSON_Delete);
    std::string s(r->content_len, '\0');
    size_t got = 0;
    while (got < s.size()) {
        int n = httpd_req_recv(r, &s[got], s.size() - got);
        if (n <= 0)
            return Json(nullptr, cJSON_Delete);
        got += n;
    }
    // Require complete JSON, including trailing data, and one object root.
    s.push_back('\0');
    auto root = cJSON_ParseWithOpts(s.c_str(), nullptr, true);
    if (root && !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        root = nullptr;
    }
    return Json(root, cJSON_Delete);
}
cJSON *wifiJson();
cJSON *status(WebSession *s) {
    auto state = ArduinoComm::getInstance().status();
    auto o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "boot_id", boot_id);
    cJSON_AddStringToObject(o, "state", state.state.c_str());
    cJSON_AddStringToObject(o, "error", state.error.c_str());
    cJSON_AddBoolToObject(o, "online", state.online);
    cJSON_AddBoolToObject(o, "busy", state.busy);
    cJSON_AddNumberToObject(o, "job", state.job);
    cJSON_AddNumberToObject(o, "outputs", state.outputs);
    cJSON_AddNumberToObject(o, "elapsed_ms", state.elapsed_ms);
    cJSON_AddNumberToObject(o, "duration_ms", state.duration_ms);
    cJSON_AddNumberToObject(o, "manual_ml", state.manual_ml);
    cJSON_AddBoolToObject(o, "admin", s && s->admin_until > esp_timer_get_time());
    cJSON_AddNumberToObject(o, "sequence", s ? s->sequence : 0);
#ifdef CONFIG_SPRIT_HARDWARE_CONFIRMED
    cJSON_AddBoolToObject(o, "hardware_enabled", true);
#else
    cJSON_AddBoolToObject(o, "hardware_enabled", false);
#endif
    cJSON_AddBoolToObject(o, "scale_enabled", false);
    cJSON_AddBoolToObject(o, "photos_ready", PhotoStore::getInstance().ready());
    cJSON_AddBoolToObject(o, "wifi_setup", WifiManager::getInstance().status().setup);
    if (s && s->admin_until > esp_timer_get_time()) cJSON_AddItemToObject(o, "wifi", wifiJson());
    return o;
}
cJSON *wifiJson() {
    auto s = WifiManager::getInstance().status();
    auto o = cJSON_CreateObject();
    cJSON_AddBoolToObject(o, "connected", s.connected);
    cJSON_AddBoolToObject(o, "configured", s.configured);
    cJSON_AddBoolToObject(o, "setup", s.setup);
    cJSON_AddBoolToObject(o, "testing", s.testing);
    cJSON_AddStringToObject(o, "ssid", s.ssid.c_str());
    cJSON_AddStringToObject(o, "saved_ssid", s.saved_ssid.c_str());
    cJSON_AddStringToObject(o, "ip", s.ip.c_str());
    cJSON_AddStringToObject(o, "message", s.message.c_str());
    return o;
}
bool newRequest(cJSON *root, WebSession &s) {
    double boot, sequence;
    if (!number(root, "boot_id", boot, 1, UINT32_MAX, true) || uint32_t(boot) != boot_id ||
        !number(root, "sequence", sequence, 1, UINT32_MAX, true) || sequence <= s.sequence)
        return false;
    s.sequence = uint32_t(sequence);
    return true;
}
cJSON *recipeJson(const Recipe &r) {
    auto o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "id", r.id);
    cJSON_AddStringToObject(o, "name", r.name.c_str());
    cJSON_AddStringToObject(o, "manual", r.manual_name.c_str());
    cJSON_AddStringToObject(o, "photo", r.photo_key.c_str());
    cJSON_AddBoolToObject(o, "review", r.needs_review);
    auto parts = cJSON_AddArrayToObject(o, "portions");
    for (size_t j = 0; j < r.dosing.count; ++j) {
        auto p = cJSON_CreateObject();
        cJSON_AddItemToArray(parts, p);
        auto &v = r.dosing.portions[j];
        cJSON_AddNumberToObject(p, "ingredient", v.ingredient);
        cJSON_AddNumberToObject(p, "ml", v.ml);
        cJSON_AddBoolToObject(p, "strength", v.strength);
    }
    sprit::Plan plan;
    cJSON_AddBoolToObject(o, "available",
                          !r.needs_review &&
                              sprit::calculate(r.dosing, PumpSettings::getInstance().all(), 400, 1,
                                               plan) == sprit::PlanError::None);
    return o;
}

cJSON *barJson() {
    auto configuration = BarSettings::getInstance().all();
    auto root = cJSON_CreateObject();
    auto names = cJSON_AddArrayToObject(root, "ingredients");
    for (auto &x : configuration.ingredients) {
        auto item = cJSON_CreateObject(); cJSON_AddItemToArray(names, item);
        cJSON_AddNumberToObject(item, "id", x.id);
        cJSON_AddStringToObject(item, "name", x.name.c_str());
    }
    auto glasses = cJSON_AddArrayToObject(root, "glasses");
    for (auto &g : configuration.glasses) {
        auto item = cJSON_CreateObject(); cJSON_AddItemToArray(glasses, item);
        cJSON_AddStringToObject(item, "name", g.name.c_str());
        cJSON_AddNumberToObject(item, "ml", g.ml);
    }
    return root;
}
bool photoId(const std::string &input, uint32_t &id) {
    const std::string uri = input.substr(0, input.find('?'));
    const std::string prefix = "/api/photo/";
    if (uri.compare(0, prefix.size(), prefix) || uri.size() <= prefix.size()) return false;
    uint64_t value = 0;
    for (size_t i = prefix.size(); i < uri.size(); ++i) {
        char c = uri[i];
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
        if (value > UINT32_MAX) return false;
    }
    id = uint32_t(value);
    return id != 0;
}
esp_err_t photoGet(httpd_req_t *request, uint32_t id) {
    Recipe recipe;
    if (!PhotoStore::getInstance().ready() || !RecipeStore::getInstance().get(id, recipe) ||
        !PhotoStore::validKey(recipe.photo_key))
        return error(request, "Kein Foto vorhanden", "404 Not Found");
    FILE *file = fopen(PhotoStore::path(recipe.photo_key).c_str(), "rb");
    if (!file) return error(request, "Foto nicht lesbar", "404 Not Found");
    httpd_resp_set_type(request, "image/jpeg");
    httpd_resp_set_hdr(request, "Cache-Control", "private, no-store");
    httpd_resp_set_hdr(request, "X-Content-Type-Options", "nosniff");
    char buffer[1024]; size_t count;
    esp_err_t e = ESP_OK;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        e = httpd_resp_send_chunk(request, buffer, count);
        if (e != ESP_OK) break;
    }
    if (ferror(file)) e = ESP_FAIL;
    fclose(file);
    return e == ESP_OK ? httpd_resp_send_chunk(request, nullptr, 0) : e;
}
esp_err_t photoPut(httpd_req_t *request, uint32_t id) {
    Recipe recipe;
    auto &photos = PhotoStore::getInstance();
    if (!photos.ready()) return error(request, "Bildspeicher nicht verfuegbar");
    if (!RecipeStore::getInstance().get(id, recipe)) return error(request, "Rezept fehlt");
    char type[32];
    if (request->content_len < 4 || request->content_len > 49152 ||
        httpd_req_get_hdr_value_str(request, "Content-Type", type, sizeof(type)) != ESP_OK ||
        strcmp(type, "image/jpeg"))
        return error(request, "JPEG bis 48 KiB erforderlich");
    auto key = photos.freshKey();
    if (key.empty()) return error(request, "Bildspeicher belegt");
    FILE *file = fopen(PhotoStore::path(key).c_str(), "wb");
    if (!file) return error(request, "Bildspeicher belegt");
    char buffer[1024]; size_t got = 0;
    unsigned char first[3] = {}, previous = 0, last = 0;
    bool good = true;
    while (got < request->content_len) {
        size_t wanted = request->content_len - got;
        if (wanted > sizeof(buffer)) wanted = sizeof(buffer);
        int n = httpd_req_recv(request, buffer, wanted);
        if (n <= 0) { good = false; break; }
        for (int i = 0; i < n; ++i) {
            if (got + size_t(i) < 3) first[got + i] = (unsigned char)buffer[i];
            previous = last; last = (unsigned char)buffer[i];
        }
        if (fwrite(buffer, 1, n, file) != size_t(n)) { good = false; break; }
        got += n;
    }
    if (fclose(file) != 0) good = false;
    good = good && first[0] == 0xff && first[1] == 0xd8 && first[2] == 0xff &&
           previous == 0xff && last == 0xd9;
    if (!good) { photos.remove(key); return error(request, "Foto konnte nicht gespeichert werden"); }
    auto old = recipe.photo_key; recipe.photo_key = key;
    // Commit the new reference only after the entire image is closed. A failed
    // NVS write retains the previous photo, even during replacement.
    if (RecipeStore::getInstance().put(recipe) != ESP_OK) {
        photos.remove(key); return error(request, "Rezept konnte nicht gespeichert werden");
    }
    photos.remove(old);
    return reply(request, recipeJson(recipe));
}
} // namespace
CocktailServer &CocktailServer::getInstance() {
    static CocktailServer i;
    return i;
}
esp_err_t CocktailServer::start() {
    auto &a = AccessControl::getInstance();
    if (!a.init()) {
        ESP_LOGW("web", "Zugangsdaten/Zertifikat fehlen. USB-Ersteinrichtung "
                        "erforderlich; kein offener Webserver.");
        return ESP_OK;
    }
    boot_id = esp_random();
    if (!boot_id)
        boot_id = 1;
    httpd_ssl_config_t cfg = HTTPD_SSL_CONFIG_DEFAULT();
    cfg.httpd.max_uri_handlers = 4;
    cfg.httpd.uri_match_fn = httpd_uri_match_wildcard;
    cfg.httpd.stack_size = 12288;
    cfg.httpd.max_open_sockets = 3;
    cfg.httpd.lru_purge_enable = true;
    cfg.servercert = reinterpret_cast<const unsigned char *>(a.certificate.c_str());
    cfg.servercert_len = a.certificate.size() + 1;
    cfg.prvtkey_pem = reinterpret_cast<const unsigned char *>(a.key.c_str());
    cfg.prvtkey_len = a.key.size() + 1;
    auto e = httpd_ssl_start(&server, &cfg);
    if (e != ESP_OK)
        return e;
    const httpd_uri_t routes[] = {{"/", HTTP_GET, pageHandler, nullptr},
                                  {"/api/*", HTTP_GET, apiHandler, nullptr},
                                  {"/api/*", HTTP_POST, apiHandler, nullptr}};
    for (auto &route : routes) {
        e = httpd_register_uri_handler(server, &route);
        if (e != ESP_OK) {
            stop();
            return e;
        }
    }
    return ESP_OK;
}
void CocktailServer::stop() {
    if (server) {
        httpd_ssl_stop(server);
        server = nullptr;
    }
}
esp_err_t CocktailServer::pageHandler(httpd_req_t *r) {
    // Public shell contains only the login form and client code, no recipes or
    // secrets.
    httpd_resp_set_type(r, "text/html; charset=utf-8");
    httpd_resp_set_hdr(r, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "X-Frame-Options", "DENY");
    httpd_resp_set_hdr(r, "Content-Security-Policy",
                       "default-src 'self'; script-src 'self' 'unsafe-inline'; "
                       "style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; "
                       "frame-ancestors 'none'; base-uri 'none'");
    return httpd_resp_send(r, reinterpret_cast<const char *>(page_start), page_end - page_start);
}
esp_err_t CocktailServer::apiHandler(httpd_req_t *r) {
    // This server has one request task: sessions and configuration mutations are
    // serialized.
    const std::string uri = r->uri;
    auto &a = AccessControl::getInstance();
    auto &comm = ArduinoComm::getInstance();
    if (uri == "/api/login" && r->method == HTTP_POST) {
        auto root = body(r);
        if (!root)
            return error(r, "JSON ungueltig");
        if (!a.verify(string(root.get(), "password"), false))
            return error(r, "Anmeldung fehlgeschlagen oder bitte kurz warten", "401 Unauthorized");
        bool remember = cJSON_IsTrue(field(root.get(), "remember"));
        auto s = a.login(remember);
        if (!s)
            return error(r, "Zu viele Sitzungen", "429 Too Many Requests");
        std::string cookie = "sprit=" + s->token + "; Path=/; Secure; HttpOnly; SameSite=Strict";
        if (remember)
            cookie += "; Max-Age=2592000";
        httpd_resp_set_hdr(r, "Set-Cookie", cookie.c_str());
        return reply(r, status(s));
    }
    auto session = a.authenticate(r);
    if (!session)
        return error(r, "Bitte anmelden", "401 Unauthorized");
    if (r->method == HTTP_GET) {
        if (uri == "/api/wifi") {
            if (session->admin_until <= esp_timer_get_time())
                return error(r, "Verwaltungsfreigabe erforderlich", "403 Forbidden");
            return reply(r, wifiJson());
        }
        if (uri == "/api/bar") return reply(r, barJson());
        uint32_t photo_id;
        if (photoId(uri, photo_id)) {
            if (comm.busy()) return error(r, "Ausgabe aktiv", "409 Conflict");
            return photoGet(r, photo_id);
        }
        if (uri == "/api/status")
            return reply(r, status(session));
        if (uri == "/api/recipes") {
            auto list = cJSON_CreateArray();
            for (auto &recipe : RecipeStore::getInstance().all())
                cJSON_AddItemToArray(list, recipeJson(recipe));
            return reply(r, list);
        }
        if (uri == "/api/pumps") {
            if (session->admin_until <= esp_timer_get_time())
                return error(r, "Verwaltungsfreigabe erforderlich", "403 Forbidden");
            auto list = cJSON_CreateArray();
            for (auto &pump : PumpSettings::getInstance().all()) {
                auto p = cJSON_CreateObject();
                cJSON_AddItemToArray(list, p);
                cJSON_AddBoolToObject(p, "active", pump.active);
                cJSON_AddNumberToObject(p, "ingredient", pump.ingredient);
                cJSON_AddNumberToObject(p, "ml_per_second", pump.ml_per_second);
            }
            return reply(r, list);
        }
        return error(r, "Unbekannte Funktion", "404 Not Found");
    }
    uint32_t photo_id;
    if (photoId(uri, photo_id)) {
        if (session->admin_until <= esp_timer_get_time())
            return error(r, "Verwaltungsfreigabe erforderlich", "403 Forbidden");
        if (comm.busy()) return error(r, "Ausgabe aktiv", "409 Conflict");
        return photoPut(r, photo_id);
    }
    auto root = body(r);
    if (!root)
        return error(r, "JSON ungueltig");
    if (uri == "/api/abort") {
        comm.abort();
        return reply(r, status(session));
    }
    if (uri == "/api/heartbeat") {
        double job;
        if (!number(root.get(), "job", job, 1, UINT32_MAX, true))
            return error(r, "Auftrag fehlt");
        comm.heartbeat(session->token, uint32_t(job));
        return reply(r, status(session));
    }
    if (uri == "/api/logout") {
        if (comm.busy())
            return error(r, "Zuerst Ausgabe stoppen", "409 Conflict");
        a.logout(*session);
        httpd_resp_set_hdr(r, "Set-Cookie",
                           "sprit=; Path=/; Max-Age=0; Secure; HttpOnly; SameSite=Strict");
        return ok(r);
    }
    if (uri == "/api/order") {
        double id, ml, strength;
        Recipe recipe;
        if (!number(root.get(), "recipe_id", id, 1, UINT32_MAX, true) ||
            !number(root.get(), "quantity_ml", ml, 20, 1000) ||
            !number(root.get(), "strength", strength, 0.5, 1.5) ||
            !RecipeStore::getInstance().get(uint32_t(id), recipe) || recipe.needs_review)
            return error(r, "Rezept zuerst einrichten und pruefen");
        if (comm.busy())
            return error(r, "Ausgabe bereits aktiv", "409 Conflict");
        sprit::Plan plan;
        if (sprit::calculate(recipe.dosing, PumpSettings::getInstance().all(), ml, strength,
                             plan) != sprit::PlanError::None)
            return error(r, "Menge, Zuordnung oder Kalibrierung ungueltig");
        if (!newRequest(root.get(), *session))
            return error(r, "Veralteter oder bereits verwendeter Startbefehl", "409 Conflict");
        std::string reason;
        if (!comm.start(plan, session->token, reason))
            return error(r, reason.c_str(), "409 Conflict");
        DisplayManager::getInstance().showMixing(recipe, int(ml), plan.duration_ms);
        return reply(r, status(session));
    }
    if (uri == "/api/admin") {
        if (comm.busy())
            return error(r, "Ausgabe bereits aktiv", "409 Conflict");
        if (!a.verify(string(root.get(), "password"), true))
            return error(r, "Verwaltungsfreigabe fehlgeschlagen oder bitte kurz warten",
                         "403 Forbidden");
        session->admin_until = esp_timer_get_time() + 15LL * 60 * 1000000;
        return reply(r, status(session));
    }
    if (session->admin_until <= esp_timer_get_time())
        return error(r, "Verwaltungsfreigabe erforderlich", "403 Forbidden");
    if (comm.busy())
        return error(r, "Aenderungen waehrend Ausgabe gesperrt", "409 Conflict");
    if (uri == "/api/wifi" || uri == "/api/wifi/start" || uri == "/api/wifi/finish") {
        auto &wifi = WifiManager::getInstance();
        std::string reason;
        bool accepted;
        if (uri == "/api/wifi/start") {
            accepted = wifi.requestSetup(); reason = "Zuerst Ausgabe stoppen";
        } else if (uri == "/api/wifi/finish") accepted = wifi.finishSetup(reason);
        else {
            if (!cJSON_IsString(field(root.get(), "ssid")) ||
                !cJSON_IsString(field(root.get(), "password")))
                return error(r, "WLAN-Name und Passwort erforderlich");
            accepted = wifi.configure(string(root.get(), "ssid"), string(root.get(), "password"), reason);
        }
        return accepted ? reply(r, wifiJson()) : error(r, reason.c_str(), "409 Conflict");
    }
    if (uri == "/api/bar") {
        auto names = field(root.get(), "ingredients"), glasses = field(root.get(), "glasses");
        if (!cJSON_IsArray(names) || cJSON_GetArraySize(names) > 32 ||
            !cJSON_IsArray(glasses) || cJSON_GetArraySize(glasses) != 2)
            return error(r, "Zutaten und zwei Glasgroessen erforderlich");
        BarConfiguration next;
        cJSON *p = nullptr;
        cJSON_ArrayForEach(p, names) {
            double id;
            if (!number(p, "id", id, 1, 255, true)) return error(r, "Zutatenkennung ungueltig");
            next.ingredients.push_back({uint16_t(id), string(p, "name")});
        }
        for (int i = 0; i < 2; ++i) {
            auto g = cJSON_GetArrayItem(glasses, i); double ml;
            if (!number(g, "ml", ml, 20, 1000)) return error(r, "Glasmenge ungueltig");
            next.glasses[i] = {string(g, "name"), ml};
        }
        // Referenced names cannot disappear accidentally. Renaming retains IDs.
        for (const auto &old : BarSettings::getInstance().all().ingredients) {
            bool exists = false;
            for (const auto &x : next.ingredients) if (x.id == old.id) exists = true;
            if (exists) continue;
            for (const auto &pump : PumpSettings::getInstance().all())
                if (pump.ingredient == old.id) return error(r, "Zutat noch einer Pumpe zugeordnet");
            for (const auto &recipe : RecipeStore::getInstance().all())
                for (size_t j = 0; j < recipe.dosing.count; ++j)
                    if (recipe.dosing.portions[j].ingredient == old.id)
                        return error(r, "Zutat noch in Rezept verwendet");
        }
        return BarSettings::getInstance().replace(next) == ESP_OK
                   ? reply(r, barJson()) : error(r, "Zutaten oder Glasgroessen ungueltig");
    }
    if (uri == "/api/photo/delete") {
        double id; Recipe recipe;
        if (!number(root.get(), "id", id, 1, UINT32_MAX, true) ||
            !RecipeStore::getInstance().get(uint32_t(id), recipe)) return error(r, "Rezept fehlt");
        auto old = recipe.photo_key; recipe.photo_key.clear();
        if (RecipeStore::getInstance().put(recipe) != ESP_OK) return error(r, "Speichern fehlgeschlagen");
        PhotoStore::getInstance().remove(old);
        return reply(r, recipeJson(recipe));
    }
    if (uri == "/api/pumps") {
        auto list = field(root.get(), "pumps");
        if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) != 8)
            return error(r, "Acht Pumpenplaetze erforderlich");
        auto pumps = PumpSettings::getInstance().all();
        for (int i = 0; i < 8; ++i) {
            auto item = cJSON_GetArrayItem(list, i);
            double ingredient, rate;
            if (!cJSON_IsBool(field(item, "active")) ||
                !number(item, "ingredient", ingredient, 0, 255, true) ||
                !number(item, "ml_per_second", rate, 0, 100))
                return error(r, "Pumpendaten ungueltig");
            pumps[i] = {bool(cJSON_IsTrue(field(item, "active"))), uint16_t(ingredient), rate};
        }
        return PumpSettings::getInstance().replace(pumps) == ESP_OK
                   ? ok(r)
                   : error(r, "Speichern fehlgeschlagen, aktive Zutaten muessen "
                              "eindeutig sein");
    }
    if (uri == "/api/test") {
        double pump, ms;
        if (!number(root.get(), "pump", pump, 0, 7, true) ||
            !number(root.get(), "duration_ms", ms, 100, 30000, true))
            return error(r, "Testlauf ungueltig (maximal 30 Sekunden)");
        if (!PumpSettings::getInstance().all()[size_t(pump)].active)
            return error(r, "Pumpe deaktiviert");
        if (!newRequest(root.get(), *session))
            return error(r, "Veralteter oder bereits verwendeter Startbefehl", "409 Conflict");
        sprit::Plan plan;
        plan.durations[size_t(pump)] = uint32_t(ms);
        plan.duration_ms = uint32_t(ms);
        std::string reason;
        if (!comm.start(plan, session->token, reason))
            return error(r, reason.c_str(), "409 Conflict");
        return reply(r, status(session));
    }
    if (uri == "/api/recipe/delete") {
        double id;
        if (!number(root.get(), "id", id, 1, UINT32_MAX, true))
            return error(r, "Rezeptkennung fehlt");
        Recipe recipe;
        if (!RecipeStore::getInstance().get(uint32_t(id), recipe) ||
            RecipeStore::getInstance().remove(uint32_t(id)) != ESP_OK)
            return error(r, "Loeschen fehlgeschlagen");
        PhotoStore::getInstance().remove(recipe.photo_key);
        return ok(r);
    }
    if (uri == "/api/recipes") {
        Recipe recipe;
        double id = 0;
        if (field(root.get(), "id") && !number(root.get(), "id", id, 0, UINT32_MAX, true))
            return error(r, "Rezeptkennung ungueltig");
        recipe.id = uint32_t(id);
        if (recipe.id && !RecipeStore::getInstance().get(recipe.id, recipe))
            return error(r, "Rezept nicht gefunden");
        recipe.dosing.count = 0;
        recipe.needs_review = false;
        recipe.name = string(root.get(), "name");
        recipe.manual_name = string(root.get(), "manual");
        auto parts = field(root.get(), "portions");
        if (!cJSON_IsArray(parts) || cJSON_GetArraySize(parts) < 1 || cJSON_GetArraySize(parts) > 8)
            return error(r, "Eine bis acht Zutaten erforderlich");
        cJSON *p = nullptr;
        cJSON_ArrayForEach(p, parts) {
            double ingredient, ml;
            if (!number(p, "ingredient", ingredient, 1, 255, true) ||
                !number(p, "ml", ml, 0.01, 400) || !cJSON_IsBool(field(p, "strength")))
                return error(r, "Zutaten ungueltig");
            recipe.dosing.portions[recipe.dosing.count++] = {
                uint16_t(ingredient), ml, bool(cJSON_IsTrue(field(p, "strength")))};
        }
        if (RecipeStore::getInstance().put(recipe) != ESP_OK)
            return error(r, "Rezept konnte nicht gespeichert werden");
        return reply(r, recipeJson(recipe));
    }
    // Legacy unauthenticated wifi/reset and order APIs are not registered.
    return error(r, "Unbekannte Funktion", "404 Not Found");
}
