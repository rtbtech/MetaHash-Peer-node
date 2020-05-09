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

#pragma once

#include <sniper/http/client/Config.h>
#include <sniper/http/server/Config.h>
#include <sniper/net/Domain.h>
#include <sniper/std/chrono.h>
#include <sniper/std/filesystem.h>
#include <sniper/std/string.h>

namespace sniper {

class Config final
{
public:
    explicit Config(const fs::path& config, const fs::path& network);

    [[nodiscard]] const intrusive_ptr<http::server::Config>& http_server_config() const noexcept;
    [[nodiscard]] const http::client::Config& http_client_config() const noexcept;

    [[nodiscard]] const string& ip() const noexcept;
    [[nodiscard]] uint16_t port() const noexcept;
    [[nodiscard]] unsigned http_post_retries() const noexcept;

    [[nodiscard]] unsigned threads_count() const noexcept;
    [[nodiscard]] seconds stats_send_interval() const noexcept;
    [[nodiscard]] bool stats_dump_stdout() const noexcept;
    [[nodiscard]] bool stats_send() const noexcept;
    [[nodiscard]] bool reqs_dump_ok() const noexcept;
    [[nodiscard]] bool reqs_dump_err() const noexcept;

    [[nodiscard]] const net::Domain& network() const noexcept;
    [[nodiscard]] const string& stats() const noexcept;

    [[nodiscard]] int64_t thread_queue_size() const noexcept;

private:
    void load_config(const fs::path& p);
    void load_network(const fs::path& p);

    intrusive_ptr<http::server::Config> _http_server_config;
    http::client::Config _http_client_config;
    string _ip;
    unsigned _port = 8080;
    unsigned _threads_count = 1;
    seconds _stats_send_interval = 1s;
    bool _stats_dump_stdout = false;
    unsigned _http_post_retries = 1;
    bool _stats_send = true;

    net::Domain _network;
    string _stats;

    int64_t _thread_queue_size = 0;

    bool _reqs_dump_ok = false;
    bool _reqs_dump_err = false;
};

} // namespace sniper
