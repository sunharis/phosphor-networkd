#include <arpa/inet.h>
#include <dirent.h>
#include <net/if.h>
#include "xyz/openbmc_project/Common/error.hpp"

#include <phosphor-logging/log.hpp>
#include <phosphor-logging/elog-errors.hpp>

#include <iostream>
#include <list>
#include <string>
#include <algorithm>

namespace phosphor
{
namespace network
{
namespace
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

uint8_t toV6Cidr(const std::string& subnetMask)
{
    uint8_t pos = 0;
    uint8_t prevPos = 0;
    uint8_t cidr = 0;
    uint16_t buff {};
    do
    {
        //subnet mask look like ffff:ffff::
        // or ffff:c000::
        pos =  subnetMask.find(":", prevPos);
        if (pos == std::string::npos)
        {
            break;
        }

        auto str = subnetMask.substr(prevPos, (pos - prevPos));
        prevPos = pos + 1;

        // String length is 0
        if (!str.length())
        {
            return cidr;
        }
        //converts it into number.
        if (sscanf(str.c_str(), "%hx", &buff) <= 0)
        {
            log<level::ERR>("Invalid Mask",
                             entry("SUBNETMASK=%s", subnetMask));

            return 0;
        }

        // convert the number into bitset
        // and check for how many ones are there.
        // if we don't have all the ones then make
        // sure that all the ones should be left justify.

        if (__builtin_popcount(buff) != 16)
        {
            if (((sizeof(buff) * 8) - (__builtin_ctz(buff))) != __builtin_popcount(buff))
            {
                log<level::ERR>("Invalid Mask",
                                entry("SUBNETMASK=%s", subnetMask));

                return 0;
            }
            cidr += __builtin_popcount(buff);
            return cidr;
        }

        cidr += 16;
    }
    while (1);

    return cidr;
}
}// anonymous namespace

uint8_t toCidr(int addressFamily, const std::string& subnetMask)
{
    if (addressFamily == AF_INET6)
    {
        return toV6Cidr(subnetMask);
    }

    uint32_t buff;

    auto rc = inet_pton(addressFamily, subnetMask.c_str(), &buff);
    if (rc <= 0)
    {
        log<level::ERR>("inet_pton failed:",
                        entry("SUBNETMASK=%s", subnetMask));
        return 0;
    }

    buff = be32toh(buff);
    // total no of bits - total no of leading zero == total no of ones
    if (((sizeof(buff) * 8) - (__builtin_ctz(buff))) == __builtin_popcount(buff))
    {
        return __builtin_popcount(buff);
    }
    else
    {
        log<level::ERR>("Invalid Mask",
                        entry("SUBNETMASK=%s", subnetMask));
        return 0;
    }
}

std::string toMask(int addressFamily, uint8_t prefix)
{
    if (addressFamily == AF_INET6)
    {
        //TODO:- conversion for v6
        return "";
    }

    if (prefix < 1 || prefix > 30)
    {
        log<level::ERR>("Invalid Prefix",
                        entry("PREFIX=%d", prefix));
        return "";
    }
    /* Create the netmask from the number of bits */
    unsigned long mask = 0;
    for (auto i = 0 ; i < prefix ; i++)
    {
        mask |= 1 << (31 - i);
    }
    struct in_addr netmask;
    netmask.s_addr = htonl(mask);
    return inet_ntoa(netmask);
}

std::string getNetworkID(int addressFamily, const std::string& ipaddress,
                       uint8_t prefix)
{
    unsigned char* pntMask = nullptr;
    unsigned char* pntNetwork = nullptr;
    int bit {};
    int offset {};
    struct in6_addr netmask {};
    const u_char maskbit[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0,
                              0xf8, 0xfc, 0xfe, 0xff
                             };

    pntMask = reinterpret_cast<unsigned char*>(&netmask);

    offset = prefix / 8;
    bit = prefix % 8;

    while (offset--)
    {
        *pntMask++ = 0xff;
    }

    if (bit)
    {
        *pntMask = maskbit[bit];
    }

    // convert ipaddres string into network address
    struct in6_addr ipaddressNetwork;
    if (inet_pton(addressFamily, ipaddress.c_str(), &ipaddressNetwork) <= 0)
    {
        log<level::ERR>("inet_pton failure",
            entry("IPADDRESS=%s",ipaddress.c_str()));
        report<InternalFailure>();

        return "";
    }

    // Now bit wise and gets you the network address
    pntMask = reinterpret_cast<unsigned char*>(&netmask);
    pntNetwork = reinterpret_cast<unsigned char*>(&ipaddressNetwork);

    for (int i = 0; i < 16 ; i++)
    {
        pntNetwork[i] = pntNetwork[i] & pntMask[i];
    }

    //convert the network address into string fomat.
    char networkString[INET6_ADDRSTRLEN] = { 0 };
    if (inet_ntop(addressFamily, &ipaddressNetwork, networkString,
                  INET6_ADDRSTRLEN) == NULL)
    {
        log<level::ERR>("inet_ntop failure");
        report<InternalFailure>();
    }
    return networkString;
}

bool isLinkLocal(const std::string& address)
{
    std::string linklocal = "fe80";
    return std::mismatch(linklocal.begin(), linklocal.end(),
                         address.begin()).first == linklocal.end() ?
                            true : false;
}

}//namespace network
}//namespace phosphor
