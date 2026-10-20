#include "../main/local_discovery.h"
#include "fakes/mdns.h"
#include <iostream>

int main() {
    assert(!LocalDiscovery::running());
    assert(LocalDiscovery::start() == ESP_OK);
    assert(LocalDiscovery::running());
    assert(fake_mdns_host == "spritschleuder");
    assert(std::string(LocalDiscovery::Url) == "https://spritschleuder.local");
    assert(fake_mdns_services.size() == 1);
    assert(fake_mdns_services[0].service == "_https" &&
           fake_mdns_services[0].protocol == "_tcp" && fake_mdns_services[0].port == 443);
    int calls = fake_mdns_calls;
    assert(LocalDiscovery::start() == ESP_OK && fake_mdns_calls == calls);
    LocalDiscovery::stop();
    LocalDiscovery::stop();
    assert(!LocalDiscovery::running() && fake_mdns_frees == 1);
    for (int failure = 1; failure <= 4; ++failure) {
        fake_mdns_calls = 0; fake_mdns_fail_at = failure;
        auto frees = fake_mdns_frees;
        assert(LocalDiscovery::start() != ESP_OK);
        assert(!LocalDiscovery::running());
        // Init failure owns no library; later failures release partial setup.
        assert(fake_mdns_frees == frees + (failure > 1));
        fake_mdns_fail_at = 0;
        assert(LocalDiscovery::start() == ESP_OK && LocalDiscovery::running());
        LocalDiscovery::stop();
    }
    std::cout << "Local discovery: fixed hostname, HTTPS-only service, no secrets, "
                 "idempotent lifecycle and cleanup/retry after each startup failure: PASS\n";
}
