#include "../main/access_control.h"
#include "fakes/nvs.h"
#include <cassert>
#include <iostream>
int64_t fake_us = 0;
int main() {
    AccessControl access;
    assert(!access.init());
    nvs_set_str("web_access", "salt", "00000000000000000000000000000000");
    // Fixed vector from Python hashlib.pbkdf2_hmac, 100000 rounds, SHA-256.
    nvs_set_str("web_access", "access_hash",
                "1a1b2f40496f30aeb0f731873ba07f010c5bbe3fb1c6fbc3bd7139e1763697d5");
    nvs_set_str("web_access", "admin_hash",
                "47707b88d1fecb9e6875f85f0cc3846ce1e88596d1aae9e686a9bc82c9573afb");
    nvs_set_str("web_access", "certificate", "certificate");
    nvs_set_str("web_access", "private_key", "key");
    assert(access.init());
    assert(access.verify("ordinary-password", false));
    assert(!access.verify("ordinary-password", false));
    fake_us += 1000001;
    assert(!access.verify("ordinary-password", true));
    fake_us += 1000001;
    assert(access.verify("admin-password", true));
    auto s = access.login(true);
    assert(s && s->token.size() == 64 && s->admin_until == 0);
    httpd_req_t req;
    assert(!access.authenticate(&req));
    req.cookie = "other=abc; sprit=" + s->token + "; third=x";
    assert(access.authenticate(&req) == s);
    req.cookie = "sprit=" + s->token + "extra";
    assert(!access.authenticate(&req));
    req.cookie = "sprit=" + s->token;
    access.logout(*s);
    assert(!access.authenticate(&req));
    auto short_session = access.login(false);
    req.cookie = "sprit=" + short_session->token;
    fake_us += 13LL * 3600 * 1000000;
    assert(!access.authenticate(&req));
    auto remembered = access.login(true);
    req.cookie = "sprit=" + remembered->token;
    fake_us += 29LL * 86400 * 1000000;
    assert(access.authenticate(&req) == remembered);
    fake_us += 2LL * 86400 * 1000000;
    assert(!access.authenticate(&req));
    for (int i = 0; i < 4; ++i)
        assert(access.login(false));
    assert(!access.login(false));
    std::cout << "password vectors, throttling, admin separation, cookies, logout and session "
                 "expiry: PASS\n";
}
