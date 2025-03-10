#ifndef C_TFTP_SERVER_H
#define C_TFTP_SERVER_H

#include "f_core/os/c_tenant.h"
#include "f_core/net/network/c_ipv4.h"
#include "f_core/net/transport/c_udp_socket.h"

class CTftpServerTenant : public CTenant {
public:
    static constexpr uint16_t TFTP_DEFAULT_PORT = 69;
    inline static CTftpServerTenant *instance = nullptr;

    /**
     * Singleton getter to avoid multiple instances of the TFTP server.
     */
    static CTftpServerTenant *getInstance(const CIPv4 &ipv4, uint16_t port = TFTP_DEFAULT_PORT) {
        if (instance == nullptr) {
            instance = new CTftpServerTenant(ipv4, port);
        }
        return instance;
    }

    void Startup() override;
    void Cleanup() override;
    void Run() override;

private:
    // The control socket bound to port 69 (or specified port)
    CUdpSocket sock;
    CIPv4 ip; // Store to create new sockets for data transfers

    typedef enum {
        NETASCII = 0,
        OCTET, // We only support octet mode
        MAIL,
        NUM_MODES,
        UNDEFINED_TFTP_MODE = -1 // Initialization placeholder
    } TftpMode;

    typedef enum {
        RRQ = 1,   // Read request
        WRQ = 2,   // Write request
        DATA = 3,  // Data packet
        ACK = 4,   // Acknowledgement
        ERROR = 5  // Error packet
    } TftpOpcode;

    typedef enum {
        FILE_NOT_FOUND = 1,
        ACCESS_VIOLATION = 2,
        DISK_FULL = 3,
        ILLEGAL_OPERATION = 4,
        UNKNOWN_TRANSFER_ID = 5,
        FILE_ALREADY_EXISTS = 6,
        NO_SUCH_USER = 7
    } TftpErrorCodes;

    //  2 bytes    string        string
    // -- -- -- -- -- -- -- -- -- -- -- --
    // | Opcode | Filename | 0 | Mode | 0 |
    // -- -- -- -- -- -- -- -- -- -- -- --
    // Assumes file path is less than  bytes (512 - 2 (zero terminator) - 2 (opcode) - strlen(TftpMode))
    static constexpr int rwRequestPacketSize = 512;

    static constexpr const char *tftpModeStrings[NUM_MODES] = {
        "netascii",
        "octet",
        "mail"
    };

    CTftpServerTenant(const CIPv4 &ipv4, uint16_t port = TFTP_DEFAULT_PORT)
        : CTenant("TFTP server"), sock(ipv4, port, port), ip(ipv4) {};

    void handleReadRequest(const sockaddr &srcAddr, const uint8_t *packet, int len);
    int waitForAck(CUdpSocket &dataSock, const sockaddr &srcAddr, uint16_t blockNum);
    int generateTree();
};

#endif // C_TFTP_SERVER_H
