#include "../main/arduino_comm.h"
#include "../main/wifi_manager.h"
#include "../shared/setup_button.h"
#include "fakes/nvs.h"
#include <iostream>
uint32_t fake_ms=0;
std::vector<uint8_t> fake_tx;
struct ControllerTest {
    static void busy(bool b){auto &c=ArduinoComm::getInstance();c.current.busy=b;c.stopping=false;}
};
struct WifiTest {
    static void at(WifiManager&w,uint32_t now){fake_ms=now;w.tick(now);}
    static void connect(WifiManager&w){fake_associated=true;w.got_ip=true;}
    static void reset(){fake_nvs.clear();fail_commit=false;fake_mode=WIFI_MODE_STA;fake_started=false;fake_associated=false;fake_wifi_error=0;fake_ms=0;ArduinoComm::getInstance().setNetworkSetup(false);}
    static void oldProfile(){nvs_set_str("wifi","ssid","Old home");nvs_set_str("wifi","password","old-password");nvs_set_str("recipes","keep","untouched");}
    static void run(){
        std::string error;
        assert(!WifiManager::validCredentials("", "12345678"));
        assert(!WifiManager::validCredentials("Home", "short"));
        assert(!WifiManager::validCredentials(std::string(33,'s'),"12345678"));
        assert(!WifiManager::validCredentials("Home",std::string(64,'z')));
        assert(!WifiManager::validCredentials("Home",std::string("long\0password",13)));
        assert(WifiManager::validCredentials(std::string(32,'s'),std::string(64,'a')));
        reset();oldProfile();
        WifiManager w;w.init();assert(w.status().configured);
        connect(w);at(w,1);at(w,120000);assert(!w.status().setup); // no Internet check
        fake_associated=false;w.got_ip=false;at(w,120001);at(w,180000);assert(!w.status().setup);
        at(w,180001);assert(w.status().setup&&fake_mode==WIFI_MODE_APSTA);
        assert(std::string(reinterpret_cast<char*>(fake_ap.ap.ssid),fake_ap.ap.ssid_len)=="spritschleuder");
        assert(std::string(reinterpret_cast<char*>(fake_ap.ap.password))=="12345678");
        assert(fake_ap.ap.authmode==WIFI_AUTH_WPA2_PSK);
        assert(!w.finishSetup(error));
        auto original=fake_nvs;
        assert(w.configure("Wrong home","bad-password",error));at(w,180250);
        assert(w.status().testing);assert(!w.configure("Other","other-password",error));
        at(w,240250);assert(!w.status().testing&&w.status().setup);
        assert(fake_nvs==original&&w.status().saved_ssid=="Old home");
        assert(std::string(reinterpret_cast<char*>(fake_sta.sta.ssid))=="Old home");
        assert(w.configure("New home","new-password",error));at(w,240500);connect(w);at(w,240750);
        at(w,242750);assert(w.status().saved_ssid=="New home"&&w.status().setup);
        assert(fake_nvs["recipes"]==original["recipes"]);
        at(w,257749);assert(w.status().setup);at(w,257750);assert(!w.status().setup);
        WifiManager restarted;restarted.init();assert(restarted.status().saved_ssid=="New home");
        assert(restarted.requestSetup());at(restarted,258000);assert(restarted.status().setup);
        assert(restarted.configure("Third home","third-password",error));at(restarted,258250);
        auto beforeFail=fake_nvs;connect(restarted);at(restarted,258500);fail_commit=true;at(restarted,260500);fail_commit=false;
        assert(fake_nvs==beforeFail&&restarted.status().saved_ssid=="New home"&&restarted.status().setup);
        connect(restarted);at(restarted,260750);assert(restarted.finishSetup(error));at(restarted,263750);assert(!restarted.status().setup);
        // No profile at all still gives a protected recovery AP after 60 s.
        reset();WifiManager blank;blank.init();at(blank,59999);assert(!blank.status().setup);at(blank,60000);assert(blank.status().setup);
        assert(blank.configure("Open venue","",error));at(blank,60250);connect(blank);at(blank,60500);at(blank,62500);assert(blank.status().configured);
        // A restored old network automatically closes only an automatic AP.
        reset();oldProfile();WifiManager autoRecovery;autoRecovery.init();at(autoRecovery,60000);connect(autoRecovery);at(autoRecovery,60250);at(autoRecovery,75250);assert(!autoRecovery.status().setup);
        // A physical request remains available until explicitly finished.
        assert(autoRecovery.requestSetup());at(autoRecovery,75500);connect(autoRecovery);at(autoRecovery,75750);at(autoRecovery,150000);assert(autoRecovery.status().setup);
        reset();WifiManager waitStop;waitStop.init();ControllerTest::busy(true);
        int switches=fake_switches;
        assert(!waitStop.requestSetup());assert(waitStop.requestSetup(true));at(waitStop,1);
        assert(fake_switches==switches&&fake_mode==WIFI_MODE_STA);
        at(waitStop,5000);assert(fake_switches==switches);
        ControllerTest::busy(false);at(waitStop,5250);assert(fake_mode==WIFI_MODE_APSTA);
        sprit::SetupButton b;
        using A=sprit::ButtonAction;
        assert(b.update(true,0)==A::None);assert(b.update(false,10)==A::None);assert(b.update(false,50)==A::None);
        assert(b.update(true,100)==A::None);assert(b.update(true,130)==A::None);assert(b.update(false,300)==A::None);assert(b.update(false,330)==A::Info);
        assert(b.update(true,400)==A::None);assert(b.update(true,430)==A::None);assert(b.update(true,5429)==A::None);assert(b.update(true,5430)==A::Setup);assert(b.update(true,9000)==A::None);assert(b.update(false,9100)==A::None);assert(b.update(false,9130)==A::None);
        sprit::SetupButton rollover;rollover.update(true,UINT32_MAX-100);rollover.update(true,UINT32_MAX-70);assert(rollover.update(true,4930)==A::Setup);
        std::cout<<"WiFi recovery: timeout, protected SSID, offline Internet, test/rollback, single-value persistence, storage failure, reboot, open venue, manual mode and debounced 5 s button: PASS\n";
    }
};
int main(){WifiTest::run();}
