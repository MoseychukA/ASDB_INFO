#ifndef _ADSB_PACKET_HH_
#define _ADSB_PACKET_HH_

#include <cstdint>

#include "buffer_utils.hh"

// Useful resource: https://mode-s.org/decode/content/ads-b/1-basics.html

class TransponderPacket
{
public:
    static const uint16_t kMaxPacketLenWords32 = 4;
    static const uint16_t kDFNUmBits = 5;    // [1-5] Downlink Format bitlength.
    static const uint16_t kMaxDFStrLen = 50; // Max length of TypeCode string.
    static const uint16_t kDebugStrLen = 200;
    static const uint16_t kSquitterPacketNumBits = 56;
    static const uint16_t kSquitterPacketNumWords32 = 2; // 56 bits = 1.75 words, round up to 2.
    static const uint16_t kExtendedSquitterPacketLenBits = 112;
    static const uint16_t kExtendedSquitterPacketNumWords32 = 4; // 112 bits = 3.5 words, round up to 4.

    // Bits 1-5: Downlink Format (DF)
    enum DownlinkFormat
    {
        kDownlinkFormatInvalid = -1,
        // DF 0-11 = short messages (56 bits)
        kDownlinkFormatShortRangeAirSurveillance = 0,
        kDownlinkFormatAltitudeReply = 4,
        kDownlinkFormatIdentityReply = 5,
        kDownlinkFormatAllCallReply = 11,
        // DF 16-24 = long messages (112 bits)
        kDownlinkFormatLongRangeAirSurveillance = 16,
        kDownlinkFormatExtendedSquitter = 17,
        kDownlinkFormatExtendedSquitterNonTransponder = 18,
        kDownlinkFormatMilitaryExtendedSquitter = 19,
        kDownlinkFormatCommBAltitudeReply = 20,
        kDownlinkFormatCommBIdentityReply = 21,
        kDownlinkFormatCommDExtendedLengthMessage = 24
        // DF 1-3, 6-10, 11-15, 22-23 not used
    };

    // Constructors
    /**
     * TransponderPacket constructor.
     * @param[in] rx_buffer Buffer to read from. Must be packed such that all 32 bits of each word are filled, with each
     * word left (MSb) aligned such that the total number of bits is 112. Words must be big-endian, with the MSb of the
     * first word being the oldest bit.
     * @param[in] rx_buffer_len_words32 Number of 32-bit words to read from the rx_buffer.
     * @param[in] rssi_dbm RSSI of the packet that was received, in dBm. Defaults to INT32_MIN if not set.
     */
    TransponderPacket(uint32_t rx_buffer[kMaxPacketLenWords32], uint16_t rx_buffer_len, int rssi_dbm = INT32_MIN);

    /**
     * TransponderPacket constructor from string.
     * @param[in] rx_string String of nibbles as hex characters. Big-endian, MSB (oldest byte) first.
     * @param[in] rssi_dbm RSSI of the packet that was received, in dBm. Defaults to INT32_MIN if not set.
     */
    TransponderPacket(char *rx_string, int rssi_dbm = INT32_MIN);

    /**
     * Default constructor.
     */
    TransponderPacket() { debug_string[0] = '\0'; };

    bool IsValid() const { return is_valid_; };

    int GetRSSIDBm() { return rssi_dbm_; }
    uint16_t GetDownlinkFormat() const { return downlink_format_; };
    uint16_t GetDownlinkFormatString(char str_buf[kMaxDFStrLen]) const;
    DownlinkFormat GetDownlinkFormatEnum();
    uint32_t GetICAOAddress() const { return icao_address_; };
    uint16_t GetPacketBufferLenBits() const { return packet_buffer_len_bits_; };

    /**
     * Dumps the internal packet buffer to a destination and returns the number of bytes written.
     * @param[in] to_buffer Destination buffer, must be of length kMaxPacketLenWords32 or larger.
     * @retval Number of bytes written to destination buffer.
     */
    uint16_t DumpPacketBuffer(uint32_t to_buffer[kMaxPacketLenWords32]) const;

    // Exposed for testing only.
    uint32_t Get24BitWordFromPacketBuffer(uint16_t first_bit_index) const
    {
        return get_n_bit_word_from_buffer(24, first_bit_index, packet_buffer_);
    };

