#include "cc/neolux/utils/unitransmit/Serial.h"

#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace cc::neolux::utils::unitransmit {

#ifdef _WIN32
SerialTransport::SerialTransport(const UrlParts &parts, const Options &opts) : opts_(opts) {
    std::string path = parts.path;
    if (path.empty()) {
        path = parts.host;
    }
    if (path.empty()) {
        return;
    }
    handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        handle_ = nullptr;
        return;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        close_handle();
        return;
    }
    dcb.BaudRate = static_cast<DWORD>(opts_.baud);
    dcb.ByteSize = static_cast<BYTE>(opts_.data_bits);
    dcb.Parity = NOPARITY;
    if (opts_.parity == 'e') {
        dcb.Parity = EVENPARITY;
    } else if (opts_.parity == 'o') {
        dcb.Parity = ODDPARITY;
    }
    dcb.StopBits = (opts_.stop_bits == 2) ? TWOSTOPBITS : ONESTOPBIT;
    if (!SetCommState(handle_, &dcb)) {
        close_handle();
        return;
    }

    COMMTIMEOUTS timeouts{};
    if (opts_.blocking) {
        timeouts.ReadIntervalTimeout = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant =
            (opts_.timeout_ms >= 0) ? static_cast<DWORD>(opts_.timeout_ms) : 0;
    } else {
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
    }
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;
    SetCommTimeouts(handle_, &timeouts);
}

SerialTransport::~SerialTransport() { close_handle(); }

void SerialTransport::close_handle() {
    if (handle_) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
}

std::size_t SerialTransport::in_waiting() const {
    if (!handle_) {
        return 0;
    }
    COMSTAT stat{};
    DWORD errors = 0;
    if (!ClearCommError(static_cast<HANDLE>(handle_), &errors, &stat)) {
        return 0;
    }
    return static_cast<std::size_t>(stat.cbInQue);
}

std::vector<std::uint8_t> SerialTransport::read() { return read(1); }

std::vector<std::uint8_t> SerialTransport::read_all() {
    std::size_t available = in_waiting();
    if (available == 0 && opts_.blocking) {
        return read(1);
    }
    return read(available);
}

std::vector<std::uint8_t> SerialTransport::read(std::size_t max_bytes) {
    if (!handle_ || max_bytes == 0) {
        return {};
    }
    std::vector<std::uint8_t> buffer(max_bytes);
    DWORD bytes_read = 0;
    if (!ReadFile(static_cast<HANDLE>(handle_), buffer.data(), static_cast<DWORD>(max_bytes),
                  &bytes_read, nullptr)) {
        return {};
    }
    buffer.resize(static_cast<std::size_t>(bytes_read));
    return buffer;
}

std::size_t SerialTransport::write(const std::uint8_t *data, std::size_t size) {
    if (!handle_ || !data || size == 0) {
        return 0;
    }
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(handle_), data, static_cast<DWORD>(size), &written, nullptr)) {
        return 0;
    }
    return static_cast<std::size_t>(written);
}
#else
SerialTransport::SerialTransport(const UrlParts &parts, const Options &opts) : opts_(opts) {
    std::string path = parts.path;
    if (path.empty()) {
        path = parts.host;
    }
    if (path.empty()) {
        return;
    }
    fd_ = open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) {
        fd_ = -1;
        return;
    }

    termios tio{};
    if (tcgetattr(fd_, &tio) != 0) {
        close(fd_);
        fd_ = -1;
        return;
    }

    speed_t speed = B115200;
    switch (opts_.baud) {
        case 9600:
            speed = B9600;
            break;
        case 19200:
            speed = B19200;
            break;
        case 38400:
            speed = B38400;
            break;
        case 57600:
            speed = B57600;
            break;
        case 115200:
        default:
            speed = B115200;
            break;
    }
    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);

    tio.c_cflag &= ~CSIZE;
    switch (opts_.data_bits) {
        case 7:
            tio.c_cflag |= CS7;
            break;
        case 8:
        default:
            tio.c_cflag |= CS8;
            break;
    }

    if (opts_.parity == 'e') {
        tio.c_cflag |= PARENB;
        tio.c_cflag &= ~PARODD;
    } else if (opts_.parity == 'o') {
        tio.c_cflag |= PARENB;
        tio.c_cflag |= PARODD;
    } else {
        tio.c_cflag &= ~PARENB;
    }

    if (opts_.stop_bits == 2) {
        tio.c_cflag |= CSTOPB;
    } else {
        tio.c_cflag &= ~CSTOPB;
    }

    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;

    tcsetattr(fd_, TCSANOW, &tio);
}

SerialTransport::~SerialTransport() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

std::size_t SerialTransport::in_waiting() const {
    if (fd_ < 0) {
        return 0;
    }
    int available = 0;
    if (ioctl(fd_, FIONREAD, &available) != 0) {
        return 0;
    }
    return static_cast<std::size_t>(available);
}

std::vector<std::uint8_t> SerialTransport::read() { return read(1); }

std::vector<std::uint8_t> SerialTransport::read_all() {
    std::size_t available = in_waiting();
    if (available == 0 && opts_.blocking) {
        if (!wait_for_fd(opts_.timeout_ms)) {
            return {};
        }
        available = in_waiting();
    }
    return read(available);
}

std::vector<std::uint8_t> SerialTransport::read(std::size_t max_bytes) {
    if (fd_ < 0 || max_bytes == 0) {
        return {};
    }
    if (opts_.blocking) {
        if (!wait_for_fd(opts_.timeout_ms)) {
            return {};
        }
    }
    std::vector<std::uint8_t> buffer(max_bytes);
    ssize_t rc = ::read(fd_, buffer.data(), max_bytes);
    if (rc <= 0) {
        return {};
    }
    buffer.resize(static_cast<std::size_t>(rc));
    return buffer;
}

std::size_t SerialTransport::write(const std::uint8_t *data, std::size_t size) {
    if (fd_ < 0 || !data || size == 0) {
        return 0;
    }
    ssize_t rc = ::write(fd_, data, size);
    if (rc <= 0) {
        return 0;
    }
    return static_cast<std::size_t>(rc);
}

bool SerialTransport::wait_for_fd(int timeout_ms) const {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(fd_, &readfds);
    timeval tv{};
    timeval *ptv = nullptr;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }
    int rc = select(fd_ + 1, &readfds, nullptr, nullptr, ptv);
    return rc > 0;
}
#endif

} // namespace cc::neolux::utils::unitransmit
