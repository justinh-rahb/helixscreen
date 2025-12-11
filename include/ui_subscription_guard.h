// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "moonraker_client.h"

#include <utility>

/**
 * @brief RAII wrapper for Moonraker subscriptions - auto-unsubscribes on destruction
 *
 * Similar to ObserverGuard but for MoonrakerClient::register_notify_update() subscriptions.
 * Ensures subscriptions are properly cleaned up when the owning object is destroyed.
 *
 * Usage:
 * @code
 *   class MyBackend {
 *       SubscriptionGuard subscription_;
 *
 *       void start(MoonrakerClient* client) {
 *           subscription_ = SubscriptionGuard(client, client->register_notify_update(...));
 *       }
 *       // Automatically unsubscribes when MyBackend is destroyed
 *   };
 * @endcode
 */
class SubscriptionGuard {
  public:
    SubscriptionGuard() = default;

    /**
     * @brief Construct guard from client and subscription ID
     *
     * @param client Moonraker client that owns the subscription
     * @param id Subscription ID from register_notify_update()
     */
    SubscriptionGuard(MoonrakerClient* client, SubscriptionId id)
        : client_(client), subscription_id_(id) {}

    ~SubscriptionGuard() {
        reset();
    }

    SubscriptionGuard(SubscriptionGuard&& other) noexcept
        : client_(std::exchange(other.client_, nullptr)),
          subscription_id_(std::exchange(other.subscription_id_, INVALID_SUBSCRIPTION_ID)) {}

    SubscriptionGuard& operator=(SubscriptionGuard&& other) noexcept {
        if (this != &other) {
            reset();
            client_ = std::exchange(other.client_, nullptr);
            subscription_id_ = std::exchange(other.subscription_id_, INVALID_SUBSCRIPTION_ID);
        }
        return *this;
    }

    SubscriptionGuard(const SubscriptionGuard&) = delete;
    SubscriptionGuard& operator=(const SubscriptionGuard&) = delete;

    /**
     * @brief Unsubscribe and release the subscription
     */
    void reset() {
        if (client_ && subscription_id_ != INVALID_SUBSCRIPTION_ID) {
            client_->unsubscribe_notify_update(subscription_id_);
            subscription_id_ = INVALID_SUBSCRIPTION_ID;
        }
        client_ = nullptr;
    }

    /**
     * @brief Release ownership without unsubscribing
     *
     * Use during shutdown when the client may already be destroyed.
     * The subscription will not be removed (it may already be gone).
     */
    void release() {
        client_ = nullptr;
        subscription_id_ = INVALID_SUBSCRIPTION_ID;
    }

    /**
     * @brief Check if guard holds a valid subscription
     */
    explicit operator bool() const {
        return client_ != nullptr && subscription_id_ != INVALID_SUBSCRIPTION_ID;
    }

    /**
     * @brief Get the raw subscription ID
     */
    SubscriptionId get() const {
        return subscription_id_;
    }

  private:
    MoonrakerClient* client_ = nullptr;
    SubscriptionId subscription_id_ = INVALID_SUBSCRIPTION_ID;
};