    /**
     * Calculates the 24-bit CRC checksum of the ADS-B packet and returns the checksum value. The returned
     * value should match the last 24-bits in the 112-bit ADS-B packet if the packet is valid.
     * @retval CRC checksum.
     */
    uint32_t CalculateCRC24(uint16_t packet_len_bits = kExtendedSquitterPacketLenBits) const;

    char debug_string[kDebugStrLen] = "";

protected:
    bool is_valid_ = false;
    uint32_t packet_buffer_[kMaxPacketLenWords32];
    uint16_t packet_buffer_len_bits_ = 0;

    uint32_t icao_address_ = 0;
    uint16_t downlink_format_ = static_cast<uint16_t>(kDownlinkFormatInvalid);
    int rssi_dbm_ = INT32_MIN;

    uint32_t parity_interrogator_id = 0;

private:
    void ConstructTransponderPacket();
};

class ADSBPacket : public TransponderPacket
{
public:
    static const uint16_t kMaxTCStrLen = 50;

    // Bitlengths of each field in the ADS-B frame. See Table 3.1 in The 1090MHz Riddle (Junzi Sun) pg. 35.
    static const uint16_t kCANumBits = 3;    // [6-8] Capability bitlength.
    static const uint16_t kICAONumBits = 24; // [9-32] ICAO Address bitlength.
    static const uint16_t kMENumBits = 56;   // [33-88] Extended Squitter Message bitlength.
    static const uint16_t kTCNumBits = 5;    // [33-37] Type code bitlength. Not always included.
    static const uint16_t kPINumBits = 24;   // Parity / Interrogator ID bitlength.

    static const uint16_t kMEFirstBitIndex = kDFNUmBits + kCANumBits + kICAONumBits;

    /**
     * Constructor. Can only create an ADSBPacket from an existing TransponderPacket, which is is referenced as the
     * parent of the ADSBPacket. Think of this as a way to use the ADSBPacket as a "window" into the contents of the
     * parent TransponderPacket. The ADSBPacket cannot exist without the parent TransponderPacket!
     */
    ADSBPacket(const TransponderPacket &packet) : TransponderPacket(packet) { ConstructADSBPacket(); };

    // Bits 6-8 [3]: Capability (CA)
    // Bits 9-32 [24]: ICAO Aircraft Address (ICAO)
    // Bits 33-88 [56]: Message, Extended Squitter (ME)
    // (Bits 33-37 [5]): Type code (TC)
    enum TypeCode
    {
        kTypeCodeInvalid = 0,
        kTypeCodeAircraftID = 1,                // 1–4	Aircraft identification
        kTypeCodeSurfacePosition = 5,           // 5–8	Surface position
        kTypeCodeAirbornePositionBaroAlt = 9,   // 9–18	Airborne position (w/Baro Altitude)
        kTypeCodeAirborneVelocities = 19,       // 19	Airborne velocities
        kTypeCodeAirbornePositionGNSSAlt = 20,  // 20–22	Airborne position (w/GNSS Height)
        kTypeCodeReserved = 23,                 // 23–27	Reserved
        kTypeCodeAircraftStatus = 28,           // 28	Aircraft status
        kTypeCodeTargetStateAndStatusInfo = 29, // 29	Target state and status information
        kTypeCodeAircraftOperationStatus = 31   // 31	Aircraft operation status
    };
    // Bits 89-112 [24]: Parity / Interrogator ID (PI)

    // Subtype enums used for specific packet types (not instantiated as part of the ADSBPacket class).
    enum AirborneVelocitiesSubtype
    {
        kAirborneVelocitiesGroundSpeedSubsonic = 1,
        kAirborneVelocitiesGroundSpeedSupersonic = 2,
        kAirborneVelocitiesAirspeedSubsonic = 3,
        kAirborneVelocitiesAirspeedSupersonic = 4
    };

    uint16_t GetCapability() const { return capability_; };
    uint16_t GetTypeCode() const { return typecode_; };
    TypeCode GetTypeCodeEnum() const;

    // Exposed for testing only.
    uint32_t GetNBitWordFromMessage(uint16_t n, uint16_t first_bit_index) const
    {
        return get_n_bit_word_from_buffer(n, kMEFirstBitIndex + first_bit_index, packet_buffer_);
    };

private:
    uint16_t capability_ = 0;

    uint16_t typecode_ = static_cast<uint16_t>(kTypeCodeInvalid);

    void ConstructADSBPacket();
};

#endif /* _ADSB_PACKET_HH_ */