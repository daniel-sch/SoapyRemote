// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyServer.hpp"
#include <iostream>

static void *serverHandlerLoop(void *p)
{
    SoapyServerThreadData *data = (SoapyServerThreadData *)p;
    SoapyClientHandler handler(*data->client);

    try
    {
        while (handler.handleOnce()){}
    }
    catch (const std::exception &ex)
    {
        std::cerr << "SoapyServerListener::handlerLoop() " << ex.what() << std::endl;
    }

    data->done = true;
    return p;
}

SoapyServerListener::SoapyServerListener(SoapyRPCSocket &sock):
    _sock(sock),
    _handlerId(0)
{
    return;
}

SoapyServerListener::~SoapyServerListener(void)
{
    return;
}

void SoapyServerListener::handleOnce(void)
{
    //cleanup completed threads
    std::map<size_t, SoapyServerThreadData>::iterator it = _handlers.begin();
    while (it != _handlers.end())
    {
        SoapyServerThreadData &data = it->second;
        if (not data.done)
        {
            ++it;
            continue;
        }
        std::cout << "SoapyServerListener::~handler()" << std::endl;
        delete data.client;
        int ret = pthread_join(data.thread, NULL);
        if (ret != 0)
        {
            std::cerr << "SoapyServerListener::pthread_join() " << ret << std::endl;
        }
        _handlers.erase(it++);
    }

    //wait with timeout for the server socket to become ready to accept
    if (not _sock.selectRecv(SOAPY_REMOTE_ACCEPT_TIMEOUT_US)) return;

    std::cout << "SoapyServerListener::handler()" << std::endl;
    SoapyRPCSocket *client = _sock.accept();
    if (client == NULL)
    {
        std::cerr << "SoapyServerListener::accept() " << _sock.lastErrorMsg() << std::endl;
        return;
    }

    //setup the thread data
    SoapyServerThreadData &data = _handlers[_handlerId++];
    data.done = false;
    data.client = client;

    //spawn a new thread
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    int ret = pthread_create(&data.thread, &attr, &serverHandlerLoop, &data);
    pthread_attr_destroy(&attr);
    if (ret != 0)
    {
        std::cerr << "SoapyServerListener::pthread_create() " << ret << std::endl;
        data.done = true;
        return;
    }
}