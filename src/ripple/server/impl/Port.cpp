//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/server/Port.h>
#include <ripple/beast/rfc2616.h>
#include <ripple/beast/core/LexicalCast.h>

namespace ripple {

bool
Port::secure() const
{
    return protocol.count("peer") > 0 ||
        protocol.count("https") > 0 ||
        protocol.count("wss") > 0 ||
        protocol.count("wss2") > 0;
}

std::string
Port::protocols() const
{
    std::string s;
    for (auto iter = protocol.cbegin();
            iter != protocol.cend(); ++iter)
        s += (iter != protocol.cbegin() ? "," : "") + *iter;
    return s;
}

std::ostream&
operator<< (std::ostream& os, Port const& p)
{
    os << "'" << p.name << "' (ip=" << p.ip << ":" << p.port << ", ";

    if (! p.admin_ip.empty ())
    {
        os << "admin IPs:";
        for (auto const& ip : p.admin_ip)
            os << ip.to_string () << ", ";
    }

    if (! p.secure_gateway_ip.empty ())
    {
        os << "secure_gateway IPs:";
        for (auto const& ip : p.secure_gateway_ip)
            os << ip.to_string () << ", ";
    }

    os << p.protocols () << ")";
    return os;
}

//------------------------------------------------------------------------------

static
void
populate (Section const& section, std::string const& field, std::ostream& log,
    boost::optional<std::vector<beast::IP::Address>>& ips,
    bool allowAllIps, std::vector<beast::IP::Address> const& admin_ip)
{
    auto const result = section.find(field);
    if (result.second)
    {
        std::stringstream ss (result.first);
        std::string ip;
        bool has_any (false);

        ips.emplace();
        while (std::getline (ss, ip, ','))
        {
            auto const addr = beast::IP::Endpoint::from_string_checked (ip);
            if (! addr.second)
            {
                log << "Invalid value '" << ip << "' for key '" << field <<
                    "' in [" << section.name () << "]\n";
                Throw<std::exception> ();
            }

            if (is_unspecified (addr.first))
            {
                if (! allowAllIps)
                {
                    log << "0.0.0.0 not allowed'" <<
                        "' for key '" << field << "' in [" <<
                        section.name () << "]\n";
                    Throw<std::exception> ();
                }
                else
                {
                    has_any = true;
                }
            }

            if (has_any && ! ips->empty ())
            {
                log << "IP specified along with 0.0.0.0 '" << ip <<
                    "' for key '" << field << "' in [" <<
                    section.name () << "]\n";
                Throw<std::exception> ();
            }

            auto const& address = addr.first.address();
            if (std::find_if (admin_ip.begin(), admin_ip.end(),
                [&address] (beast::IP::Address const& ip)
                {
                    return address == ip;
                }
                ) != admin_ip.end())
            {
                log << "IP specified for " << field << " is also for " <<
                    "admin: " << ip << " in [" << section.name() << "]\n";
                Throw<std::exception> ();
            }

            ips->emplace_back (addr.first.address ());
        }
    }
}

void
parse_Port (ParsedPort& port, Section const& section, std::ostream& log)
{
    {
        auto result = section.find("ip");
        if (result.second)
        {
            try
            {
                port.ip = boost::asio::ip::address::from_string(result.first);
            }
            catch (std::exception const&)
            {
                log << "Invalid value '" << result.first <<
                    "' for key 'ip' in [" << section.name() << "]\n";
                Rethrow();
            }
        }
    }

    {
        auto const result = section.find("port");
        if (result.second)
        {
            try
            {
                port.port =
                    beast::lexicalCastThrow<std::uint16_t>(result.first);

                // Port 0 is not supported
                if (*port.port == 0)
                    Throw<std::exception> ();
            }
            catch (std::exception const&)
            {
                log <<
                    "Invalid value '" << result.first << "' for key " <<
                    "'port' in [" << section.name() << "]\n";
                Rethrow();
            }
        }
    }

    {
        auto const result = section.find("protocol");
        if (result.second)
        {
            for (auto const& s : beast::rfc2616::split_commas(
                    result.first.begin(), result.first.end()))
                port.protocol.insert(s);
        }
    }

    {
        auto const lim = get (section, "limit", "unlimited");

        if (!beast::detail::ci_equal (lim, "unlimited"))
        {
            try
            {
                port.limit = static_cast<int> (
                    beast::lexicalCastThrow<std::uint16_t>(lim));
            }
            catch (std::exception const&)
            {
                log <<
                    "Invalid value '" << lim << "' for key " <<
                    "'limit' in [" << section.name() << "]\n";
                Rethrow();
            }
        }
    }

    populate (section, "admin", log, port.admin_ip, true, {});
    populate (section, "secure_gateway", log, port.secure_gateway_ip, false,
        port.admin_ip.get_value_or({}));

    set(port.user, "user", section);
    set(port.password, "password", section);
    set(port.admin_user, "admin_user", section);
    set(port.admin_password, "admin_password", section);
    set(port.ssl_key, "ssl_key", section);
    set(port.ssl_cert, "ssl_cert", section);
    set(port.ssl_chain, "ssl_chain", section);
}

} // ripple
