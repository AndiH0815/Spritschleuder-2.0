#include "cocktail_server.h"
#include "arduino_comm.h"
#include "config.h"
#include "display_manager.h"
#include "recipes.h"
#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdlib>
#include <cstring>
#include <string>

static const char *TAG = "web";
static const char PAGE[] = R"HTML(<!doctype html><html lang="de"><meta name="viewport" content="width=device-width"><meta charset="utf-8"><link rel="icon" href="data:,"><title>Spritschleuder</title><style>body{font:16px system-ui;max-width:680px;margin:auto;padding:20px;background:#171717;color:#eee}section{background:#292929;padding:18px;margin:14px 0;border-radius:12px}button,input,select{font:inherit;padding:10px;margin:5px;border-radius:7px;border:0}button{background:#f57c00;color:white;cursor:pointer}.ok{color:#ff9800}.stop{background:#f57c00;font-weight:bold}.pumps{display:grid;grid-template-columns:auto 1fr;align-items:center}</style><h1>🍹 Spritschleuder</h1><section id="setup"><h2>WLAN einrichten</h2><input id="ssid" placeholder="WLAN-Name"><input id="pass" type="password" placeholder="Passwort"><button onclick="wifi()">Speichern & Neustarten</button></section><section><h2>Cocktail mixen</h2><select id="drink"></select><select id="ml"><option>40</option><option selected>200</option><option>400</option></select> ml <button onclick="mix()">Mixen</button><button class="stop" onclick="abortMix()">SOFORT STOPP</button><p id="msg"></p></section><section><h2>Neue Mixtur</h2><input id="recipeName" maxlength="24" placeholder="Name"><div class="pumps"><label>Pumpe 1</label><input id="p0" type="number" min="0" max="1000" value="0"><label>Pumpe 2</label><input id="p1" type="number" min="0" max="1000" value="0"><label>Pumpe 3</label><input id="p2" type="number" min="0" max="1000" value="0"><label>Pumpe 4</label><input id="p3" type="number" min="0" max="1000" value="0"></div><small>Anteile in ml; 0 lässt eine Pumpe aus.</small><br><button onclick="addRecipe()">Mixtur speichern</button></section><section><button onclick="resetWifi()">WLAN-Konfiguration löschen</button></section><script>async function post(u,o){let r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)}),t=await r.text();if(!r.ok)throw Error(t);return t}async function loadRecipes(){let rs=await(await fetch('/api/recipes')).json();drink.innerHTML='';rs.forEach((r,i)=>drink.add(new Option(r.name,i)))}async function wifi(){try{await post('/api/wifi',{ssid:ssid.value,password:pass.value});msg.textContent='Gespeichert. Gerät startet neu…'}catch(e){msg.textContent=e}}async function mix(){try{await post('/api/order',{cocktail_id:+drink.value,quantity_ml:+ml.value});msg.className='ok';msg.textContent='Bestellung angenommen'}catch(e){msg.textContent=e}}async function abortMix(){await post('/api/abort',{});msg.className='';msg.textContent='Mischung abgebrochen'}async function addRecipe(){try{await post('/api/recipes',{name:recipeName.value,pump0_ml:+p0.value,pump1_ml:+p1.value,pump2_ml:+p2.value,pump3_ml:+p3.value});recipeName.value='';p0.value=p1.value=p2.value=p3.value=0;await loadRecipes();msg.className='ok';msg.textContent='Mixtur gespeichert'}catch(e){msg.textContent=e}}async function resetWifi(){if(confirm('WLAN wirklich löschen?'))await post('/api/wifi/reset',{})}loadRecipes()</script></html>)HTML";

CocktailServer &CocktailServer::getInstance() { static CocktailServer i; return i; }
static void jsonReply(httpd_req_t *r, const char *body, const char *status = nullptr) { if (status) httpd_resp_set_status(r, status); httpd_resp_set_type(r, "application/json"); httpd_resp_sendstr(r, body); }
static bool readBody(httpd_req_t *r, std::string &body) {
    if (r->content_len <= 0 || r->content_len > 512) return false;
    body.resize(r->content_len); int got = 0;
    while (got < r->content_len) { int n = httpd_req_recv(r, body.data() + got, r->content_len - got); if (n <= 0) return false; got += n; }
    return true;
}
static bool jsonString(const std::string &body, const char *key, std::string &value) {
    std::string token = std::string("\"") + key + "\""; size_t p = body.find(token);
    if (p == std::string::npos || (p = body.find(':', p + token.size())) == std::string::npos || (p = body.find('"', p + 1)) == std::string::npos) return false;
    size_t end = body.find('"', p + 1); if (end == std::string::npos) return false; value = body.substr(p + 1, end - p - 1); return true;
}
static bool jsonInt(const std::string &body, const char *key, int &value) {
    std::string token = std::string("\"") + key + "\""; size_t p = body.find(token);
    if (p == std::string::npos || (p = body.find(':', p + token.size())) == std::string::npos) return false;
    char *end = nullptr; long number = std::strtol(body.c_str() + p + 1, &end, 10); if (end == body.c_str() + p + 1) return false; value = static_cast<int>(number); return true;
}

