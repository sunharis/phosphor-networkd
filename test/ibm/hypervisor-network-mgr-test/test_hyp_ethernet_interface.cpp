#include "mock_hyp_ethernet_interface.hpp"

#include <sdbusplus/bus.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <gtest/gtest.h>

namespace phosphor
{
namespace network
{

class TestHypEthernetInterface : public testing::Test
{
  public:
    sdbusplus::bus::bus bus;
    HypNetworkMgr manager;
    sdeventplus::Event event = sdeventplus::Event::get_default();

    MockHypEthernetInterface interface;
    TestHypEthernetInterface() :
        bus(sdbusplus::bus::new_default()),
        manager(bus, event, "/xyz/openbmc_test/network/hypervisor"),
        interface(makeInterface(bus, manager))
    {
        manager.setDefaultBIOSTableAttrsOnIntf("if0");
        manager.setDefaultBIOSTableAttrsOnIntf("if1");
        manager.setDefaultHostnameInBIOSTableAttrs();
        interface.createIPAddrObj();
    }

    static MockHypEthernetInterface makeInterface(sdbusplus::bus::bus& bus,
                                                  HypNetworkMgr& manager)
    {
        return {bus, "/xyz/openbmc_test/network/hypervisor/eth0", "eth0",
                manager};
    }

    bool isIPObjExist(const std::string& intf, const std::string& ipaddress)
    {
        auto it = interface.addrs.find(intf);
        if (it != interface.addrs.end())
        {
            if (ipaddress == (it->second)->address())
            {
                return true;
            }
        }
        return false;
    }
};

TEST_F(TestHypEthernetInterface, CheckIPAddress)
{
    EXPECT_EQ(true, isIPObjExist("eth0", "9.9.9.9"));
    EXPECT_EQ(false, isIPObjExist("eth0", "10.10.10.10"));
}

} // namespace network
} // namespace phosphor