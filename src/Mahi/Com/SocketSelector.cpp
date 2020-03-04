// #include <MEL/Communications/Socket.hpp>
// #include <MEL/Communications/SocketSelector.hpp>
// #include <MEL/Logging/Log.hpp>
#include <Mahi/Util.hpp>
#include <Mahi/Com.hpp>
#include <algorithm>
#include <utility>

#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <basetsd.h>
#ifdef _WIN32_WINDOWS
#undef _WIN32_WINDOWS
#endif
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINDOWS 0x0501
#define _WIN32_WINNT 0x0501
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4127)  // "conditional expression is constant"
                                 // generated by the FD_SET macro
#endif

namespace mel {
struct SocketSelector::SocketSelectorImpl {
    fd_set allSockets;  ///< Set containing all the sockets handles
    fd_set
        socketsReady;  ///< Set containing handles of the sockets that are ready
    int maxSocket;     ///< Maximum socket handle
    int socketCount;   ///< Number of socket handles
};

SocketSelector::SocketSelector() : impl_(new SocketSelectorImpl) {
    clear();
}

SocketSelector::SocketSelector(const SocketSelector& copy)
    : impl_(new SocketSelectorImpl(*copy.impl_)) {}

SocketSelector::~SocketSelector() {
    delete impl_;
}

void SocketSelector::add(Socket& socket) {
    SocketHandle handle = socket.get_handle();
    if (handle != Socket::invalid_socket()) {
#if defined(_WIN32)
        if (impl_->socketCount >= FD_SETSIZE) {
            LOG(Error)
                << "The socket can't be added to the selector because the "
                << "selector is full. This is a limitation of your operating "
                << "system's FD_SETSIZE setting.";
            return;
        }
        if (FD_ISSET(handle, &impl_->allSockets))
            return;
        impl_->socketCount++;
#else
        if (handle >= FD_SETSIZE) {
            LOG(Error)
                << "The socket can't be added to the selector because its "
                << "ID is too high. This is a limitation of your operating "
                << "system's FD_SETSIZE setting.";
            return;
        }

        // SocketHandle is an int in POSIX
        impl_->maxSocket = std::max(impl_->maxSocket, handle);
#endif
        FD_SET(handle, &impl_->allSockets);
    }
}

void SocketSelector::remove(Socket& socket) {
    SocketHandle handle = socket.get_handle();
    if (handle != Socket::invalid_socket()) {
#if defined(_WIN32)
        if (!FD_ISSET(handle, &impl_->allSockets))
            return;
        impl_->socketCount--;
#else
        if (handle >= FD_SETSIZE)
            return;
#endif
        FD_CLR(handle, &impl_->allSockets);
        FD_CLR(handle, &impl_->socketsReady);
    }
}

void SocketSelector::clear() {
    FD_ZERO(&impl_->allSockets);
    FD_ZERO(&impl_->socketsReady);
    impl_->maxSocket   = 0;
    impl_->socketCount = 0;
}

bool SocketSelector::wait(Time timeout) {
    // Setup the timeout
    timeval time;
    time.tv_sec  = static_cast<long>(timeout.as_microseconds() / 1000000);
    time.tv_usec = static_cast<long>(timeout.as_microseconds() % 1000000);
    // Initialize the set that will contain the sockets that are ready
    impl_->socketsReady = impl_->allSockets;
    // Wait until one of the sockets is ready for reading, or timeout is reached
    // The first parameter is ignored on Windows
    int count = select(impl_->maxSocket + 1, &impl_->socketsReady, NULL, NULL,
                       timeout != Time::Zero ? &time : NULL);
    return count > 0;
}

bool SocketSelector::is_ready(Socket& socket) const {
    SocketHandle handle = socket.get_handle();
    if (handle != Socket::invalid_socket()) {
#if !defined(_WIN32)
        if (handle >= FD_SETSIZE)
            return false;
#endif
        return FD_ISSET(handle, &impl_->socketsReady) != 0;
    }
    return false;
}

SocketSelector& SocketSelector::operator=(const SocketSelector& right) {
    SocketSelector temp(right);
    std::swap(impl_, temp.impl_);
    return *this;
}

}  // namespace mel