esp_err_t CocktailServer::start() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG(); cfg.server_port = HTTP_PORT; cfg.max_uri_handlers = 10;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &cfg), TAG, "HTTP server");
    const httpd_uri_t routes[] = {
        {"/", HTTP_GET, pageHandler, nullptr}, {"/api/status", HTTP_GET, statusHandler, nullptr},
        {"/api/order", HTTP_POST, orderHandler, nullptr}, {"/api/wifi", HTTP_POST, wifiHandler, nullptr},
        {"/api/wifi/reset", HTTP_POST, resetWifiHandler, nullptr},
        {"/api/recipes", HTTP_GET, recipesHandler, nullptr}, {"/api/recipes", HTTP_POST, addRecipeHandler, nullptr},
        {"/api/abort", HTTP_POST, abortHandler, nullptr},
    };
    for (const auto &route : routes) ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
    ESP_LOGI(TAG, "Webserver gestartet"); return ESP_OK;
}
void CocktailServer::stop() { if (server) { httpd_stop(server); server = nullptr; } }
esp_err_t CocktailServer::pageHandler(httpd_req_t *r) { httpd_resp_set_type(r, "text/html; charset=utf-8"); return httpd_resp_send(r, PAGE, HTTPD_RESP_USE_STRLEN); }
esp_err_t CocktailServer::statusHandler(httpd_req_t *r) {
    auto &w = WifiManager::getInstance();
    std::string out = std::string("{\"configured\":") + (w.isConfigured() ? "true" : "false") +
        ",\"connected\":" + (w.isConnected() ? "true" : "false") + ",\"ip\":\"" + w.ipAddress() + "\"}";
    jsonReply(r, out.c_str()); return ESP_OK;
}
esp_err_t CocktailServer::orderHandler(httpd_req_t *r) {
    std::string body; int id, ml;
    Recipe recipe;
    if (!readBody(r, body) || !jsonInt(body, "cocktail_id", id) || !jsonInt(body, "quantity_ml", ml) || id < 0 || !RecipeStore::getInstance().get(id, recipe) || ml < 20 || ml > 400) {
        jsonReply(r, "{\"error\":\"Cocktail oder Menge ungueltig\"}", "400 Bad Request"); return ESP_OK;
    }
    CocktailOrder o{recipe, ml};
    getInstance().processOrder(o); jsonReply(r, "{\"status\":\"ok\"}"); return ESP_OK;
}
esp_err_t CocktailServer::wifiHandler(httpd_req_t *r) {
    std::string body, ssid, password;
    if (!readBody(r, body) || !jsonString(body, "ssid", ssid) || !jsonString(body, "password", password) || WifiManager::getInstance().saveCredentials(ssid, password) != ESP_OK) {
        jsonReply(r, "{\"error\":\"WLAN-Daten ungueltig\"}", "400 Bad Request"); return ESP_OK;
    }
    jsonReply(r, "{\"status\":\"restarting\"}"); vTaskDelay(pdMS_TO_TICKS(300)); esp_restart(); return ESP_OK;
}
esp_err_t CocktailServer::resetWifiHandler(httpd_req_t *r) {
    esp_err_t e = WifiManager::getInstance().clearCredentials(); jsonReply(r, e == ESP_OK ? "{\"status\":\"restarting\"}" : "{\"error\":\"NVS\"}");
    if (e == ESP_OK) { vTaskDelay(pdMS_TO_TICKS(300)); esp_restart(); } return ESP_OK;
}
void CocktailServer::processOrder(const CocktailOrder &o) {
    ESP_LOGI(TAG, "Bestellung: %s (%d ml)", o.recipe.name.c_str(), o.quantity_ml);
    ArduinoComm::getInstance().sendOrder(o); DisplayManager::getInstance().showMixing(o.recipe, o.quantity_ml);
}

static std::string jsonEscape(const std::string &value) {
    std::string out; for (char c : value) { if (c == '"' || c == '\\') out += '\\'; if (static_cast<unsigned char>(c) >= 0x20) out += c; } return out;
}
esp_err_t CocktailServer::recipesHandler(httpd_req_t *r) {
    auto recipes = RecipeStore::getInstance().all(); std::string out = "[";
    for (size_t i = 0; i < recipes.size(); ++i) { if (i) out += ','; out += "{\"name\":\"" + jsonEscape(recipes[i].name) + "\"}"; }
    out += ']'; jsonReply(r, out.c_str()); return ESP_OK;
}
esp_err_t CocktailServer::addRecipeHandler(httpd_req_t *r) {
    std::string body, name; int amounts[4] = {};
    if (!readBody(r, body) || !jsonString(body, "name", name)) { jsonReply(r, "{\"error\":\"Name fehlt\"}", "400 Bad Request"); return ESP_OK; }
    Recipe recipe; recipe.name = name;
    for (int pump = 0; pump < 4; ++pump) {
        std::string key = "pump" + std::to_string(pump) + "_ml";
        if (!jsonInt(body, key.c_str(), amounts[pump]) || amounts[pump] < 0 || amounts[pump] > 1000) { jsonReply(r, "{\"error\":\"Pumpenmenge ungueltig\"}", "400 Bad Request"); return ESP_OK; }
        if (amounts[pump]) recipe.ingredients[recipe.num_ingredients++] = {static_cast<uint8_t>(pump), static_cast<uint16_t>(amounts[pump])};
    }
    esp_err_t result = RecipeStore::getInstance().add(recipe);
    if (result != ESP_OK) jsonReply(r, "{\"error\":\"Mixtur ungueltig oder Speicher voll\"}", "400 Bad Request"); else jsonReply(r, "{\"status\":\"ok\"}"); return ESP_OK;
}
esp_err_t CocktailServer::abortHandler(httpd_req_t *r) {
    ArduinoComm::getInstance().abort(); DisplayManager::getInstance().showAborted(); jsonReply(r, "{\"status\":\"aborted\"}"); return ESP_OK;
}
