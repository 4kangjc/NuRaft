/************************************************************************
Modifications Copyright 2017-present eBay Inc.

Original Copyright:
See URL: https://github.com/datatechnology/cornerstone

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    https://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
**************************************************************************/

#pragma once

#include "basic_types.hxx"
#include "event_awaiter.h"
#include "pp_util.hxx"
#include "ptr.hxx"

#include <atomic>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

namespace nuraft {

class raft_server;

/**
 * Configurations for the initialization of `nuraft_global_mgr`.
 */
struct nuraft_global_config {
    nuraft_global_config()
        : num_commit_threads_(1)
        , num_append_threads_(1)
        , max_scheduling_unit_ms_(20)
        {}

    /**
     * The number of globally shared threads executing the
     * commit of state machine.
     */
    size_t num_commit_threads_;

    /**
     * The number of globally shared threads executing replication.
     */
    size_t num_append_threads_;

    /**
     * If a commit of a Raft instance takes longer than this time,
     * worker thread will pause the commit of the current instance
     * and schedule the next instance, to avoid starvation issue.
     */
    size_t max_scheduling_unit_ms_;
};

static nuraft_global_config __DEFAULT_NURAFT_GLOBAL_CONFIG;

// Singleton class.
class nuraft_global_mgr {
public:
    /**
     * Initialize the global instance.
     *
     * @return If succeeds, the initialized instance.
     *         If already initialized, the existing instance.
     */
    static nuraft_global_mgr* init(const nuraft_global_config& config =
                                       __DEFAULT_NURAFT_GLOBAL_CONFIG);

    /**
     * Shutdown the global instance and free all resources.
     * All Raft instances should be shut down before calling this API.
     */
    static void shutdown();

    /**
     * Get the current global instance.
     *
     * @return The current global instance if initialized.
     *         `nullptr` if not initialized.
     */
    static nuraft_global_mgr* get_instance();

    /**
     * This function is called by the constructor of `raft_server`.
     *
     * @param server Raft server instance.
     */
    void init_raft_server(raft_server* server);

    /**
     * This function is called by the destructor of `raft_server`.
     *
     * @param server Raft server instance.
     */
    void close_raft_server(raft_server* server);

    /**
     * Request `append_entries` for the given server.
     *
     * @param server Raft server instance to request `append_entries`.
     */
    void request_append(ptr<raft_server> server);

    /**
     * Request background commit execution for the given server.
     *
     * @param server Raft server instance to execute commit.
     */
    void request_commit(ptr<raft_server> server);

private:
    struct worker_handle {
        worker_handle(size_t id = 0);
        ~worker_handle();
        void shutdown();

        enum status {
            SLEEPING = 0,
            WORKING = 1,
        };

        size_t id_;
        EventAwaiter ea_;
        ptr<std::thread> thread_;
        bool stopping_;
        std::atomic<status> status_;
    };

    static std::mutex instance_lock_;
    static std::atomic<nuraft_global_mgr*> instance_;

    nuraft_global_mgr();

    ~nuraft_global_mgr();

    __nocopy__(nuraft_global_mgr);

    /**
     * Initialize thread pool with the given config.
     */
    void init_thread_pool();

    /**
     * Loop for commit worker threads.
     */
    void commit_worker_loop(ptr<worker_handle> handle);

    /**
     * Loop for append worker threads.
     */
    void append_worker_loop(ptr<worker_handle> handle);

    /**
     * Global config.
     */
    nuraft_global_config config_;

    /**
     * Counter for assigning thread ID.
     */
    std::atomic<size_t> thread_id_counter_;

    /**
     * Commit thread pool.
     */
    std::vector< ptr<worker_handle> > commit_workers_;

    /**
     * Commit thread pool.
     */
    std::vector< ptr<worker_handle> > append_workers_;

    /**
     * Commit requests.
     * Duplicate requests from the same `raft_server` will not be allowed.
     */
    std::list< ptr<raft_server> > commit_queue_;

    /**
     * A set for efficient duplicate checking of `raft_server`.
     * It will contain all `raft_server`s currently in `commit_queue_`.
     */
    std::unordered_set< ptr<raft_server> > commit_server_set_;

    /**
     * Lock for `commit_queue_` and `commit_server_set_`.
     */
    std::mutex commit_queue_lock_;

    /**
     * Append (replication) requests.
     * Duplicate requests from the same `raft_server` will not be allowed.
     */
    std::list< ptr<raft_server> > append_queue_;

    /**
     * A set for efficient duplicate checking of `raft_server`.
     * It will contain all `raft_server`s currently in `append_queue_`.
     */
    std::unordered_set< ptr<raft_server> > append_server_set_;

    /**
     * Lock for `append_queue_` and `append_server_set_`.
     */
    std::mutex append_queue_lock_;
};

} // namespace nuraft;
