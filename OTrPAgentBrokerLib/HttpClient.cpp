/* Copyright (c) Microsoft Corporation.  All Rights Reserved. */
#include <Windows.h>
#include <wininet.h>
#include <assert.h>
#include <string.h>
extern "C" {
#include "OTrPSession.h"
#include "HttpHelper.h"
#include "HttpClient.h"
#include "../OTrPAgentBrokerLib/OTrPAgent_u.h"
#include "../OTrPAgentBrokerLib/OTrPAgentBrokerLib.h"
};

#define OTRP_JSON_MEDIA_TYPE "application/otrp+json"

OTrPSession g_Session = { 0 };

// Send an empty POST to the indicated URI.
int ocall_Connect(const char* tamUri)
{
    char authority[266];
    char hostName[256];
    char path[256];
    int statusCode;
    char* responseBuffer;
    URL_COMPONENTS components = { sizeof(components) };

    // Get authority and path from URI.
    components.dwHostNameLength = 255;
    components.lpszHostName = hostName;
    components.dwUrlPathLength = 255;
    components.lpszUrlPath = path;
    if (!InternetCrackUrl(tamUri, 0, 0, &components)) {
        return GetLastError();
    }
    sprintf_s(authority, sizeof(authority), "%s:%d", components.lpszHostName, components.nPort);

    // Create session state.
    OTrPSession* session = &g_Session;
    strcpy_s(session->TamUri, tamUri);

    int err = MakeHttpCall(
        "POST",
        authority,
        path,
        nullptr,
        nullptr,
        OTRP_JSON_MEDIA_TYPE,
        &statusCode,
        &responseBuffer);
    if (err != 0) {
        return err;
    }
    if (statusCode != 200) {
        return statusCode;
    }

    assert(session->InboundMessage == nullptr);
    session->InboundMessage = responseBuffer;

    return 0;
}

int ocall_QueueOutboundOTrPMessage(void* sessionHandle, const char* message)
{
    OTrPSession* session = (OTrPSession*)sessionHandle;

    size_t messageLength = strlen(message);
    assert(session->OutboundMessage == nullptr);

    // Save message for later transmission after the ECALL returns.
    session->OutboundMessage = _strdup(message);
    return (session->OutboundMessage == nullptr);
}

// The caller is responsible for freeing the returned buffer if non-null.
const char* SendOTrPMessage(OTrPSession* session)
{
    char authority[266];
    char hostName[256];
    char path[256];
    int statusCode;
    char* responseBuffer;
    URL_COMPONENTS components = { sizeof(components) };
    const char* extraHeaders = "Content-type: " OTRP_JSON_MEDIA_TYPE "\r\n";

    // Get authority and path from URI.
    components.dwHostNameLength = 255;
    components.lpszHostName = hostName;
    components.dwUrlPathLength = 255;
    components.lpszUrlPath = path;
    if (!InternetCrackUrl(session->TamUri, 0, 0, &components)) {
        return nullptr;
    }
    sprintf_s(authority, sizeof(authority), "%s:%d", components.lpszHostName, components.nPort);

    int err = MakeHttpCall(
        "POST",
        authority,
        path,
        extraHeaders,
        session->OutboundMessage,
        OTRP_JSON_MEDIA_TYPE,
        &statusCode,
        &responseBuffer);

    if (session->OutboundMessage != nullptr) {
        free((char*)session->OutboundMessage);
        session->OutboundMessage = nullptr;
    }

    if (err != 0) {
        return nullptr;
    }
    if (statusCode != 200) {
        return nullptr;
    }
    return responseBuffer;
}
