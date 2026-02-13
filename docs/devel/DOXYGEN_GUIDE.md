# Doxygen Documentation Guide

This guide explains how to write effective API documentation using Doxygen for the HelixScreen project.

## Overview

We use Doxygen to generate comprehensive API documentation from specially formatted comments in C++ source code. Documentation is automatically published to GitHub Pages when version tags are pushed.

## Quick Start

### Basic Documentation Pattern

```cpp
/**
 * @brief Short one-line description of the function
 *
 * Optional detailed description that can span multiple lines.
 * Explain the purpose, behavior, and any important details.
 *
 * @param param_name Description of parameter
 * @param another_param Description of another parameter
 * @return Description of return value
 */
ReturnType function_name(ParamType param_name, AnotherType another_param);
```

### Key Principles

1. **Public APIs MUST be documented** - All public classes, methods, and functions require documentation
2. **Private implementation MAY be documented** - Document complex private methods if they're non-obvious
3. **Brief is mandatory** - Every documented entity needs a `@brief` tag
4. **Document parameters** - Use `@param` for all function parameters
5. **Document returns** - Use `@return` for non-void functions
6. **Be concise but complete** - Explain "why" and "what", not "how" (code shows how)

## Documentation Elements

### Classes

```cpp
/**
 * @brief WebSocket client for Moonraker API communication
 *
 * Implements JSON-RPC 2.0 protocol for Klipper/Moonraker integration.
 * Handles connection lifecycle, automatic reconnection, and message routing.
 *
 * Thread Safety: Callbacks may be invoked from libhv event loop thread.
 * Ensure proper synchronization when accessing shared state.
 *
 * Example usage:
 * ```cpp
 * auto client = std::make_unique<MoonrakerClient>(loop);
 * client->connect("ws://127.0.0.1:7125/websocket",
 *                 on_connected, on_disconnected);
 * ```
 */
class MoonrakerClient : public hv::WebSocketClient {
    // ...
};
```

**Pattern:**
- Brief summary on first line
- Detailed description explaining purpose and key features
- Thread safety notes if relevant
- Usage example if helpful

### Methods/Functions

```cpp
/**
 * @brief Connect to WiFi network
 *
 * Attempts to connect to the specified network. Operation is asynchronous;
 * callback invoked when connection succeeds or fails.
 *
 * @param ssid Network name
 * @param password Network password (empty for open networks)
 * @param on_complete Callback with (success, error_message)
 */
void connect(const std::string& ssid,
             const std::string& password,
             std::function<void(bool success, const std::string& error)> on_complete);
```

**Pattern:**
- Brief description of what the function does
- Detailed behavior (sync/async, side effects, special cases)
- Document ALL parameters with `@param`
- Document return value with `@return` (if non-void)

### Enums

```cpp
/**
 * @brief Connection state for Moonraker WebSocket
 */
enum class ConnectionState {
    DISCONNECTED,  ///< Not connected
    CONNECTING,    ///< Connection in progress
    CONNECTED,     ///< Connected and ready
    RECONNECTING,  ///< Automatic reconnection in progress
    FAILED         ///< Connection failed (max retries exceeded)
};
```

**Pattern:**
- Brief description for enum type
- Inline `///` comments for each enum value

### Structs

```cpp
/**
 * @brief WiFi network information
 */
struct WiFiNetwork {
    std::string ssid;           ///< Network name (SSID)
    int signal_strength;        ///< Signal strength (0-100 percentage)
    bool is_secured;            ///< True if network requires password
    std::string security_type;  ///< Security type ("WPA2", "WPA3", "WEP", "Open")
};
```

**Pattern:**
- Brief description for struct type
- Inline `///` comments for each member

## Special Tags

### Thread Safety

Document thread safety characteristics for concurrent code:

```cpp
/**
 * @brief Register callback for WiFi events
 *
 * Events are delivered asynchronously and may arrive from background threads.
 * Ensure thread safety in callback implementations.
 *
 * @param name Event type identifier
 * @param callback Handler function (invoked from libhv event loop thread)
 */
void register_event_callback(const std::string& name,
                             std::function<void(const std::string&)> callback);
```

### Virtual Methods

Mark methods that can be overridden:

