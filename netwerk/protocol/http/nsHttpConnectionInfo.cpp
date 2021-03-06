/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

// Log on level :5, instead of default :4.
#undef LOG
#define LOG(args) LOG5(args)
#undef LOG_ENABLED
#define LOG_ENABLED() LOG5_ENABLED()

#include "nsHttpConnectionInfo.h"
#include "mozilla/net/DNS.h"
#include "prnetdb.h"

namespace mozilla {
namespace net {

nsHttpConnectionInfo::nsHttpConnectionInfo(const nsACString &physicalHost,
                                           int32_t physicalPort,
                                           const nsACString &npnToken,
                                           const nsACString &username,
                                           nsProxyInfo *proxyInfo,
                                           bool endToEndSSL)
    : mAuthenticationPort(443)
{
    Init(physicalHost, physicalPort, npnToken, username, proxyInfo, endToEndSSL);
}

nsHttpConnectionInfo::nsHttpConnectionInfo(const nsACString &physicalHost,
                                           int32_t physicalPort,
                                           const nsACString &npnToken,
                                           const nsACString &username,
                                           nsProxyInfo *proxyInfo,
                                           const nsACString &logicalHost,
                                           int32_t logicalPort)

{
    mEndToEndSSL = true; // so DefaultPort() works
    mAuthenticationPort = logicalPort == -1 ? DefaultPort() : logicalPort;

    if (!physicalHost.Equals(logicalHost) || (physicalPort != logicalPort)) {
        mAuthenticationHost = logicalHost;
    }
    Init(physicalHost, physicalPort, npnToken, username, proxyInfo, true);
}

void
nsHttpConnectionInfo::Init(const nsACString &host, int32_t port,
                           const nsACString &npnToken,
                           const nsACString &username,
                           nsProxyInfo* proxyInfo,
                           bool e2eSSL)
{
    LOG(("Init nsHttpConnectionInfo @%p\n", this));

    mUsername = username;
    mProxyInfo = proxyInfo;
    mEndToEndSSL = e2eSSL;
    mUsingConnect = false;
    mNPNToken = npnToken;

    mUsingHttpsProxy = (proxyInfo && proxyInfo->IsHTTPS());
    mUsingHttpProxy = mUsingHttpsProxy || (proxyInfo && proxyInfo->IsHTTP());

    if (mUsingHttpProxy) {
        mUsingConnect = mEndToEndSSL;  // SSL always uses CONNECT
        uint32_t resolveFlags = 0;
        if (NS_SUCCEEDED(mProxyInfo->GetResolveFlags(&resolveFlags)) &&
            resolveFlags & nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL) {
            mUsingConnect = true;
        }
    }

    SetOriginServer(host, port);
}

void
nsHttpConnectionInfo::SetOriginServer(const nsACString &host, int32_t port)
{
    mHost = host;
    mPort = port == -1 ? DefaultPort() : port;

    //
    // build hash key:
    //
    // the hash key uniquely identifies the connection type.  two connections
    // are "equal" if they end up talking the same protocol to the same server
    // and are both used for anonymous or non-anonymous connection only;
    // anonymity of the connection is setup later from nsHttpChannel::AsyncOpen
    // where we know we use anonymous connection (LOAD_ANONYMOUS load flag)
    //

    const char *keyHost;
    int32_t keyPort;

    if (mUsingHttpProxy && !mUsingConnect) {
        keyHost = ProxyHost();
        keyPort = ProxyPort();
    } else {
        keyHost = Host();
        keyPort = Port();
    }

    // The hashkey has 4 fields followed by host connection info
    // byte 0 is P/T/. {P,T} for Plaintext/TLS Proxy over HTTP
    // byte 1 is S/. S is for end to end ssl such as https:// uris
    // byte 2 is A/. A is for an anonymous channel (no cookies, etc..)
    // byte 3 is P/. P is for a private browising channel
    // byte 4 is R/. R is for 'relaxed' unauthed TLS for http:// uris
    // byte 5 is X/. X is for disallow_spdy flag

    mHashKey.AssignLiteral("......");
    mHashKey.Append(keyHost);
    mHashKey.Append(':');
    mHashKey.AppendInt(keyPort);
    if (!mUsername.IsEmpty()) {
        mHashKey.Append('[');
        mHashKey.Append(mUsername);
        mHashKey.Append(']');
    }

    if (mUsingHttpsProxy) {
        mHashKey.SetCharAt('T', 0);
    } else if (mUsingHttpProxy) {
        mHashKey.SetCharAt('P', 0);
    }
    if (mEndToEndSSL) {
        mHashKey.SetCharAt('S', 1);
    }

    // NOTE: for transparent proxies (e.g., SOCKS) we need to encode the proxy
    // info in the hash key (this ensures that we will continue to speak the
    // right protocol even if our proxy preferences change).
    //
    // NOTE: for SSL tunnels add the proxy information to the cache key.
    // We cannot use the proxy as the host parameter (as we do for non SSL)
    // because this is a single host tunnel, but we need to include the proxy
    // information so that a change in proxy config will mean this connection
    // is not reused

    if ((!mUsingHttpProxy && ProxyHost()) ||
        (mUsingHttpProxy && mUsingConnect)) {
        mHashKey.AppendLiteral(" (");
        mHashKey.Append(ProxyType());
        mHashKey.Append(':');
        mHashKey.Append(ProxyHost());
        mHashKey.Append(':');
        mHashKey.AppendInt(ProxyPort());
        mHashKey.Append(')');
    }

    if(!mAuthenticationHost.IsEmpty()) {
        mHashKey.AppendLiteral(" <TLS-LOGIC ");
        mHashKey.Append(mAuthenticationHost);
        mHashKey.Append(':');
        mHashKey.AppendInt(mAuthenticationPort);
        mHashKey.Append('>');
    }

    if (!mNPNToken.IsEmpty()) {
        mHashKey.AppendLiteral(" {NPN-TOKEN ");
        mHashKey.Append(mNPNToken);
        mHashKey.AppendLiteral("}");
    }
}

nsHttpConnectionInfo*
nsHttpConnectionInfo::Clone() const
{
    nsHttpConnectionInfo *clone;
    if (mAuthenticationHost.IsEmpty()) {
        clone = new nsHttpConnectionInfo(mHost, mPort, mNPNToken, mUsername, mProxyInfo, mEndToEndSSL);
    } else {
        MOZ_ASSERT(mEndToEndSSL);
        clone = new nsHttpConnectionInfo(mHost, mPort, mNPNToken, mUsername, mProxyInfo,
                                         mAuthenticationHost,
                                         mAuthenticationPort);
    }

    // Make sure the anonymous, relaxed, and private flags are transferred
    clone->SetAnonymous(GetAnonymous());
    clone->SetPrivate(GetPrivate());
    clone->SetRelaxed(GetRelaxed());
    clone->SetNoSpdy(GetNoSpdy());
    MOZ_ASSERT(clone->Equals(this));

    return clone;
}

void
nsHttpConnectionInfo::CloneAsDirectRoute(nsHttpConnectionInfo **outCI)
{
    if (mAuthenticationHost.IsEmpty()) {
        *outCI = Clone();
        return;
    }

    nsRefPtr<nsHttpConnectionInfo> clone =
        new nsHttpConnectionInfo(mAuthenticationHost, mAuthenticationPort,
                                 EmptyCString(), mUsername, mProxyInfo, mEndToEndSSL);
    // Make sure the anonymous, relaxed, and private flags are transferred
    clone->SetAnonymous(GetAnonymous());
    clone->SetPrivate(GetPrivate());
    clone->SetRelaxed(GetRelaxed());
    clone->SetNoSpdy(GetNoSpdy());
    clone.forget(outCI);
}

nsresult
nsHttpConnectionInfo::CreateWildCard(nsHttpConnectionInfo **outParam)
{
    // T???mozilla.org:443 (https:proxy.ducksong.com:3128) [specifc form]
    // TS??*:0 (https:proxy.ducksong.com:3128)   [wildcard form]

    if (!mUsingHttpsProxy) {
        MOZ_ASSERT(false);
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    nsRefPtr<nsHttpConnectionInfo> clone;
    clone = new nsHttpConnectionInfo(NS_LITERAL_CSTRING("*"), 0,
                                     mNPNToken, mUsername, mProxyInfo, true);
    // Make sure the anonymous and private flags are transferred!
    clone->SetAnonymous(GetAnonymous());
    clone->SetPrivate(GetPrivate());
    clone.forget(outParam);
    return NS_OK;
}

bool
nsHttpConnectionInfo::UsingProxy()
{
    if (!mProxyInfo)
        return false;
    return !mProxyInfo->IsDirect();
}

bool
nsHttpConnectionInfo::HostIsLocalIPLiteral() const
{
    PRNetAddr prAddr;
    // If the host/proxy host is not an IP address literal, return false.
    if (ProxyHost()) {
        if (PR_StringToNetAddr(ProxyHost(), &prAddr) != PR_SUCCESS) {
          return false;
        }
    } else if (PR_StringToNetAddr(Host(), &prAddr) != PR_SUCCESS) {
        return false;
    }
    NetAddr netAddr;
    PRNetAddrToNetAddr(&prAddr, &netAddr);
    return IsIPAddrLocal(&netAddr);
}

} // namespace mozilla::net
} // namespace mozilla
