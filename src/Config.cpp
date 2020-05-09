// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

/*
 * Copyright (c) 2018 - 2019, MetaHash, Oleg Romanenko (oleg@romanenko.ro)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <libconfig.h++>
#include <openssl/opensslv.h>
#include <sniper/file/load.h>
#include <sniper/log/log.h>
#include <sniper/net/Peer.h>
#include <sniper/net/ip.h>
#include <sniper/std/boost_vector.h>
#include <sniper/std/check.h>
#include <sniper/std/filesystem.h>
#include <sniper/std/vector.h>
#include <sniper/strings/atoi.h>
#include <sniper/strings/split.h>
#include <thread>
#include "Config.h"
#include "version.h"

namespace sniper {

namespace {

//net-main
//206.189.11.155 9999 256
//206.189.11.153 9999 256
tuple<string, vector<net::Peer>> load_network_nodes(const fs::path& p)
{
    string network;
    vector<net::Peer> nodes;

    static_vector<string_view, 3> cont;
    file::load_file_by_line(p, [&](string_view line) {
        if (network.empty()) {
            network = line;
            log_info("network: {}", network);
            return;
        }

        if (strings::split(line, " ", cont, cont.max_size()) && cont.size() >= 2) {
            uint32_t ip;
            if (auto port = strings::fast_atoi64(cont[1]); port && net::ip_from_sv(cont[0], ip)) {
                log_info("\t{}:{}", cont[0], *port);
                nodes.emplace_back(ip, *port);
            }
        }
    });

    return make_tuple(std::move(network), std::move(nodes));
}

} // namespace

Config::Config(const fs::path& config_p, const fs::path& network_p) :
    _http_server_config(http::server::make_config()), _threads_count(std::thread::hardware_concurrency())
{
    load_config(config_p);
    load_network(network_p);

    check(!_stats.empty(), "[Config] Stats url not set");
    check(!_network.name().empty(), "[Config] Network name empty");
    check(!_network.nodes.empty(), "[Config] Network nodes list empty");
}

void Config::load_network(const fs::path& p)
{
    log_info("Load network file: {}", p.string());

    auto [network, nodes] = load_network_nodes(p);
    if (!network.empty())
        _network.set(network);

    for (auto& node : nodes)
        _network.nodes.emplace_back(std::move(node));
}

void Config::load_config(const fs::path& p)
{
    {
        error_code ec;
        check(fs::exists(p, ec) && !ec, "Config file {} does not exists", p.string());
        check(fs::file_size(p, ec) && !ec, "Empty config file");
    }

    log_info("Load config file: {}", p.string());

    libconfig::Config cfg;
    cfg.readFile(p.c_str());
    int64_t queue_size = 0;

    if (cfg.getRoot().exists("core")) {
        auto& core = cfg.getRoot()["core"];

        core.lookupValue("threads", _threads_count);
        if (!_threads_count)
            _threads_count = std::thread::hardware_concurrency();

        core.lookupValue("reqs_dump_ok", _reqs_dump_ok);
        core.lookupValue("reqs_dump_err", _reqs_dump_err);

        if (unsigned int num; core.lookupValue("queue_size", num)) {
            queue_size = num;
            _thread_queue_size = queue_size / _threads_count;
        }
    }

    if (cfg.getRoot().exists("stats")) {
        auto& stats = cfg.getRoot()["stats"];

        if (unsigned num; stats.lookupValue("interval_seconds", num))
            _stats_send_interval = seconds(num);

        stats.lookupValue("url", _stats);
        stats.lookupValue("dump_stdout", _stats_dump_stdout);
        stats.lookupValue("send", _stats_send);
    }

    if (cfg.getRoot().exists("http")) {
        auto& http = cfg.getRoot()["http"];

        if (http.exists("server")) {
            auto& server = http["server"];

            server.lookupValue("ip", _ip);
            server.lookupValue("port", _port);

            if (unsigned int num; server.lookupValue("max_conns", num))
                _http_server_config->max_conns = num;

            server.lookupValue("backlog", _http_server_config->backlog);

            if (unsigned num; server.lookupValue("keep_alive_timeout_seconds", num))
                _http_server_config->keep_alive_timeout = seconds(num);

            if (unsigned num; server.lookupValue("message_max_size", num)) {
                _http_server_config->buffer_size = num;
                _http_server_config->request_max_size = num;
            }
        }

        if (http.exists("client")) {
            auto& client = http["client"];

            if (unsigned int num; client.lookupValue("conns_per_ip", num))
                _http_client_config.pool.conns_per_ip = num;

            if (unsigned int num; client.lookupValue("pool_max_conns", num))
                _http_client_config.pool.max_conns = num;

            if (unsigned num; client.lookupValue("reponse_timeout_ms", num))
                _http_client_config.pool.connection.response_timeout = milliseconds(num);

            client.lookupValue("retries", _http_post_retries);

            if (unsigned num; client.lookupValue("message_max_size", num))
                _http_client_config.pool.connection.message.body_max_size = num;
        }
    }

    // Print setings
    log_info("Peer: {} ({}, {} UTC), rev {}", VERSION, __DATE__, __TIME__, GIT_SHA1);
    log_info("OpenSSL: {}", OPENSSL_VERSION_TEXT);
    log_info("Config dump:");
    log_info("core");
    log_info("\tthreads: {}", _threads_count);
    log_info("\treqs_dump_ok: {}", _reqs_dump_ok);
    log_info("\treqs_dump_err: {}", _reqs_dump_err);
    log_info("\tqueue_size: {}", queue_size);

    log_info("stats");
    log_info("\tinterval_seconds: {}", _stats_send_interval.count());
    log_info("\turl: {}", _stats);
    log_info("\tdump_stdout: {}", _stats_dump_stdout);
    log_info("\tsend: {}", _stats_send);

    log_info("http server");
    log_info("\tip: {}", _ip.empty() ? "0.0.0.0" : _ip);
    log_info("\tport: {}", _port);
    log_info("\tmax_conns: {}", _http_server_config->max_conns);
    log_info("\tbacklog: {}", _http_server_config->backlog);
    log_info("\tkeep_alive_timeout_seconds: {}",
             duration_cast<seconds>(_http_server_config->keep_alive_timeout).count());
    log_info("\tmessage_max_size: {}", _http_server_config->request_max_size);

    log_info("http client");
    log_info("\tconns_per_ip: {}", _http_client_config.pool.conns_per_ip);
    log_info("\tpool_max_conns: {}", _http_client_config.pool.max_conns);
    log_info("\treponse_timeout_ms: {}", _http_client_config.pool.connection.response_timeout.count());
    log_info("\tretries: {}", _http_post_retries);
    log_info("\tmessage_max_size: {}", _http_client_config.pool.connection.message.body_max_size);
}

const intrusive_ptr<http::server::Config>& Config::http_server_config() const noexcept
{
    return _http_server_config;
}

const http::client::Config& Config::http_client_config() const noexcept
{
    return _http_client_config;
}

uint16_t Config::port() const noexcept
{
    return _port;
}

unsigned Config::threads_count() const noexcept
{
    return _threads_count;
}

seconds Config::stats_send_interval() const noexcept
{
    return _stats_send_interval;
}

const net::Domain& Config::network() const noexcept
{
    return _network;
}

const string& Config::stats() const noexcept
{
    return _stats;
}

bool Config::stats_dump_stdout() const noexcept
{
    return _stats_dump_stdout;
}

unsigned Config::http_post_retries() const noexcept
{
    return _http_post_retries;
}

bool Config::stats_send() const noexcept
{
    return _stats_send;
}

int64_t Config::thread_queue_size() const noexcept
{
    return _thread_queue_size;
}

const string& Config::ip() const noexcept
{
    return _ip;
}

bool Config::reqs_dump_ok() const noexcept
{
    return _reqs_dump_ok;
}

bool Config::reqs_dump_err() const noexcept
{
    return _reqs_dump_err;
}

} // namespace sniper