```cpp
/**
 * @brief Perform printer auto-discovery sequence
 *
 * Calls printer.objects.list → server.info → printer.info → printer.objects.subscribe
 * in sequence, parsing discovered objects and populating PrinterState.
 *
 * Virtual to allow mock override for testing without real printer connection.
 *
 * @param on_complete Callback invoked when discovery completes successfully
 */
virtual void discover_printer(std::function<void()> on_complete);
```

### Deprecated Features

Mark deprecated APIs:

```cpp
/**
 * @brief Legacy connection method
 *
 * @deprecated Use connect() with on_complete callback instead
 * @param url WebSocket URL
 */
[[deprecated("Use connect() with callback")]]
int connect_sync(const char* url);
```

## Code Examples

Include usage examples for complex APIs:

```cpp
/**
 * @brief Ethernet Manager - High-level interface for Ethernet status queries
 *
 * Provides simple API for checking Ethernet connectivity and retrieving
 * network information.
 *
 * Usage:
 * ```cpp
 * auto manager = std::make_unique<EthernetManager>();
 *
 * if (manager->has_interface()) {
 *     std::string ip = manager->get_ip_address();
 *     if (!ip.empty()) {
 *         // Display "Connected (192.168.1.100)"
 *     }
 * }
 * ```
 */
class EthernetManager {
    // ...
};
```

## Documentation Checklist

Before committing code, ensure:

- [ ] All public classes have `@brief` descriptions
- [ ] All public methods have `@brief` descriptions
- [ ] All parameters documented with `@param`
- [ ] All non-void returns documented with `@return`
- [ ] Complex behaviors explained in detail section
- [ ] Thread safety noted if relevant
- [ ] Usage examples for non-obvious APIs
- [ ] No typos or broken formatting

## Reference Examples

Study these well-documented files for patterns:

### Excellent Documentation
- `include/moonraker_client.h` - Comprehensive class and method docs
- `include/wifi_manager.h` - Clear API with behavioral notes
- `include/wifi_backend.h` - Abstract interface with design principles
- `include/ethernet_manager.h` - Concise with usage examples

### Key Patterns
1. **Managers** - High-level API with usage examples
2. **Backends** - Abstract interface with design principles
3. **Utilities** - Brief descriptions with parameter/return docs
4. **Data structures** - Inline comments for members

## Generating Documentation Locally

Test your documentation formatting before pushing:

```bash
# From project root
doxygen Doxyfile

# View output
open build/docs/html/index.html  # macOS
xdg-open build/docs/html/index.html  # Linux
```

Check for:
- Warnings/errors in Doxygen output
- Correct formatting in generated HTML
- All documented entities appear in output
- Links between classes work correctly

## CI/CD Pipeline

Documentation is automatically built and deployed:

1. **Local Development:** Write Doxygen comments, test with `doxygen Doxyfile`
2. **Create Release:** Push version tag (e.g., `git tag v0.1.0 && git push origin v0.1.0`)
3. **CI Build:** GitHub Actions runs Doxygen on tag push
4. **Commit Docs:** Generated HTML committed to `docs/api/` on main branch
5. **Deploy:** GitHub Pages automatically serves from `docs/` folder

## Common Mistakes

### ❌ Missing @brief
```cpp
/**
 * Connect to network
 */
void connect(const std::string& ssid);
```

### ✅ Correct
```cpp
/**
 * @brief Connect to WiFi network
 */
void connect(const std::string& ssid);
```

---

### ❌ Undocumented Parameters
```cpp
/**
 * @brief Send JSON-RPC request
 */
int send_jsonrpc(const std::string& method, const json& params);
```

### ✅ Correct
```cpp
/**
 * @brief Send JSON-RPC request
 *
 * @param method RPC method name (e.g., "printer.info")
 * @param params JSON parameters object
 * @return 0 on success, non-zero on error
 */
int send_jsonrpc(const std::string& method, const json& params);
```

---

### ❌ Vague Descriptions
```cpp
/**
 * @brief Process data
 */
void process();
```

### ✅ Correct
```cpp
/**
 * @brief Check for timed out requests and invoke error callbacks
 *
 * Should be called periodically (e.g., every 1-5 seconds) from main loop
 * to detect requests that exceeded their timeout threshold.
 */
void check_request_timeouts();
```

## Further Reading

- **Doxygen Manual:** https://www.doxygen.nl/manual/index.html
- **Special Commands:** https://www.doxygen.nl/manual/commands.html
- **Markdown Support:** https://www.doxygen.nl/manual/markdown.html

---

**Questions?** Review existing well-documented files or ask in PR reviews.
