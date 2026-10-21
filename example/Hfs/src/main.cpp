#include "filesys.h"
#include "res.h"

#include "alogger.h"
#include "atransmit.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace {
    struct page_asset {
        const char* key;
        alx::uint_16 mime;
    };

    constexpr page_asset assets[] = {
        {"index.html", alx::http::mime::html},
        {"app.css", alx::http::mime::css},
        {"i18n.js", alx::http::mime::js},
        {"api.js", alx::http::mime::js},
        {"ui.js", alx::http::mime::js},
        {"app.js", alx::http::mime::js},
    };

    alx::http::api::api_func page_handler(const char* _key, alx::uint_16 _mime) {
        return [_key, _mime](const alx::http::request& _req) {
            if (alx::http::action::GET != _req.get_action())
                return alx::http::reply(_req.get_version(), alx::http::state::FORBIDDEN);
            alx::bytes data;
            if (!res::load(_key, data))
                return alx::http::reply(_req.get_version(), alx::http::state::NOT_FOUND);
            alx::http::reply reply(_req.get_version(), alx::http::state::OK);
            reply.set_content(_mime, alx::bytes_view(data));
            return reply;
        };
    }

    alx::http::api* new_api() {
        alx::http::api* root = new alx::http::api(page_handler("index.html", alx::http::mime::html));
        for (const page_asset& asset : assets)
            root->insert(new alx::http::api(page_handler(asset.key, asset.mime)), asset.key);
        root->insert(filesys::new_api(), "filesys");
        return root;
    }

    void usage() {
        std::cout << "Hfs [ip|ip:port] [root_dir]\n"
                  << "  default: 127.0.0.1:8080, virtual root\n"
                  << "  root_dir: existing directory served as \"/\"\n"
                  << "  exit     stop the server\n";
    }
}

int main(int _argc, char** _argv) {
    if (_argc > 3) {
        usage();
        return 1;
    }

    std::string addr_port{"127.0.0.1:8080"};
    std::string root_dir;
    if (_argc > 1) {
        const std::string arg(_argv[1]);
        if ("-h" == arg || "--help" == arg) {
            usage();
            return 0;
        }
        addr_port = std::string::npos == arg.find(':') ? arg + ":8080" : arg;
    }
    if (_argc > 2) root_dir = _argv[2];

    std::string error;
    if (!filesys::set_root(root_dir, error)) {
        alx::log_error().type("hfs") << error;
        return 1;
    }

    res::init(_argv[0]);

    alx::http::server srv(addr_port, new_api(), 4, 32);
    srv.mesg_prit.connect([](const std::string& _msg) {
        alx::log_debug().type("http") << _msg;
    });
    srv.comm_flag.connect([](const alx::uint_64 _loc, bool _flag) {
        alx::log_warn().type("comm") << alx::transmit::to_strip(_loc) << (_flag ? " connect" : " disconnect");
    });
    if (!srv.open()) {
        alx::log_error().type("hfs") << "listen " << addr_port << " failed";
        return 1;
    }
    alx::log_info().type("hfs") << "http://" << addr_port;
    if (root_dir.empty()) alx::log_info().type("hfs") << "root: virtual";
    else alx::log_info().type("hfs") << "root: " << root_dir;

    std::string line;
    while (std::getline(std::cin, line))
        if ("exit" == line) break;
    if (std::cin.eof()) {
        alx::log_info().type("hfs") << "stdin closed, press Ctrl+C to stop";
        while (true) std::this_thread::sleep_for(std::chrono::hours(24));
    }

    srv.close();
    return 0;
}
