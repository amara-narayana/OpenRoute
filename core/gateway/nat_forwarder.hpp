#pragma once

#include <string>
#include <vector>

namespace openroute::gateway {

struct NetworkInterfaceInfo {
    std::string alias;
    std::string description;
    std::string ipv4_address;
    bool is_up = false;
    bool is_wireless = false;
};

class NatForwarder {
public:
    NatForwarder();
    ~NatForwarder();

    static bool IsElevated();
    static std::vector<NetworkInterfaceInfo> EnumerateInterfaces();
    static std::string DetectPrimaryInterface();

    bool SetupNat(const std::string& nat_name = "OpenRouteNat",
                  const std::string& subnet_prefix = "10.77.0.0/24");
    bool EnableForwarding(const std::string& external_interface,
                         const std::string& tunnel_interface = "openroute0");
    bool TeardownNat(const std::string& nat_name = "OpenRouteNat");

    [[nodiscard]] bool is_configured() const { return configured_; }
    [[nodiscard]] const std::string& active_nat_name() const { return nat_name_; }

private:
    bool ExecutePowerShellCommand(const std::string& command, std::string* output = nullptr);

    bool configured_ = false;
    std::string nat_name_;
};

} // namespace openroute::gateway
